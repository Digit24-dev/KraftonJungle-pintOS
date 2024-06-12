#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
// #define VM
#ifdef VM
#include "vm/vm.h"
#endif
#define MAX_ARGS 25

static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);
struct thread* get_child_process(int tid);

/* General process initializer for initd and other process. */
static void
process_init (void) {
	struct thread *current = thread_current ();
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. */
tid_t
process_create_initd (const char *file_name) {
	char *fn_copy, *save_ptr;
	tid_t tid;

	/* Make a copy of FILE_NAME.
	 * Otherwise there's a race between the caller and load(). */
	fn_copy = palloc_get_page (PAL_ZERO);
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE);

	strtok_r(file_name, " ", &save_ptr);

	/* Create a new thread to execute FILE_NAME. */
	tid = thread_create (file_name, PRI_DEFAULT, initd, fn_copy);
	if (tid == TID_ERROR)
		palloc_free_page (fn_copy);
	return tid;
}

/* A thread function that launches first user process. */
static void
initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif

	process_init ();

	if (process_exec (f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED ();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. */
tid_t
process_fork (const char *name, struct intr_frame *if_ UNUSED) {
	tid_t ret;
	struct thread *cur = thread_current();
	memcpy(&cur->copied_if, if_, sizeof(struct intr_frame));

	/* Clone current thread to new thread.*/
	ret = thread_create (name,
			PRI_DEFAULT, __do_fork, cur);
	
	if (ret == TID_ERROR)
		return TID_ERROR;

	struct thread *ct = get_child_process(ret);

	sema_down(&ct->sema_load);
	
	if (ct->tid == TID_ERROR)
	{
		sema_up(&ct->sema_exit);
		list_remove(&ct->child_elem);
		return TID_ERROR;
	}

	return ret;
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. */
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;
	
	/* 1. TODO: If the parent_page is kernel page, then return immediately. */
	if (is_kernel_vaddr(va))
		return true;

	/* 2. Resolve VA from the parent's page map level 4. */
	parent_page = pml4_get_page (parent->pml4, va);
	if (parent_page == NULL) return false;

	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */
	newpage = palloc_get_page(PAL_USER);
	if (newpage == NULL)
		return false;

	/* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). */
	memcpy(newpage, parent_page, PGSIZE);
	writable = is_writable(pte);

	/* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission. */
	if (!pml4_set_page (current->pml4, va, newpage, writable)) {
		/* 6. TODO: if fail to insert page, do error handling. */
		palloc_free_page(newpage);
		return false;
	}
	return true;
}
#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. */
static void
__do_fork (void *aux) {
	struct intr_frame if_;
	struct thread *parent = (struct thread *) aux;
	struct thread *current = thread_current ();
	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
	struct intr_frame *parent_if = &parent->copied_if;
	bool succ = true;

	/* 1. Read the cpu context to local stack. */
	memcpy (&if_, parent_if, sizeof (struct intr_frame));

	/* 2. Duplicate PT */
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;

	process_activate (current);
#ifdef VM
	supplemental_page_table_init (&current->spt);
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent.*/
	for (size_t i = 2; i < MAX_FDT; i++) {
		if (parent->fdt[i] != NULL) 
			current->fdt[i] = file_duplicate(parent->fdt[i]);
		else
			current->fdt[i] = NULL;
	}
	current->nex_fd = parent->nex_fd;
	/* copy file descriptor table */

	sema_up(&current->sema_load);
	process_init ();
	if_.R.rax = 0;

	/* Finally, switch to the newly created process. */
	if (succ) {
		do_iret (&if_);
	}
error:
	sema_up(&current->sema_load);
	thread_current()->exit_code = TID_ERROR;
	thread_exit ();
	// exit(-1);
}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. */
int
process_exec (void *f_name) {
	// char *file_name = f_name;
	bool success;

	/* We cannot use the intr_frame in the thread structure.
	 * This is because when current thread rescheduled,
	 * it stores the execution information to the member. */
	struct intr_frame _if;
	_if.ds = _if.es = _if.ss = SEL_UDSEG;
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS;

	/* We first kill the current context */
	process_cleanup ();
	/* And then load the binary */
	success = load (f_name, &_if);


	// sema_up(&thread_current()->sema_load);

	/* If load failed, quit. */
	palloc_free_page (f_name);
	if (!success)
		return -1;
	
	// if (lock_held_by_current_thread(&filesys_lock))
	// 	lock_release(&filesys_lock);
	/* Start switched process. */
	do_iret (&_if);
	NOT_REACHED ();
}

struct thread*
get_child_process(int tid) {
	struct list *cl = &thread_current()->child_list;
	if (list_empty(cl)) return NULL;
	struct list_elem *e;
	struct thread *cthread;
	for (e = list_begin(cl); e != list_end(cl); e = list_next(e))
	{
		cthread = list_entry(e, struct thread, child_elem);
		if (cthread->tid == tid)
			return cthread;
	}
	return NULL;
}

/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
int
process_wait (tid_t child_tid UNUSED) {
	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */

	struct thread* child_thread = get_child_process(child_tid);

	if (child_thread == NULL)
		return -1;

	sema_down(&child_thread->sema_exit);

	int exit_code = child_thread->exit_code;
	list_remove(&child_thread->child_elem);

	sema_up(&child_thread->sema_wait);

	return exit_code;
}

/* Exit the process. This function is called by thread_exit (). */
void
process_exit (void) {
	struct thread *curr = thread_current ();
	/* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */
	for (size_t i = 2; i < MAX_FDT; i++) {
		if (curr->fdt[i] != NULL) 
			file_close(curr->fdt[i]);
	}
	
	palloc_free_multiple(curr->fdt, 1);
	file_close(curr->fp);
	process_cleanup ();
}

/* Free the current process's resources. */
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
#endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		/* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void
process_activate (struct thread *next) {
	/* Activate thread's page tables. */
	pml4_activate (next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update (next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */
static bool
load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* parsing file_name */
	char *save_ptr;
	char *argv[50];
	
	strlcpy(argv, file_name, strlen(file_name) + 1);
	strtok_r(file_name, " ", &save_ptr);

	/* Allocate and activate page directory. */
	t->pml4 = pml4_create ();
	if (t->pml4 == NULL)
		goto done;
	process_activate (thread_current ());

	/* Open executable file. */
	lock_acquire(&filesys_lock);
	file = filesys_open (file_name);
	if (file == NULL) {
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}
	t->fp = file;

	/* Read and verify executable header. */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* Ignore this segment. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
				if (validate_segment (&phdr, file)) {
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* Normal segment.
						 * Read initial part from disk and zero the rest. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* Entirely zero.
						 * Don't read anything from disk. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
						goto done;
				}
				else
					goto done;
				break;
		}
	}
	
	/* Set up stack. */
	if (!setup_stack (if_))
		goto done;

	/* Start address. */
	if_->rip = ehdr.e_entry;

	/* TODO: Your code goes here.
	 * TODO: Implement argument passing (see project2/argument_passing.html). */

	// <--- argument passing ---> //
	int argc = 0;
	int padded = 0;
	char *token;
	char *args[MAX_ARGS];
	uintptr_t args_p[MAX_ARGS];

	for (token = strtok_r(argv, " ", &save_ptr); token != NULL; token = strtok_r(NULL, " ", &save_ptr)) {
		args[argc] = token;
		++argc;
	}

	// arguments
	for (i = argc - 1; i >= 0; i--)
	{
		if_->rsp -= strlen(args[i]) + 1;
		args_p[i] = if_->rsp;
		memcpy(if_->rsp, args[i], strlen(args[i]) + 1);
	}

	// padding
	padded = (USER_STACK - if_->rsp) % (sizeof(uintptr_t));
	if (padded != 0) {
		if_->rsp -= sizeof(uintptr_t) - padded;
		memset(if_->rsp, 0, sizeof(uintptr_t) - padded);
	}

	// argument's address
	if_->rsp -= sizeof(uintptr_t);
	memset(if_->rsp, 0, sizeof(char*));
	for (i = argc - 1; i >= 0; i--)
	{
		if_->rsp -= sizeof(char*);
		unsigned char **p = (void*)if_->rsp;
		*p = (void *)args_p[i];
	}

	// argc & argv
	if_->R.rsi = if_->rsp;
	if_->R.rdi = argc;

	// return
	if_->rsp -= sizeof(void*);
	memset(if_->rsp, 0, sizeof(void*));

	// <--- argument passing ---> //
	file_deny_write(file);
	success = true;
done:
	/* We arrive here whether the load is successful or not. */
	// file_close (file);
	lock_release(&filesys_lock);
	return success;
}


/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. */
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page (kpage);
	}
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

	/* 파일로부터 세그먼트를 로드한다.
		주소 VA 에서 첫 번째 페이지 폴트 발생 시 이 함수가 호출된다.
		이 함수를 호출할 때 VA(가상 주소)를 사용할 수 있다. */
static bool
lazy_load_segment (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	struct lazy_load_info *info = (struct lazy_load_info*)aux;
	struct file *file = info->file;

	size_t ofs = info->ofs;
	size_t page_read_bytes = info->read_bytes;
	size_t page_zero_bytes = info->zero_bytes;
	
	/* 읽어야 하는 파일의 포인터를 옮겨준다.
		struct file* 에는 pos라는 internal parameter가 있는데, 파일을 읽게 되면 파일의 pos 위치 부터 읽는다.
		따라서, file의 pos를 offset 만큼 띄어줘야 한다. -> file_read_at을 사용하면 필요 없을 것 같으나,
		후에 파일 swap 과의 일치를 위해서 냅둔다. */
	file_seek (file, ofs);

	/* 물리 프레임이 할당되지 않았을 경우 */
	if (page->frame->kva == NULL)
		return false;

	/* 파일을 읽어 물리 프레임에 작성한다. */
	if (file_read (file, page->frame->kva, page_read_bytes) != (int) page_read_bytes) {
		// palloc_free_page(page->frame->kva);
		return false;
	}
	/* 실제 읽은 부분 외 나머지 부분은 0으로 채운다. -> stick out (?) */
	memset (page->frame->kva + page_read_bytes, 0, page_zero_bytes);
	
	return true;
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
/* ELF 헤더를 읽기 위하여 파일을 읽는 프로시저를 진행한다. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	/* 파일에 읽은 데이터가 남았을 때까지 하위 프로시저를 반복한다. */
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		/* 페이지 단위로 데이터를 읽고, 해당 데이터를 물리 페이지를 매핑하여 전달할 것이므로, 페이지 단위로 align. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		
		/* 파일에 대한 데이터는 Lazy Load 되어야 한다. lazy load를 위한 infomation을 malloc으로 할당해둔 뒤에
			lazy load 가 실행될 때 info를 가져올 수 있도록 한다. */
		struct lazy_load_info *aux_info = malloc(sizeof(struct lazy_load_info));
		if (aux_info == NULL)
			return false;

		aux_info->file = file;
		aux_info->ofs = ofs;
		aux_info->read_bytes = page_read_bytes;
		aux_info->zero_bytes = page_zero_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		/* *Anonymous 페이지를 할당한다. 
			**Load할 파일을 Anonymous 페이지로 할당하여 업로드 하는 이유!?** -> 기억 안날 경우 노션 찾아보기. */
		if (!vm_alloc_page_with_initializer (VM_ANON, upage, writable, lazy_load_segment, aux_info)) {
			/* page allocation이 실패할 경우 할당 해주었던 info를 해제해 준다. */
			free(aux_info);
			return false;
		}

		/* 읽어야 하는 데이터 갯수를 업데이트 해준다. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
		/*
			파일에서 데이터를 읽어올 때 파일 오프셋을 적절히 이동시키기 위해서이다.
			load_segment 함수는 파일의 특정 오프셋부터 시작하여 세그먼트를 로드한다. 
			이때 세그먼트의 크기가 페이지 크기보다 클 경우, 여러 페이지에 걸쳐서 세그먼트를 로드해야 한다.
			각 반복마다 page_read_bytes 만큼의 데이터를 파일에서 읽어와 페이지에 로드하고,
			이 때 파일 오프셋 ofs를 page_read_bytes 만큼 증가시켜야 다음 페이지를 로드할 때 파일의 올바른 위치에서 데이터를 읽어올 수 있다.
		*/
		ofs += page_read_bytes;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
/* stack_bottom(유저 스택 주소 - 4KB[1개의 페이지])에 스택을 매핑하고 페이지를 즉시 요청한다. */
static bool
setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* VM_MARKER_0 -> 스택 페이지임을 나타내는 정보로 사용될 수 있다. 하지만 우리의 프로젝트에서는 사용하지 않았다.
		스택 성장 시에 스택 포인터는 성장해야 하는 주소를 가리키는 데이터를 나타낼 것이기 때문에 해당 주소va에 대한 pg_round_down(va)의
		페이지 할당 요청을 할 것이기 때문이다. 이후 스왑에서 사용될 수도 있겠으나, 사용하지 않았다.	*/
	if (vm_alloc_page(VM_ANON | VM_MARKER_0, stack_bottom, true)) {
		success = vm_claim_page(stack_bottom);
		
		/* 성공하면 스택 포인터를 할당 */
		if (success)
			if_->rsp = USER_STACK;
	}

	return success;
}
#endif /* VM */
