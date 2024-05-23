#include "userprog/syscall.h"
// #include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "lib/stdio.h"
#include "filesys/filesys.h"

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

#define MAX_FD	255
static struct file *inner_fd[MAX_FD];

void
fd_init(void) {
	inner_fd[STDIN_FILENO] = NULL;
	inner_fd[STDOUT_FILENO] = NULL;
}

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
	// TODO: Your implementation goes here.
	// printf ("system call!\n");
	uint64_t arg1 = f->R.rdi;
	uint64_t arg2 = f->R.rsi;
	uint64_t arg3 = f->R.rdx;
	uint64_t arg4 = f->R.r10;
	uint64_t arg5 = f->R.r8;
	uint64_t arg6 = f->R.r9;

	// check validity
	switch (f->R.rax)
	{
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit((int)f->R.rdi);
			f->R.rax = f->R.rdi;
			break;
	// 	case SYS_FORK:
	// 		fork();
	// 		break;
	// 	case SYS_EXEC:
	// 		exec();
	// 		break;
	// 	case SYS_WAIT:
	// 		wait();
	// 		break;
		case SYS_CREATE:
			create((const char*)arg1, (unsigned int)arg2);
			break;
		case SYS_REMOVE:
			remove((const char*)arg1);
			break;
		case SYS_OPEN:
			open((const char*)arg1);
			break;
	// 	case SYS_FILESIZE:
	// 		filesize();
	// 		break;
		case SYS_READ:
			printf("Read() called. \n");
			f->R.rax = read((int)arg1, (void*)arg2, (unsigned int)arg3);
			break;
		case SYS_WRITE:
			f->R.rax = write((int)arg1, (const void*)arg2, (unsigned int)arg3);
			break;
	// 	case SYS_SEEK:
	// 		seek();
	// 		break;
	// 	case SYS_TELL:
	// 		tell();
	// 		break;
	// 	case SYS_CLOSE:
	// 		close();
	// 		break;
		default:
			thread_exit();
			break;
	}
}

void halt (void)
{
	power_off();
}

void exit (int status)
{
	struct thread *curr = thread_current();
	printf("%s: exit(%d)\n", curr->name, curr->exit_code);
	thread_current()->exit_code = status;
	thread_exit();
}

int write (int fd, const void *buffer, unsigned length)
{
	int str_cnt = 0;
	const char *p = buffer;
	switch (fd)
	{
	case STDIN_FILENO:
		/* need to set errno to EBADF (Bad File Descriptor) */
		str_cnt = -1;
		break;
	case STDOUT_FILENO:
		while (*p != '\0' && str_cnt <= length) {
			p++;
			++str_cnt;
		}
		putbuf(buffer, str_cnt);
		break;

	default:
		break;
	}
	return str_cnt;
}

int read (int fd, void *buffer, unsigned length)
{
	int str_cnt = 0;
	const char *p = buffer;
	char c;
	switch (fd)
	{
	case 0:
		while (c = input_getc())
		{
			if (c == '\n') // || c == '\0')
				break;
			*(char*)p = c;
			p++;
			++str_cnt;
		}
		break;
	case 1:
		/* need to set errno to EBADF */
		str_cnt = -1;
		break;

	default:
		break;
	}
	return str_cnt;
}

bool create (const char *file, unsigned initial_size)
{
	return filesys_create(file, initial_size);
}

bool remove (const char *file)
{
	return filesys_remove(file);
}

int open (const char *file)
{
	filesys_open(file);
}

int filesize (int fd)
{

}