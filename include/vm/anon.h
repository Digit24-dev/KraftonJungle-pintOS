#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"

struct page;
enum vm_type;

struct anon_page {
    disk_sector_t anon_swap_sector_no;
    struct thread *thread;
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#define SECTORS_PER_PAGE (1<<12)/512

#endif
