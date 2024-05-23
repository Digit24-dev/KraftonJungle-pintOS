#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
/* customed 0523 */
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/input.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	/* customed 0523 */
	uint64_t syscall_number = f->R.rax;
	switch (syscall_number)
	{
		case SYS_WRITE :
		{
			int fd = f->R.rdi;
			const void *buffer = f->R.rsi;
			unsigned size = f->R.rdx;

			address_checker(buffer);

			if (fd == 1)
			{
				putbuf(buffer, size);
				f->R.rax = size;
			}
			else
			{
				f->R.rax = -1;
			}
			break;
		}

		case SYS_EXIT :
		{
			int status = f->R.rdi;
			thread_current()->exit_code = status;
			thread_exit();
			break;
		}

		case SYS_OPEN :
		{
			const char *file = f->R.rdi;
			address_checker(file);
			struct file *open_file = filesys_open(file);
			if (open_file == NULL)
			{
				f->R.rax = -1;
			}
		}
		
		case SYS_READ :
		{
			int str_cnt = 0;
			
			int fd = f->R.rdi;
			const void *buffer = f->R.rsi;
			unsigned size = f->R.rdx;
			address_checker(buffer);

			if (fd == 0)
			{
				char c;
				while (c = input_getc())
				{
					if (c == '\n')
						break;
					*(char*)buffer = c;
					buffer++;
					++str_cnt;
				}
				f->R.rax = str_cnt;
			}
			else
			{
				f->R.rax = -1;
			}
			break;
		}

		case SYS_CREATE :
		{
			const char *file = f->R.rdi;
			unsigned initial_size = f->R.rsi;
			address_checker((void *)file);
			f->R.rax = filesys_create(file, initial_size);
			break;
		}

		case SYS_REMOVE :
		{
			const char *file = f->R.rdi;
			address_checker((void *)file);
			f->R.rax = filesys_remove(file);
			break;
		}

		default :
			printf ("system call!\n");
			thread_exit ();
	}
}

/* customed 0523 */
void address_checker(void *address){
	if (address == NULL || pml4_get_page(thread_current()->pml4, address) == NULL)
	{
		thread_exit();
	}
}