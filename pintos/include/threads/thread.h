#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* States in a thread's life cycle. */
enum thread_status
{
	THREAD_RUNNING, /* Running thread. */
	THREAD_READY,	/* Not running but ready to run. */
	THREAD_BLOCKED, /* Waiting for an event to trigger. */
	THREAD_DYING	/* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) - 1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0	   /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63	   /* Highest priority. */

#define FD_MAX 128 		//최대 파일 디스크립터 개수


/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread
{
	/* Owned by thread.c. */
	tid_t tid;				   /* Thread identifier. */
	enum thread_status status; /* Thread state. */
	char name[16];			   /* Name (for debugging purposes). */
	int base_priority;		   /* thread base priority. */
	int priority;			   /* Priority. */
	struct list donators;	   /* donation list. */
	struct lock *waiting_lock;  /* wating lock. */
	int64_t wakeup_tick;	   /* ticks of wakeup. */
	/* Shared between thread.c and synch.c. */
	struct list_elem elem;			/* List element. */
	struct list_elem donation_elem; /* Donation list element. */

	struct file *fd_table[FD_MAX]; 		//파일 객체 포인터 배열
	int next_fd; 		//다음에 할당할 fd 번호
	struct file *running_executable;	//현재 실행 중인 파일 객체

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4; /* Page map level 4 */

	struct semaphore fork_sema;		/* 자식 프로세스가 fork를 완료할때까지 부모가 기다리기 위한 세마포어 */
	struct intr_frame parent_if;	/* 부모 레지스터 정보를 자식에게 전달하기 위한 프레임 */
	
	/* exit와 wait 동기화에 필요한 멤버 변수 */
	int exit_status;			/* 자식 프로세스가 exit호출했을때 status값 저장 */
	struct semaphore wait_sema;	/* 부모 프로세스가 wait 콜에서 자식이 종료될때까지 기다림, 자식 exit할때 세마 업해서 잠든 부모 깨움*/
	struct semaphore reap_sema;	/* 부모가 자원 회수를 완료했음을 알리는 세마포어 */
	struct thread *parent;		/* 현재 스레드의 부모 스레드를 가리키는 포인터, 자식 종료->누구의 wait_sema를 깨울지 */
	struct list child_list;		/* 부모 스레드의 모든 자식 저장, wait 시 리스트 검색하여 자식 찾음 */
	struct list_elem child_elem;/* 현재(자식) 스레드를 부모의 child_list에 넣기 위한 연결고리 */
	bool exit_called;           /* sys_exit() 등에서 이미 exit 처리가 되었는지 표시 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf; /* Information for switching */
	unsigned magic;		  /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_sleep(void);
void awake_sleep_threads(int64_t tick);

void thread_block(void);
void thread_unblock(struct thread *);
void thread_awake(struct thread *t);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

void donate(struct thread *thr, struct lock *l);
void thread_restore_by_lock(struct lock *lock);

void do_iret(struct intr_frame *tf);

bool higher_priority(const struct list_elem *a, const struct list_elem *b, void *aux);

#endif /* threads/thread.h */
