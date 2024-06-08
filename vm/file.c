/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "string.h"

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
}

static bool
lazy_load_segment_by_file (struct page *page, void *aux) {
	/* 파일로부터 세그먼트를 로드한다.
		주소 VA 에서 첫 번째 페이지 폴트 발생 시 이 함수가 호출된다.
		이 함수를 호출할 때 VA(가상 주소)를 사용할 수 있다. */
	struct file_page *info = (struct file_page*)aux;

	struct file *file = info->file;
	
	size_t offset = info->offset;
	size_t read_bytes = info->read_bytes;
	size_t zero_bytes = info->zero_bytes;
	
	// read_at으로 하니 필요 없을 듯
	// file_seek (file, offset); 

	/* Do calculate how to fill this page.
	 * We will read PAGE_READ_BYTES bytes from FILE
	 * and zero the final PAGE_ZERO_BYTES bytes. */
	size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
	size_t page_zero_bytes = PGSIZE - page_read_bytes;

	if (page->frame->kva == NULL)
		return false;

	off_t read_byte = 0;
	read_byte = file_read_at(file, page->va, page_read_bytes ,offset);
	if(read_byte != page_read_bytes) return false;

	// 읽어야 할 길이가 PGSIZE의 배수가 아닌 경우
	// stick out 조치
	if(read_byte%PGSIZE > 0)
		memset(page->va+read_byte, 0, read_byte%PGSIZE);

	return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	
	size_t temp_length = length < PGSIZE ? length : PGSIZE;
	size_t temp_zero_length = PGSIZE - temp_length;

	if((temp_length + temp_zero_length) % PGSIZE == 0);
	if(offset % PGSIZE == 0);
	
	while (temp_length > 0 || temp_zero_length > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = page_read_bytes < PGSIZE ? page_read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		
		struct file_page *aux = malloc(sizeof(struct file_page));
		if (aux == NULL)
			return NULL;
		
		aux->file = file;
		aux->offset = offset;
		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;
		aux->writalbe = writable;

		if( !vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment_by_file, aux) ){	
			free(aux);
			return NULL;
		}
		
		/* Advance. */
		temp_length -= page_read_bytes;
		temp_zero_length -= page_zero_bytes;
		addr += PGSIZE;
		/*
			파일에서 데이터를 읽어올 때 파일 오프셋을 적절히 이동시키기 위해서이다.
			load_segment 함수는 파일의 특정 오프셋부터 시작하여 세그먼트를 로드한다. 
			이때 세그먼트의 크기가 페이지 크기보다 클 경우, 여러 페이지에 걸쳐서 세그먼트를 로드해야 한다.
			각 반복마다 page_read_bytes 만큼의 데이터를 파일에서 읽어와 페이지에 로드하고,
			이 때 파일 오프셋 ofs를 page_read_bytes 만큼 증가시켜야 다음 페이지를 로드할 때 파일의 올바른 위치에서 데이터를 읽어올 수 있다.
		*/
		offset += page_read_bytes;
	}
	return addr;
}



/* Do the munmap */
void
do_munmap (void *addr) {

}
