#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct file_page {
	vm_initializer *init;
	enum vm_type type;
	struct file* file; /* 가상주소와 맵핑된 파일 */

	/* Initiate the struct page and maps the pa to the va */
	bool (*file_backed_initializer) (struct page *, enum vm_type, void *kva);
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void do_munmap (void *va);
#endif
