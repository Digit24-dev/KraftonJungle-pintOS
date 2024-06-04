#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

struct anon_page {
    /* Project 3 */
    /* Initiate the contets of the page */
	vm_initializer *init;
	enum vm_type type;
    bool is_writeable; // 이따가 초기화 할 것
    bool is_accessed;
    bool is_updated;
	/* Initiate the struct page and maps the pa to the va */
	// bool (*anon_initializer) (struct page *, enum vm_type, void *kva);
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
