/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
/* Project 3 */
#include "threads/mmu.h"
#include "vm/uninit.c"
#include "vm/file.c"
#include "vm/anon.c"

/* Project 3 */
uint64_t hash_hash_func_impl(const struct hash_elem *e, void *aux){
    // elem의 필드를 사용하여 해시 값을 계산하여 반환
	const struct page *p = hash_entry(e, struct page, h_elem);
	return hash_bytes(&p->va, sizeof p->va);
}
bool hash_less_func_impl (const struct hash_elem *a_, const struct hash_elem *b_, void *aux){
	const struct page *a = hash_entry(a_, struct page, h_elem);
	const struct page *b = hash_entry(b_, struct page, h_elem);
	return a->va < b->va;
}

/* Performs some operation on hash element E, given auxiliary
 * data AUX. */
void hash_action_func_impl (struct hash_elem *e, void *aux){
	struct page *p = hash_entry(e, struct page, h_elem);
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
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* upage가 이미 사용 중인지 여부를 확인합니다. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: 페이지를 생성하고 VM 유형에 따라 초기화기를 가져오고,
		 * TODO: uninit_new를 호출하여 "uninit" 페이지 구조체를 만듭니다.
		 * TODO: uninit_new를 호출한 후에 필드를 수정해야 합니다. */
		upage = (struct page *)palloc_get_page(PAL_USER || PAL_ZERO);
		
		switch (type)
		{
		case 0: 
			/* code VM_UNINIT */
			uninit_initialize(upage, aux);
			break;
		case 1:
			/* code ANON */
			anon_initializer((struct page *)upage, type, ((struct page *)upage)->va); // 세번째 인자 잘 모르겠음
			break;
		case 2:
			/* code FILE */
			file_backed_initializer((struct page *)upage, type, ((struct page *)upage)->va); // 세번째 인자 잘 모르겠음
			break;
		case 3:
			/* code CACHE */
			// Project 4에서 쓸거니까 평생볼일없음 퉤퉤
			break;
		}

		// page_get_type(upage);
		uninit_new(upage, &upage, init, type, aux, false);
		/* TODO: 페이지를 spt에 삽입합니다. */
		// 맞는지 모르겠음
		hash_insert(&spt->hash_brown, &((struct page *) upage)->h_elem);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	/* Project 3 */
	page = pml4_get_page(&thread_current()->pml4 ,va);
	if (page == NULL) return page;

	struct hash_elem *e = hash_find(&spt->hash_brown, &page->h_elem);
	if(e != NULL)
		return page;
	else 
		return NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	/* Project 3 */
	if ( hash_find(&spt->hash_brown, &page->h_elem) == NULL ){
		hash_insert(&spt->hash_brown, &page->h_elem);
		succ = true;
	}
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
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	frame = palloc_get_page(PAL_USER || PAL_ZERO);

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
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = palloc_get_page(PAL_USER || PAL_ZERO);
	
	uint64_t *pml4 = thread_current()->pml4;
	
	pml4_set_page(pml4,page, va, 1);

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	uint64_t *pml4 = thread_current()->pml4;
	uint64_t *pte = pml4e_walk (pml4, (uint64_t) page, 1);
	// pte


	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	/* Project 3 */
	hash_init(&spt->hash_brown, hash_hash_func_impl, hash_less_func_impl, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED, struct supplemental_page_table *src UNUSED) {
	/* Project 3 */

}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	/* Project 3 */
	hash_destroy(&spt->hash_brown, hash_action_func_impl);
}
