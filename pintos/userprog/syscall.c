#include "userprog/syscall.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "intrinsic.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/loader.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/gdt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall-nr.h>

#define MSR_STAR 0xc0000081
#define MSR_LSTAR 0xc0000082
#define MSR_SYSCALL_MASK 0xc0000084

void syscall_entry(void);
void syscall_handler(struct intr_frame *);
static void check_address(void *addr);
static void release_fd(int fd);
static int allocate_fd(struct file *file);
static struct file *get_file(int fd);
static void check_buffer(void *buffer, unsigned size);
static void *mmap(void *addr, size_t length, int writable, int fd, off_t offset);
static void munmap(void *addr);
static struct lock filesys_lock;

void syscall_init(void) {
  write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 | ((uint64_t)SEL_KCSEG)
                                                               << 32);

  write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

  write_msr(MSR_SYSCALL_MASK,
            FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

  lock_init(&filesys_lock);
}

void syscall_handler(struct intr_frame *f) {
  uint64_t nr = f->R.rax;
#ifdef VM
  thread_current()->rsp = f->rsp;
#endif
  switch (nr) {
  case SYS_HALT:
    power_off();
    break;

  case SYS_EXIT: {
    int status = (int)f->R.rdi; // 첫 번째 인자에서 종료 상태 추출
    struct thread *cur = thread_current();
    cur->exit_status = status; // 부모가 wait으로 받을 종료 상태 저장
    printf("%s: exit(%d)\n", cur->name, status); // 디버깅용 출력
    thread_exit(); // 프로세스 종료 (리턴하지 않음)
  } break;

  case SYS_WRITE: {
    int fd = (int)f->R.rdi;                      // 파일 디스크립터
    const void *buffer = (const void *)f->R.rsi; // 데이터 버퍼
    unsigned size = (unsigned)f->R.rdx;          // 쓸 크기

    check_buffer((void *)buffer, size);

    if (fd == 1) {
      putbuf((const char *)buffer, size);
      f->R.rax = size; // 성공시 쓴 바이트 수 반환
    } else if (fd == 0) {
      f->R.rax = -1; // 에러 반환
    } else {
      struct file *file = get_file(fd);
      if (file == NULL) {
        f->R.rax = -1;
        break;
      }

      lock_acquire(&filesys_lock);
      int bytes_written = file_write(file, buffer, size);
      lock_release(&filesys_lock);

      f->R.rax = bytes_written; // 실제로 쓴 바이트 수 반환
    }
    break;
  }

  case SYS_CREATE: {
    const char *path = (const char *)f->R.rdi;
    unsigned sz = (unsigned)f->R.rsi;

    check_address((void *)path);

    if (!path || path[0] == '\0') {
      f->R.rax = false;
      break;
    }

    lock_acquire(&filesys_lock);
    bool result = filesys_create(path, sz);
    lock_release(&filesys_lock);

    f->R.rax = result; // 생성 결과 반환
    break;
  }

  case SYS_OPEN: {
    const char *path = (const char *)f->R.rdi;

    check_address((void *)path); // 경로 주소 검증

    lock_acquire(&filesys_lock);
    struct file *file = filesys_open(path);
    lock_release(&filesys_lock);

    if (file == NULL) {
      f->R.rax = -1;
    } else {
      int fd = allocate_fd(file);
      if (fd == -1) {
        file_close(file); // 메모리 누수 방지
        f->R.rax = -1;
      } else {
        f->R.rax = fd; // 성공적으로 할당된 fd 반환
      }
    }
    break;
  }

  case SYS_CLOSE: {
    int fd = (int)f->R.rdi;

    struct file *file = get_file(fd);
    if (file != NULL) {
      lock_acquire(&filesys_lock);
      file_close(file);
      lock_release(&filesys_lock);

      release_fd(fd); // fd 슬롯 해제
    }
    break;
  }

  case SYS_READ: {
    int fd = (int)f->R.rdi;
    void *buffer = (void *)f->R.rsi;
    unsigned size = (unsigned)f->R.rdx;

    check_buffer(buffer, size);

    if (fd == 0) {
      char *buf = (char *)buffer;
      for (unsigned i = 0; i < size; i++) {
        buf[i] = input_getc(); // 키보드에서 문자 하나 읽기
      }
      f->R.rax = size; // 요청한 만큼 다 읽었다고 가정
    } else if (fd == 1) {
      f->R.rax = -1;
    } else {
      struct file *file = get_file(fd);
      if (file == NULL) {

        f->R.rax = -1;
        break;
      }

      lock_acquire(&filesys_lock);
      int bytes_read = file_read(file, buffer, size);
      lock_release(&filesys_lock);

      f->R.rax = bytes_read;
    }
    break;
  }

  case SYS_FILESIZE: {
    int fd = (int)f->R.rdi;

    struct file *file = get_file(fd);
    if (file == NULL) {
      f->R.rax = -1;
      break;
    }

    lock_acquire(&filesys_lock);
    off_t size = file_length(file);
    lock_release(&filesys_lock);

    f->R.rax = size;
    break;
  }

  case SYS_FORK: {
    const char *thread_name = (const char *)f->R.rdi;

    check_address((void *)thread_name);

    f->R.rax = process_fork(thread_name, f);
    break;
  }

  case SYS_EXEC: {
    const char *cmd_line = (const char *)f->R.rdi;
    check_address((void *)cmd_line);

    char *cmd_copy = palloc_get_page(0); // 커널 메모리 할당
    if (cmd_copy == NULL) {
      f->R.rax = -1;
      break;
    }
    strlcpy(cmd_copy, cmd_line, PGSIZE); // 안전한 문자열 복사

    f->R.rax = process_exec(cmd_copy);
    break;
  }

  case SYS_WAIT: {
    tid_t pid = (tid_t)f->R.rdi;
    f->R.rax = process_wait(pid);
    break;
  }

  case SYS_SEEK: {
    int fd = (int)f->R.rdi;
    unsigned position = (unsigned)f->R.rsi;

    struct file *file = get_file(fd);
    if (file != NULL) {
      lock_acquire(&filesys_lock);
      file_seek(file, position);
      lock_release(&filesys_lock);
    }
    break;
  }

  case SYS_TELL: {
    int fd = (int)f->R.rdi;
    struct file *file = get_file(fd);
    if (file == NULL) {
      f->R.rax = -1;
      break;
    }

    lock_acquire(&filesys_lock);
    f->R.rax = file_tell(file);
    lock_release(&filesys_lock);
    break;
  }

  case SYS_REMOVE: {
    const char *file = (const char *)f->R.rdi;
    check_address((void *)file);

    lock_acquire(&filesys_lock);
    f->R.rax = filesys_remove(file);
    lock_release(&filesys_lock);
    break;
  }

  case SYS_MMAP:
    f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8);
    break;

  case SYS_MUNMAP:
    munmap(f->R.rdi);
    break;

  default:
    printf("Unknown system call: %d\n", (int)nr);
    printf("%s: exit(-1)\n", thread_current()->name);
    thread_current()->exit_status = -1;
    thread_exit(); // 프로세스 강제 종료
    break;
  }
}

static void check_address(void *addr) {
  if (addr == NULL || !is_user_vaddr(addr)) {
    printf("%s: exit(-1)\n", thread_current()->name);
    thread_current()->exit_status = -1;
    thread_exit();
  }
}

static int allocate_fd(struct file *file) {
  struct thread *cur = thread_current();

  for (int fd = 3; fd < cur->fd_idx && fd < FDCOUNT_LIMIT; fd++) {
    if (cur->fdt[fd] == NULL) {
      cur->fdt[fd] = file;
      if (fd >= cur->fd_idx)
        cur->fd_idx = fd + 1; // 최대 인덱스 업데이트
      return fd;
    }
  }

  if (cur->fd_idx < FDCOUNT_LIMIT) {
    cur->fdt[cur->fd_idx] = file;
    return cur->fd_idx++; // 후위 증가로 현재값 반환 후 증가
  }

  return -1;
}

static struct file *get_file(int fd) {
  struct thread *cur = thread_current();

  if (fd < 0 || fd >= FDCOUNT_LIMIT || fd >= cur->fd_idx) {
    return NULL;
  }

  return cur->fdt[fd];
}

static void release_fd(int fd) {
  struct thread *cur = thread_current();

  if (fd >= 3 && fd < FDCOUNT_LIMIT && fd < cur->fd_idx) {
    cur->fdt[fd] = NULL; // 슬롯을 비워서 재사용 가능하게 함
  }
}

static void check_buffer(void *buffer, unsigned size) {
  if (buffer == NULL || !is_user_vaddr(buffer)) {
    printf("%s: exit(-1)\n", thread_current()->name);
    thread_current()->exit_status = -1;
    thread_exit();
  }

  char *end = (char *)buffer + size - 1;
  if (!is_user_vaddr(end)) {
    printf("%s: exit(-1)\n", thread_current()->name);
    thread_current()->exit_status = -1;
    thread_exit();
  }
}

static void *mmap(void *addr, size_t length, int writable, int fd, off_t offset) {
  if(!addr || addr != pg_round_down(addr)) {
    return NULL;
  }

  if(offset != pg_round_down(offset)) {
    return NULL;
  }

  if(!is_user_vaddr(addr) || !is_user_vaddr(addr + length)) {
    return NULL;
  }

  if(spt_find_page(&thread_current()->spt, addr)) {
    return NULL;
  }

  struct file *f = process_get_file(fd);

  if (f == NULL) {
    return NULL;
  }

  if(file_length(f) == 0 || (int)length <= 0) {
    return NULL;
  }

  return do_mmap(addr, length, writable, f, offset);
}

static void munmap(void *addr) {
  do_munmap(addr);
}
