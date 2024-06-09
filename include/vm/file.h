#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct file_page {
	struct file* file; // 읽을거
	size_t offset; // 읽기 시작지점 (A.K.A 읽은 바이트)
	size_t read_bytes; // 읽을 바이트
	size_t zero_bytes; // 0으로 채울 바이트
	/* is writable */
	bool writalbe;
	bool has_next;
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void do_munmap (void *va);
#endif
