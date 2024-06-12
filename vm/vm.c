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

bool hash_less_func_impl (const struct hash_elem *a_, const struct hash_elem *b_, void *aux){
	struct page *a = hash_entry(a_, struct page, h_elem);
	struct page *b = hash_entry(b_, struct page, h_elem);
	return a->va < b->va;
}

/* Performs some operation on hash element E, given auxiliary
 * data AUX. */
void hash_action_func_impl (struct hash_elem *e, void *aux){
	struct page *p = hash_entry(e, struct page, h_elem);
	// list_remove(&p->frame->f_elem);
	destroy(p);
	free(p);
}

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
	list_init(&frame_table);
	lock_init(&frame_lock);
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

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	// 주소 검증은 필요 없을까?
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		struct page *newpage = malloc(sizeof(struct page));

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
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */
	/* only manages frames for user pages (PAL_USER) */

	/* Project 3 - Swap DISK (CLOCK algorithm) */
	/* Q1: Clock 알고리즘에서 시작하는 주소는 항상 프레임 테이블의 처음이어도 될까? */
	/* Q2: 전역 변수로 위치를 저장해둔다면 어떻게 해야되나? */
	/* Q3: No need to frame_lock? */
	lock_acquire(&frame_lock);
	struct list_elem *e = list_begin(&frame_table);
	while (e)
	{
		struct frame *cur_frame = list_entry(e, struct frame, f_elem);
		uint64_t *cur_pml4 = thread_current()->pml4;
		const void* upage = cur_frame->page->va;

		/* if kernel address */
		if (pml4_is_accessed(cur_pml4, cur_frame->page->va))
			pml4_set_accessed(cur_pml4, cur_frame->page->va, false);
		else 
		{
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
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	list_remove(&victim->f_elem);
	if (victim != NULL) {
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
    frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);

    if (frame->kva == NULL) {
        frame = vm_evict_frame();
        frame->page = NULL;

        return frame;
    }
	
    frame->page = NULL;

    return frame;
}

/* Growing the stack. */
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
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,			// <= ???
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
// #define DEBUG
#ifdef DEBUG
printf("PF Stat:: usr: %d, wr: %d, np: %d, addr: %lld, rsp: %lld, f-rsp: %lld \n", user, write, not_present, addr, thread_current()->rsp, f->rsp);
#endif
	/* TODO: Validate the fault */
	if (addr == NULL || is_kernel_vaddr(addr)) 
		return false;

	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = spt_find_page(spt, pg_round_down(addr));
	uint64_t rsp = user ? f->rsp : thread_current()->rsp;
	// 유저 모드일 경우 Intr_frame의 rsp를 가리켜야 한다.

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
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {

	// TODO: src부터 dst까지 spt를 복사
	struct hash_iterator h_iter;
    hash_first(&h_iter, &src->hash_brown);

    while (hash_next(&h_iter)) {
        // hash_cur: 현재 elem을 리턴하거나, table의 끝인 null 포인터를 반환하거나
        struct page *src_page = hash_entry(hash_cur(&h_iter), struct page, h_elem);

        enum vm_type type = src_page->operations->type;
        void *upage = src_page->va;
        bool writable = src_page->writable;

        // UNINIT: 
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
		// Defaults
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