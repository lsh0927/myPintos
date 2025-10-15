#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

/* 지연 로딩에 필요한 정보를 담는 구조체 */
struct lazy_load_info{
	struct file *file;
	off_t ofs;
	uint32_t read_bytes;
	uint32_t zero_bytes;
};

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

extern struct lock filesys_lock;

#endif /* userprog/process.h */
