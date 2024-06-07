/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include <hash.h>

/* Project 3 */
#include "threads/mmu.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <string.h>

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
	// lock_init(&frame_lock);
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
		struct page *newpage = (struct page *)malloc(sizeof(struct page));
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
		if (spt_insert_page(spt, newpage)) {
			return true;
		} else {
			goto err;
		}
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {

	struct page *page = (struct page *) malloc(sizeof(struct page));
	page->va = pg_round_down(va);	// va가 가리키는 가상 페이지의 시작 포인트 (오프셋이 0으로 설정된 va) 반환

	struct hash_elem *e = hash_find(&spt->hash_brown, &page->h_elem);

	return e != NULL ? hash_entry(e, struct page, h_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	if (hash_insert(&spt->hash_brown, &page->h_elem) == NULL)
		succ = true;
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */
	/* LRU or Clock ? */
	/* only manages frames for user pages (PAL_USER) */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = (struct frame*)malloc(sizeof(struct frame));
    frame->kva = palloc_get_page(PAL_USER);

    if (frame->kva == NULL) {
        frame = vm_evict_frame();
        frame->page = NULL;

        return frame;
    }
	// lock_acquire(&frame_lock);
    // list_push_back (&frame_table, &frame->f_elem);
	// lock_release(&frame_lock);
    frame->page = NULL;

    ASSERT (frame != NULL);
    ASSERT (frame->page == NULL);

    return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,			// <= ???
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {

	/* TODO: Validate the fault */
	if (addr == NULL || is_kernel_vaddr(addr)) 
		return false;
	
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;

	if (not_present) {
		page = spt_find_page(spt, addr);
		if (page == NULL) return false;
		// if (write && !page->writable) return false;
		if (write == 1 && page->writable == 0) return false;
		return vm_do_claim_page(page);
	}
	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
/*
	인자로 주어진 va에 페이지를 할당하고, 해당 페이지에 프레임을 할당한다.
	우선 한 페이지를 얻어야 하고, 그 이후에 해당 페이지를 인자로 갖는 vm_do_claim_page를 호출해야 한다.
*/
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL) return false;
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */

static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	if (frame == NULL) return false;
	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread *curr = thread_current();
    bool success = (pml4_get_page (curr->pml4, page->va) == NULL && pml4_set_page (curr->pml4, page->va, frame->kva, page->writable));

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
	
	struct hash_iterator iterator;
	hash_first(&iterator, &src->hash_brown);

	while (hash_next(&iterator))
	{	
		// 이 방식은 쓰레기다.
		// 실제 물리 메모리가 할당 되어야 할것
		struct hash_elem* hash_elem = hash_cur(&iterator);
		struct page* temp_page = hash_entry(hash_elem, struct page, h_elem);
		
		// struct page* new_page = (struct page*) malloc( sizeof(struct page) );
		// if(new_page == NULL) return false;

		// memcpy( new_page, temp_page, sizeof( struct page ) );

		// if(!vm_alloc_page(new_page->operations->type, new_page, new_page->writable)){
		// 	free(new_page);
		// 	return false;
		// }

		// if( !spt_insert_page( &dst->hash_brown, new_page ) ){
		// 	free(new_page);
		// 	return false;
		// }
		// 무엇이 다른가
		enum vm_type type = temp_page->operations->type;
		void *upage = temp_page->va;
		bool writable = temp_page->writable;

		if(VM_TYPE(type) == VM_UNINIT){
			vm_initializer * init = temp_page->uninit.init;
			void * aux = malloc(sizeof( struct lazy_load_info));
			memcpy(aux, temp_page->uninit.aux, sizeof( struct lazy_load_info));
			vm_alloc_page_with_initializer(VM_ANON, upage, writable, init, aux);
			continue;
		}

		if(!vm_alloc_page(type, upage, writable)){
			return false;
		}

		if(!vm_claim_page(upage)){
			return false;
		}

		struct page *new_page = spt_find_page(dst, upage);
		memcpy(new_page->frame->kva, temp_page->frame->kva, PGSIZE);
	}
	
	return true;
	
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->hash_brown, hash_action_func_impl);
	// hash_destroy(&spt->hash_brown, hash_action_func_impl);
}

bool delete_page(struct hash *pages, struct page *p) {
	return !hash_delete(pages, &p->h_elem)? true : false;
}