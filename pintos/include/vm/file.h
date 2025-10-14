#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

/* Page structure for file-backed pages. */
struct file_page {
  struct file *file;      /* 매핑된 파일 객체 */
  off_t ofs;              /* 파일 내 오프셋 */
  uint32_t read_bytes;    /* 파일에서 읽을 데이터 크기 */
  uint32_t zero_bytes;    /* 남은 부분 0으로 채움 */
  bool writable;          /* 쓰기 가능 여부 */
};

struct lazy_load_arg {
  struct file *file;      /* 백업 파일 객체 */
  off_t ofs;              /* 읽을 시작 위치 */
  uint32_t read_bytes;    /* 파일에서 읽을 바이트 수 */
  uint32_t zero_bytes;    /* 나머지를 0으로 채워야 할 바이트 수 */
  bool writable;          /* 쓰기 가능 여부 */
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void do_munmap (void *va);
#endif
