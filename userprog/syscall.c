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
#include "threads/init.h"
#include "include/threads/vaddr.h"
/* customed 0524 */
#include "include/userprog/process.h"
#include "threads/palloc.h"

struct lock filesys_lock;

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

	uint64_t arg1 = f->R.rdi;
	uint64_t arg2 = f->R.rsi;
	uint64_t arg3 = f->R.rdx;
	uint64_t arg4 = f->R.r10;
	uint64_t arg5 = f->R.r8;
	uint64_t arg6 = f->R.r9;

	uint64_t syscall_number = f->R.rax;
	switch (syscall_number)
	{
		case SYS_HALT :
		{
			halt();
			break;
		}

		case SYS_EXIT :
		{
			f->R.rax = f->R.rdi;
			exit((int)f->R.rdi);
			break;
		}

		case SYS_FORK :
		{
			address_checker(arg1);
			memcpy(&thread_current()->copied_if, f, sizeof(struct intr_frame));
			f->R.rax = fork(arg1);
			break;
		}

		case SYS_EXEC :
		{
			address_checker(arg1);
			f->R.rax = exec(arg1);
		}

		case SYS_WAIT :
		{
			f->R.rax = wait((pid_t)arg1);
			break;
		}

		case SYS_CREATE :
		{
			address_checker(arg1);
			f->R.rax = create((const char*)arg1, (unsigned int)arg2);
			break;
		}

		case SYS_REMOVE : 
		{
			address_checker(arg1);
			f->R.rax = remove((const char*)arg1);
			break;
		}

		case SYS_OPEN :
		{
			address_checker(arg1);
			f->R.rax = open((const char*)arg1);
			break;
		}

		case SYS_FILESIZE :
		{
			f->R.rax = filesize((int)arg1);
			break;
		}

		case SYS_READ : //file
		{
			address_checker(arg2);
			f->R.rax = read((int)arg1, (void*)arg2, (unsigned int)arg3);
			break;
		}

		case SYS_WRITE : //file
		{
			address_checker((void*)arg2);
			f->R.rax = write((int)arg1, (const void*)arg2, (unsigned int)arg3);
			break;
		}

		case SYS_SEEK :
		{
			seek((int)arg1, (unsigned int)arg2);
			break;
		}

		case SYS_TELL :
		{
			f->R.rax = tell((int)arg1);
			break;
		}
		case SYS_CLOSE :
		{
			close((int)arg1);
			break;
		}
		
		default :
			exit(-1);
			break;
	}
}

/* customed 0523 */
void address_checker(void *address){
	if (address == NULL || is_kernel_vaddr(address) || pml4_get_page(thread_current()->pml4, address) == NULL)
	{
		exit(-1);
	}
}

void halt (void)
{
	power_off();
}

void exit (int status)
{
	struct thread *curr = thread_current();
	curr->exit_code = status;
	printf ("%s: exit(%d)\n", curr->name, status);
	thread_exit();
}

int write (int fd, const void *buffer, unsigned length)
{
	int str_cnt = 0;
	const char *p = buffer;
	if (fd >= FDT_MAX || fd < 0) return -1;

	if (fd == 1)
	{
		while (*p != '\0' && str_cnt <= length) {
			p++;
			++str_cnt;
		}
		putbuf(buffer, str_cnt);
		return str_cnt;
	}
	else if (fd == 0)
	{
		str_cnt = -1;
		return str_cnt;
	}
	else
	{
		struct file *open_file = fd_to_file(fd);
		if (open_file == NULL)
			exit(-1);
		
		str_cnt = file_write(open_file, buffer, length);
		return str_cnt;
	}
}

int read (int fd, void *buffer, unsigned length)
{
	address_checker(buffer);
	int str_cnt = 0;
	char c;
	const char *p = buffer;

	if (fd >= FDT_MAX || fd < 0) return -1;

	if (fd == 0)
	{
		while (c = input_getc())
		{
			if (c == '\n')
				break;
			*(char*)p = c;
			p++;
			++str_cnt;
		}
		return str_cnt;
	}
	else if (fd == 1)
	{
		str_cnt = -1;
		return str_cnt;
	}
	else
	{
		struct file *open_file = fd_to_file(fd);
		if (open_file == NULL)
			exit(-1);

		str_cnt = file_read(open_file, buffer, length);
		return str_cnt;
	}
}

bool create (const char *file, unsigned initial_size)
{
	address_checker(file);
	return filesys_create(file, initial_size);
}

bool remove (const char *file)
{
	address_checker(file);
	return filesys_remove(file);
}

int open (const char *file)
{
	address_checker(file);
	struct file *open_file = filesys_open(file);
	if (open_file == NULL)
		return -1;
	return thread_add_file(open_file);
}

int filesize (int fd)
{
	struct file *open_file = fd_to_file(fd);
	return file_length(open_file);
}

void seek (int fd, unsigned position)
{
	struct file*open_file = fd_to_file(fd);
	file_seek(open_file, position);
}

unsigned tell (int fd)
{
	struct file *open_file = fd_to_file(fd);
	return file_tell(open_file);
}

void close (int fd)
{
	if (fd < 0 || fd >= FDT_MAX) exit(-1);
	struct file *open_file = fd_to_file(fd);
	if (open_file == NULL) exit(-1);
	thread_current()->fdt[fd] = NULL;
	file_close(open_file);
}

/* fd */
struct file* fd_to_file(int fd)
{
    struct thread *t = thread_current();
    
    // 파일 디스크립터 유효성 검사
    if (fd < 0 || fd >= FDT_MAX)
        return NULL;
    
    // 해당 파일 객체가 존재하는지 확인
    if (t->fdt[fd] == NULL)
        return NULL;
    
    return t->fdt[fd];
}

int file_to_fd (struct file* file)
{
    struct file **p = thread_current()->fdt;
    for (size_t i = 2; i < FDT_MAX; i++)
    {
        if (*(p + i) == file) {
            return i;
        }
    }
    return -1;    // Error
}

int thread_add_file (struct file *f)
{
    struct thread *cur = thread_current();
    
    // 파일 디스크립터 테이블이 가득 찼는지 확인
    if (cur->next_fd >= FDT_MAX)
        return -1;
    
    int ret = cur->next_fd;
    cur->fdt[cur->next_fd] = f;
    cur->next_fd++;
    return ret;
}

/* process */
pid_t fork (const char *thread_name){
	address_checker(thread_name);
	return process_fork(thread_name, NULL);
}

int wait (pid_t pid) {
	return process_wait(pid);
}

int exec (const char *cmd_line){
	address_checker(cmd_line);
	if (cmd_line == NULL)
		exit(-1);
	
	char *cmd_line_tmp = palloc_get_page(0);
	if (cmd_line_tmp == NULL)
		exit(-1);

	memcpy(cmd_line_tmp, cmd_line, strlen(cmd_line) + 1);

	tid_t tid = process_exec(cmd_line_tmp);
	if (tid == TID_ERROR)
		exit(-1);

	return tid;
}