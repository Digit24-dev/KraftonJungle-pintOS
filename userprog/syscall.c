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
#include "filesys/file.h"
#include "threads/palloc.h"
#include <string.h>

#include "userprog/process.h"
// #include "string.h"

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
	lock_init(&filesys_lock);

	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

bool
address_check (void *pointer) {
	// printf("NULL: %p \n", (void *)NULL);
	// printf("pointer: %p \n", pml4_get_page(thread_current()->pml4, pointer));
	void *pp = pml4_get_page(thread_current()->pml4, pointer);
	if (pointer == NULL || is_kernel_vaddr(pointer) || pp == NULL)
		exit(-1);
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
			f->R.rax = f->R.rdi;
			exit((int)f->R.rdi);
			break;

		case SYS_FORK:
			address_check((void*)arg1);
			f->R.rax = fork(arg1, f);
			break;

		case SYS_EXEC:
			address_check((void*)arg1);
			if (arg1 == "") exit(-1);
			f->R.rax = exec((const char*)arg1);
			break;

		case SYS_WAIT:
			f->R.rax = wait((int)arg1);
			break;

		case SYS_CREATE:
			address_check((void*)arg1);
			if (arg1 == "") exit(-1);
			f->R.rax = create((const char*)arg1, (unsigned int)arg2);
			break;

		case SYS_REMOVE:
			address_check((void*)arg1);
			f->R.rax = remove((const char*)arg1);
			break;

		case SYS_OPEN:
			address_check((void*)arg1);
			if (arg1 == "") exit(-1);
			f->R.rax = open((const char*)arg1);
			break;

		case SYS_FILESIZE:
			f->R.rax = filesize((int)arg1);
			break;

		case SYS_READ:
			address_check((void*)arg2);
			f->R.rax = read((int)arg1, (void*)arg2, (unsigned int)arg3);
			break;

		case SYS_WRITE:
			address_check((void*)arg2);
			f->R.rax = write((int)arg1, (const void*)arg2, (unsigned int)arg3);
			break;

		case SYS_SEEK:
			seek((int)arg1, (unsigned int)arg2);
			break;

		case SYS_TELL:
			f->R.rax = tell((int)arg1);
			break;

		case SYS_CLOSE:
			close((int)arg1);
			break;

		default:
			exit(-1);
			break;
	}
}

void halt (void)
{
	power_off();
	NOT_REACHED();
}

void exit (int status)
{
	struct thread *curr = thread_current();
	thread_current()->exit_code = status;
	printf("%s: exit(%d)\n", curr->name, curr->exit_code);
	thread_exit();
}

int write (int fd, const void *buffer, unsigned length)
{
	unsigned int str_cnt = 0;
	const char *p = buffer;
	struct file *param = NULL;

	if (fd >= MAX_FDT || fd < 0) return -1;

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
		param = fd_to_file(fd);
		if (param == NULL) return -1;
		lock_acquire(&filesys_lock);
		str_cnt = file_write(param, buffer, length);
		lock_release(&filesys_lock);
		break;
	}
	return str_cnt;
}

int read (int fd, void *buffer, unsigned length)
{
	int str_cnt = 0;
	const char *p = buffer;
	struct file *fp = NULL;
	char c;

	if (fd >= MAX_FDT || fd < 0) return -1;

	switch (fd)
	{
	case STDIN_FILENO:
		while (c = input_getc())
		{
			if (c == '\n') // || c == '\0')
				break;
			*(char*)p = c;
			p++;
			++str_cnt;
		}
		break;
	case STDOUT_FILENO:
		/* need to set errno to EBADF */
		str_cnt = -1;
		break;
	default:
		fp = fd_to_file(fd);
		if (fp == NULL) exit(-1);
		lock_acquire(&filesys_lock);
		str_cnt = file_read(fp, buffer, length);
		lock_release(&filesys_lock);
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
	struct file* param = filesys_open(file);
	if (param == NULL) return -1;
	return thread_add_file(param);
}

int filesize (int fd)
{
	struct file *param = fd_to_file(fd);
	return file_length(param);
}

void seek (int fd, unsigned position)
{
	struct file* param = fd_to_file(fd);
	file_seek(param, position);
}

unsigned tell (int fd)
{
	struct file *param = fd_to_file(fd);
	return file_tell(param);
}

void close (int fd)
{
	if (fd < 0 || fd >= MAX_FDT) exit(-1);
	struct file *param = fd_to_file(fd);
	if (param == NULL) exit(-1);
	thread_current()->fdt[fd] = NULL;
	file_close(param);
}

/* fd -> struct file* */
struct file*
fd_to_file (int fd) {
	return thread_current()->fdt[fd];
}

int 
thread_add_file (struct file *f)
{
	struct thread *cur = thread_current();
	int ret = cur->nex_fd;
	cur->fdt[cur->nex_fd] = f;
	++cur->nex_fd;
	return ret;
}

pid_t fork (const char *thread_name, struct intr_frame *f)
{
	address_check((void*)thread_name);
	return process_fork(thread_name, f);
}

int wait (pid_t pid)
{
	return process_wait(pid);
}

int exec (const char *file)
{
	char *temp = palloc_get_page(0);
	strlcpy(temp, file, strlen(file) + 1);
	sema_down(&thread_current()->sema_load);
	return process_exec(temp);
}
