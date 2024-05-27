#include <stdbool.h>


#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
#endif /* userprog/syscall.h */

typedef int pid_t;
void address_checker(void *address);
void halt (void);
void exit (int status);
int write (int fd, const void *buffer, unsigned length);
int read (int fd, void *buffer, unsigned length);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
struct file* fd_to_file(int fd);
int file_to_fd (struct file* file);
int thread_add_file (struct file *f);
pid_t fork (const char *thread_name);
int wait (pid_t pid);
int exec (const char *cmd_line);