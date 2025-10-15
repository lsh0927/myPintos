#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct file_page {
    struct file *file;      // 매핑된 파일 객체 포인터
    off_t offset;           // 파일 내 오프셋
    size_t read_bytes;      // 페이지에 해당하는 파일의 크기
};

/* mmap 지연 로딩에 필요한 정보를 담는 구조체 */
struct mmap_load_info {
    struct file *file;
    off_t offset;
    size_t read_bytes;
    size_t zero_bytes;
};

bool lazy_load_file(struct page *page, void *aux);
void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void do_munmap (void *va);
#endif
