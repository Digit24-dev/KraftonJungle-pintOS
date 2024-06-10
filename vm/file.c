/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "string.h"
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	
	struct file_page *info = (struct file_page*)page->uninit.aux;
	file_page->file = info->file;
	file_page->offset = info->offset;
	file_page->read_bytes = info->read_bytes;
	file_page->zero_bytes = info->zero_bytes;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	struct thread *t = thread_current();

	if (pml4_is_dirty(t->pml4, page->va)) {
		file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->offset);
		pml4_set_dirty(t->pml4, page->va, false);
	}
	pml4_clear_page(t->pml4, page->va);
  file_close( page->file.file );
}
// static void
// file_backed_destroy (struct page *page) {
// 	struct file_page *file_page UNUSED = &page->file;

// }

static bool
lazy_load_segment_by_file (struct page *page, void *aux) {
	
	// if (page->frame->kva == NULL)
	// 	return false;

	struct file_page *info = (struct file_page*)aux;
	struct file *file = info->file;
	
	size_t offset = info->offset;
	size_t page_read_bytes = info->read_bytes;
	size_t page_zero_bytes = info->zero_bytes;
	
	file_seek (file, offset); 

	if(page->frame->kva == NULL)
		return false;

	/* Do calculate how to fill this page.
	 * We will read PAGE_READ_BYTES bytes from FILE
	 * and zero the final PAGE_ZERO_BYTES bytes. */
	if (file_read(file, page->frame->kva, page_read_bytes) != (int) page_read_bytes) {
		return false;
	}
	// off_t read_byte = 0;
	// read_byte = file_read_at(file, page->frame->kva, page_read_bytes, offset);
	// if(read_byte != page_read_bytes) 
	// 	return false;
	memset(page->frame->kva + page_read_bytes, 0, page_zero_bytes);

	return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	
	struct file *reopened_file = file_reopen(file);
	size_t temp_length = length < file_length(reopened_file) ? length : file_length(reopened_file);
	size_t temp_zero_length = PGSIZE - temp_length % PGSIZE;
	
	// if((temp_length + temp_zero_length) % PGSIZE != 0) return NULL;
	// if(offset % PGSIZE != 0) return NULL;

	file_seek(reopened_file, offset);

	while (temp_length > 0 || temp_zero_length > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = temp_length < PGSIZE ? temp_length : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		
		struct file_page *aux = malloc(sizeof(struct file_page));
		if (aux == NULL)
			return NULL;
		
		aux->file = reopened_file;
		aux->offset = offset;
		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;
		aux->has_next = temp_length > PGSIZE;

		if( !vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment_by_file, aux) ){	
			file_close(reopened_file);
			// free(aux);
			return NULL;
		}
		
		/* Advance. */
		temp_length -= page_read_bytes;
		temp_zero_length -= page_zero_bytes;

		addr += PGSIZE;
		offset += PGSIZE;
	}

	return addr;
}


/* Do the munmap */
void
do_munmap (void *addr) {
	struct thread *t = thread_current();
	struct page *page = spt_find_page(&t->spt, addr);
	
	if (!page)
		return;
	
	bool has_next;
	// struct file* file = page->file.file;
	do {
		has_next = page->file.has_next;
		// destroy(page);
		spt_remove_page(&t->spt, page);
		addr += PGSIZE;
	} while (has_next && (page = spt_find_page(&t->spt, addr)));
	// file_close(file);
}
