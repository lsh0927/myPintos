/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/synch.h"

extern struct lock filesys_lock; // syscall.c에 정의된 전역 락

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
bool lazy_load_file(struct page *page, void *aux);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	// uninit_initialize에서 전달된 aux 정보를 page->file에 복사합니다.
	// 이 부분이 누락되면 lazy_load_file에서 파일 정보를 알 수 없습니다.
	struct mmap_load_info *info = (struct mmap_load_info *)page->uninit.aux;
	struct file_page *file_page = &page->file;

	file_page->file = info->file;
	file_page->offset = info->offset;
	file_page->read_bytes = info->read_bytes;
    
	return true;
}
/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;
	struct thread *curr = thread_current();

	if (pml4_is_dirty(curr->pml4, page->va)) {
        // 파일 시스템 접근 전에 락을 획득합니다.
        lock_acquire(&filesys_lock);
		file_write_at(file_page->file, page->frame->kva, 
					  file_page->read_bytes, file_page->offset);
        // 파일 시스템 접근 후에 락을 해제합니다.
		lock_release(&filesys_lock);
		
		pml4_set_dirty(curr->pml4, page->va, false);
	}
}

/* mmap된 파일 페이지를 지연 로딩하는 함수 */
bool
lazy_load_file(struct page *page, void *aux) {
    struct mmap_load_info *info = (struct mmap_load_info *)aux;

    // 파일에서 데이터를 읽어 kva(물리 프레임)에 저장
    if (file_read_at(info->file, page->frame->kva, info->read_bytes, info->offset) != (int)info->read_bytes) {
        free(info);
        return false;
    }

    // 페이지의 나머지 부분을 0으로 채움
    memset(page->frame->kva + info->read_bytes, 0, info->zero_bytes);

    free(info);
    return true;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

    // 1. 파일 복제
    // file_reopen을 통해 이 mmap만을 위한 별도의 file 객체를 만듭니다.
    struct file *reopened_file = file_reopen(file);

	if (reopened_file == NULL) {
        return NULL;
    }
	
	off_t total_file_length = file_length(reopened_file);
    if (total_file_length == 0) {
        file_close(reopened_file);
        return NULL;
    }

    // 2. 페이지 생성 루프
    void *current_addr = addr;
    size_t remaining_length = length;
    off_t current_offset = offset;

    while (remaining_length > 0) {
        // 2-1. 현재 페이지에서 읽을 바이트 수 계산
        size_t page_read_bytes = remaining_length < PGSIZE ? remaining_length : PGSIZE;

		off_t actual_file_left = total_file_length > current_offset ? total_file_length - current_offset : 0;
        if (page_read_bytes > actual_file_left) {
            page_read_bytes = actual_file_left;
        }

        size_t page_zero_bytes = PGSIZE - page_read_bytes;
        
        // 2-2. SPT에 이미 페이지가 있는지 확인 (주소 충돌 방지)
        if (spt_find_page(&thread_current()->spt, current_addr) != NULL) {
            // 실패 시, 지금까지 만든 페이지들을 모두 정리하고 NULL 반환해야 함 (구현 필요)
            file_close(reopened_file);
            return NULL;
        }

        // 2-3. 지연 로딩 정보를 담을 aux 구조체 생성
        struct mmap_load_info *info = (struct mmap_load_info *)malloc(sizeof(struct mmap_load_info));
        if (info == NULL) {
            file_close(reopened_file);
            return NULL;
        }
        info->file = reopened_file;
        info->offset = current_offset;
        info->read_bytes = page_read_bytes;
        info->zero_bytes = page_zero_bytes;

        // 2-4. 지연 로딩을 위한 UNINIT 페이지 생성
        if (!vm_alloc_page_with_initializer(VM_FILE, current_addr, writable, lazy_load_file, info)) {
            free(info);
            file_close(reopened_file);
            // 실패 시, 지금까지 만든 페이지들을 모두 정리하고 NULL 반환해야 함 (구현 필요)
            return NULL;
        }

        // 2-5. 다음 페이지를 위해 변수 업데이트
		size_t length_this_page = remaining_length < PGSIZE ? remaining_length : PGSIZE;

       // 수정된 부분: page_read_bytes가 아닌, 이번 루프에서 처리한
        //    		 가상 주소 공간의 크기(PGSIZE 또는 남은 길이)만큼 빼줍니다.
        remaining_length -= length_this_page;
        current_addr += PGSIZE;
        current_offset += page_read_bytes;
    }

    return addr; // 성공 시 시작 주소 반환
}

/* Do the munmap */
void
do_munmap (void *addr) {
    struct thread *curr = thread_current();
    
    // 1. 시작 주소에 해당하는 페이지를 SPT에서 찾습니다.
    struct page *p = spt_find_page(&curr->spt, addr);
    if (p == NULL || VM_TYPE(p->operations->type) != VM_FILE) {
        return; // 매핑된 페이지가 아니면 아무것도 하지 않습니다.
    }

    // 2. 이 매핑에 사용된 file 객체를 식별자로 사용합니다.
    struct file *target_file = p->file.file; 

    // 3. 같은 파일에 매핑된 모든 연속적인 페이지를 찾아 해제합니다.
    void *current_addr = addr;
    while (true) {
        struct page *current_page = spt_find_page(&curr->spt, current_addr);
        if (current_page == NULL) break; // 페이지가 없으면 중단

        // 페이지 타입이 다르거나, 다른 파일에 매핑된 페이지이면 중단
        if (VM_TYPE(current_page->operations->type) != VM_FILE || current_page->file.file != target_file) {
            break;
        }

        // 페이지 제거 요청 (내부적으로 destroy -> file_backed_destroy 호출)
        spt_remove_page(&curr->spt, current_page);

        current_addr += PGSIZE;
    }

    // 4. 매핑에 사용했던 파일 객체를 닫습니다.
    file_close(target_file);
}
