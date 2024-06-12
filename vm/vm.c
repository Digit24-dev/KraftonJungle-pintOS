/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "string.h"

/* Project 3 */
uint64_t hash_hash_func_impl(const struct hash_elem *e, void *aux){
    // elem의 필드를 사용하여 해시 값을 계산하여 반환
	struct page *p = hash_entry(e, struct page, h_elem);
	return hash_bytes(&p->va, sizeof p->va);
}
// 해시 비교 함수
bool hash_less_func_impl (const struct hash_elem *a_, const struct hash_elem *b_, void *aux){
	struct page *a = hash_entry(a_, struct page, h_elem);
	struct page *b = hash_entry(b_, struct page, h_elem);
	return a->va < b->va;
}

/* Performs some operation on hash element E, given auxiliary
 * data AUX. */
// 해시에 들어간 페이지를 할당 해제하고 삭제
void hash_action_func_impl (struct hash_elem *e, void *aux){
	struct page *p = hash_entry(e, struct page, h_elem);
	// list_remove(&p->frame->f_elem);
	destroy(p);
	free(p);
}
/* Project 3 */

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	// 프레임 테이블과 프레임 락 초기화
	list_init(&frame_table);
	lock_init(&frame_lock);
	// 스왑 테이블 할당
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
/*
	이니셜라이저를 사용하여 pending 페이지 객체를 생성한다. 만약 페이지를 생성하고 싶다면,
	직접 생성하지 말고 이 함수 혹은 'vm_alloc_page'를 통해 생성할 것.
*/
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {
	// 다른 페이지로 변화할 uninit page를 만들어 주는것이기 때문에 type이 uninit page로 오는 요청은 에러
	ASSERT (VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current ()->spt;

	// 주소 검증은 필요 없을까?
	/* Check wheter the upage is already occupied or not. */
	// 요청한 주소가 이미 가상 메모리가(page) 할당되어 있는지 확인
	if (spt_find_page (spt, upage) == NULL) {
		// 없다면 페이지 메모리 할당 후 타입에 따라 처리한다.
		struct page *newpage = malloc(sizeof(struct page));

		// 각 페이지는 page algined 이어야 하기 때문에 pg_round_down을 이용, 페이지의 시작 주소를 넘겨준다.
		// vm type에 따라 초기화 함수를 다르게 지정해준다.
		switch (VM_TYPE(type))
		{
		case VM_ANON:
			uninit_new(newpage, pg_round_down(upage), init, type, aux, anon_initializer);
			break;

		case VM_FILE:
			uninit_new(newpage, pg_round_down(upage), init, type, aux, file_backed_initializer);
			break;

		default:
			break;
		}
		/* after uninit_new, you have to fix fields. */
		newpage->writable = writable;
		
		// 보조 페이지 테이블은 spt에 삽입한다.
		/* TODO: Insert the page into the spt. */
		if (spt_insert_page(spt, newpage)) 
			return true;
		else 
			goto err;

	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
// spt 안의 해시 테이블에서 h_elem으로 hash_elem을 뽑아내고 그걸 기반으로 hash_entry로 page 객체로 접근한다.
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = (struct page*)malloc(sizeof(struct page));
	page->va = pg_round_down(va);	// va가 가리키는 가상 페이지의 시작 포인트 (오프셋이 0으로 설정된 va) 반환

	struct hash_elem *e = hash_find(&spt->hash_brown, &page->h_elem);

	free(page);

	return e != NULL ? hash_entry(e, struct page, h_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;

	if (hash_insert(&spt->hash_brown, &page->h_elem) == NULL)
		succ = true;
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	struct hash_elem *result = hash_delete(&spt->hash_brown, &page->h_elem);
	if (result == NULL)
		return false;

	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
// disk swap시에 희생자 페이지를 정하는 알고리즘 CLOCK 알고리즘으로 짜여져 있다 LRU도 가능하다
static struct frame *
vm_get_victim (void) {

	 struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */
	/* only manages frames for user pages (PAL_USER) */

	/* Project 3 - Swap DISK (CLOCK algorithm) */
	/* Q1: Clock 알고리즘에서 시작하는 주소는 항상 프레임 테이블의 처음이어도 될까? */
	/* A1: 어디서 시작하든 상관 없을것 같음, 하지만 필요하다면 마지막에 시곗바늘이 멈춘 곳을 저장해도 좋을듯 */
	/* Q2: 전역 변수로 위치를 저장해둔다면 어떻게 해야되나? */
	/* A2: frame_table에 마지막 으로 선정한 위치 e를 저장하고 알고리즘 시작 전 리스트를 순회하여 저장한 위치와 맞는 elem을 꺼내야한다.
		   그러나 이는 매우 비효율적이므로 알고리즘 시작 전, frame table을 정렬한 후 찾는걸 추천 */
	/* Q3: No need to frame_lock? */
	/* A3: need lock 연산의 원자성을 보장해야 함. 그러지 않으면 연산 도중 잦은 접근 비트 변경으로 인해 잘못된 페이지가 
	       선택되고 이로인해 성능의 저하가 발생할 수 있음. */
	lock_acquire(&frame_lock);
	struct list_elem *e = list_begin(&frame_table);
	// 리스트에서 원소 하나를 꺼내어 알고리즘에 의해 어느 한 프레임이 선택될 때까지 순회한다.
	while (e)
	{
		struct frame *cur_frame = list_entry(e, struct frame, f_elem);
		uint64_t *cur_pml4 = thread_current()->pml4;
		const void* upage = cur_frame->page->va;

		// 클락 알고리즘은 참조된 적이 있는 프레임에 접근하면 해당 프레임의 참조 비트를 초기화하고
		/* if kernel address */
		if (pml4_is_accessed(cur_pml4, cur_frame->page->va))
			pml4_set_accessed(cur_pml4, cur_frame->page->va, false);
		else 
		{
			// 참조된 적이 없는 프레임에 도달할 때까지 순회한다.
			// 참조된 적이 없는 프레임에 도달하면 해당 프레임을 선택하고 순회를 종료한다.
			victim = cur_frame;
			break;
		}
		
		if (e == list_end(&frame_table))
		{
			e = list_begin(&frame_table);
			continue;
		}
		e = list_next(e);
	}
	lock_release(&frame_lock);
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
// vm_get_victim 함수로 설정된 희생자 프레임을 반환해주는 함수
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	// 희생자로 선택되어 곧 죽을거니까 프레임 테이블에서도 빼버린다
	list_remove(&victim->f_elem);
	// 희생자가 선택되지 않았으면 패닉에 빠진다.
	if (victim != NULL) {
		// 해당페이지 초기화시에 swap_out으로 매핑된 함수를 실행하게 되는데,
		// 스왑 아웃하는데 실패하면 NULL을 반환한다.
		// 희생자가 잔혹하게 희생되는 모습. ㅠㅠ
		if (!swap_out(victim->page))
			return NULL;

		return victim;
	}
	else {
		PANIC("PANIC!");
	}

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
/* user 풀로부터 새로운 물리 페이지를 palloc_get_page를 통해 생성한다. */
static struct frame *
vm_get_frame (void) {
	struct frame *frame = (struct frame*)malloc(sizeof(struct frame));
	// 유저 풀에서 0으로 초기화된 따끈따끈한 물리 프레임
    frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);
	// 만약 유저 풀에 자리가 없어 새 프레임을 얻을 수 없다면 
    if (frame->kva == NULL) {
		// PANIC("TODO. ");
		// 희생자를 선택한다. ( OS는 잔혹하다 )
		// 희생자 프레임을 얻은 후 해당 프레임에 기존에 연결되어있던 가상 페이지를 NULL로 초기화 한 후 반환
        frame = vm_evict_frame();
        frame->page = NULL;

        return frame;
    }
	
    frame->page = NULL;

    return frame;
}

/* Growing the stack. */
// 스택을 늘려주는 함수이다. 인자로 전달받은 주소에 대해서 가상 페이지를 할당 하고 물리 프레임을 연결한다.
// 연결 또는 할당 실패시 할당한 가상 페이지를 할당해제한다.
static void
vm_stack_growth (void *addr UNUSED) {
	if (!vm_alloc_page(VM_ANON, addr, true) && !vm_claim_page(addr)) {
		vm_dealloc_page(spt_find_page(&thread_current()->spt, addr));
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
// 페이지 폴트 발생시 호출되는 함수로 가상 메모리의 핵심 원리인 lazy load를 구현하기 위한 함수다.
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,			// <= ???
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
// #define DEBUG
#ifdef DEBUG
printf("PF Stat:: usr: %d, wr: %d, np: %d, addr: %lld, rsp: %lld, f-rsp: %lld \n", user, write, not_present, addr, thread_current()->rsp, f->rsp);
#endif
	/* TODO: Validate the fault */
	// 주소가 없거나 커널 주소여선 안된다.
	if (addr == NULL || is_kernel_vaddr(addr)) 
		return false;

	// 커널스택이란건 사실 존재하지 않는다.
	// 하지만 페이지 폴트가 어느 모드(커널, 유저)에서 발생했는지에 따라 rsp가 바뀌므로 
	// 해당 부분에 대한 처리를 한다.
	
	// ※자세한 설명 >_<※
	// 페이지 폴트는 페이지 폴트가 난 곳의 주소와, 해당 주소에 접근했던 인터럽트 프레임을 가져오는데
	// 커널 모드(설령 유저 페이지에 접근했더라도)에서의 인터럽트 프레임과 
	// 유저 모드에서의 인터럽트 프레임은 가리키는 곳이 다르다.
	// 우리는 유저 페이지에서의 rsp가 필요하기 때문에 thread 구조체에 rsp를 저장할수 있는 
	// 변수를 만들고 거기에 rsp를 저장함으로써 이를 해결하였다.
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = spt_find_page(spt, pg_round_down(addr));
	uint64_t rsp = user ? f->rsp : thread_current()->rsp;
	// 유저 모드일 경우 Intr_frame의 rsp를 가리켜야 한다.
	// 해당 주소에 실제 매핑된 물리 프레임이 존재하지 않을 경우 이면서,
	// page가 존재하지 않는다. 해당 주소가 lazy load 대기중이 아닌 처음 할당된 페이지 일경우
	// 이 경우는 anonpage인 스택에서 anonpage 밖의 주소에 접근한 경우밖에 없기 때문에 스택을 증가시켜준다.
	if (not_present) {
		if (page == NULL) {
			if (pg_round_down(addr) >= (void*)MAX_STACK_BOTTOM && addr < (void*)USER_STACK && addr >= (void*)(rsp - 8)) {
#ifdef DEBUG
printf("stack growth! \n");
#endif
				vm_stack_growth((pg_round_down(addr)));
				return addr != NULL ? true : false;
			}
			else
				return false;
		}
	}

	if (write && !page->writable)
		return false;

done:
	// 다른게 다 끝나면 lazy load의 완성을 위해 물리 프레임을 할당 해준다.
	return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
/* va에 해당하는 페이지를 가져옴 -> vm_do_claim_page 호출 */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL) 
		return false;
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
/* vm_get_frame()을 호출하여 frame에 새로운 물리 프레임 할당 */
/* 그리고 page에 frame을 매핑해준다. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	if (frame == NULL) return false;
	/* Set links */
	frame->page = page;
	page->frame = frame;
	
	// lock_acquire(&frame_lock);
    list_push_back (&frame_table, &frame->f_elem);
	// lock_release(&frame_lock);

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread *curr = thread_current();
    bool success = pml4_set_page (curr->pml4, page->va, frame->kva, page->writable);
    return success ? swap_in(page, frame->kva) : false;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->hash_brown, hash_hash_func_impl, hash_less_func_impl, NULL);
}

/* Copy supplemental page table from src to dst */
// fork 시에 페이지 전체 복사하기 위한 함수
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {

	// TODO: src부터 dst까지 spt를 복사
	struct hash_iterator h_iter;
    hash_first(&h_iter, &src->hash_brown);
	// spt src 순회하면서 다 끄집어내서 
	// uninit인 경우에 memcpy로 복사해서 spt dst 안에 다 넣어줌
	// file 인 경우에는 같은 파일을 바라보게 초기화 까지 해서 넣어줌
	// 다 끝나면 가상 페이지 할당해주고 물리 프레임 붙여주고 spt에 넣어줌
    while (hash_next(&h_iter)) {
        // hash_cur: 현재 elem을 리턴하거나, table의 끝인 null 포인터를 반환하거나
        struct page *src_page = hash_entry(hash_cur(&h_iter), struct page, h_elem);

        enum vm_type type = src_page->operations->type;
        void *upage = src_page->va;
        bool writable = src_page->writable;

        // 
        if (VM_TYPE(type) == VM_UNINIT) {
			struct lazy_load_info * temp_info = malloc( sizeof(struct lazy_load_info) );
			memcpy ( temp_info , ((struct lazy_load_info*) src_page->uninit.aux), sizeof(struct lazy_load_info));
			
			if (!vm_alloc_page_with_initializer (page_get_type(src_page), upage, src_page->writable,
				 src_page->uninit.init, (void *)temp_info)) {
					return false;
			}
        }
		// 이 분기를 타는 페이지들은 전부 fork된 file_page를 부모로 가진 
		// 생성될 uninit page들 뿐이다. 
		else if (VM_TYPE(type) == VM_FILE) { 
			struct lazy_load_info *info = malloc(sizeof(struct lazy_load_info));
			info->file = src_page->file.file;
			info->ofs = src_page->file.offset;
			info->read_bytes = src_page->file.read_bytes;
			info->zero_bytes = src_page->file.zero_bytes;
			// 자식 만들기
			if (!vm_alloc_page_with_initializer(type, upage, writable, NULL, info))
				return false;
			// 진로 강요
			struct page *dst_file_page = spt_find_page(dst, upage);
			file_backed_initializer(dst_file_page, type, NULL);
			dst_file_page->frame = src_page->frame;
			pml4_set_page(thread_current()->pml4, dst_file_page->va, src_page->frame->kva, src_page->writable);
			continue;
		}
		else {
			if (!vm_alloc_page(page_get_type(src_page), src_page->va, src_page->writable)) 
				return false;

			if (!vm_claim_page(src_page->va))
				return false;
			
			// 매핑된 프레임에 내용 로딩
			struct page *dst_page = spt_find_page(dst, upage);
			memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
		}
    }
    return true;

}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->hash_brown, hash_action_func_impl);
}

/* Project 3 */

