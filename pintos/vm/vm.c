/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include <cstdlib>

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

		/* TODO: Insert the page into the spt. */
	}
err:
	return false;
}

void spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame * vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame * vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

// User pool에서 새로운 Physical page를 가져와서 새로운 frame 구조체에 할당해서 반환
static struct frame * vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
  
  // user pool에서 새로운 physical page를 가져옴
  void *kva = palloc_get_page(PAL_UESR);
  
  // page 할당 실패 -> 나중에 swap_out 처리
  // OS 중시, 소스 파일명, 라인 번호, 함수명 등의 정보와 함께 사용자 지정 메시지를 출력
  if (kva == NULL) {
    PANIC("todo");
  }
  
  // 프레임 할당
  frame = malloc(sizeof(struct frame));
  // 프레임 멤버 초기화
  frame->kva = kva;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED, bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

// spt에서 va에 해당하는 페이지를 가져와서 frame과의 매핑을 요청
bool vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
  // spt에서 va에 해당하는 page 찾기
  page = spt_find_page(&thread_current()->spt, va);

  if (page == NULL) {
    return false;
  }

	return vm_do_claim_page(page);
}

// 새 frame을 가져와서 page와 맵핑
static bool vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
  // 가상 주소와 물리 주소를 매핑
  struct thread *current = thread_current();
  pml4_set_page(current->pml4, page->va, frame->kva, page->writable)
	
  return swap_in (page, frame->kva);
}


/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED, struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}


/* 해시 함수: 페이지의 가상 주소를 해시값으로 변환 */
static unsigned page_hash(const struct hash_elem *e, void *aux UNUSED) {
  // 1. hash_elem으로부터 struct page를 얻어야 함
	// Pintos가 제공하는 hash_entry 매크로
	const struct page *p = hash_entry(e, struct page, hash_elem);

  // 2. page의 va를 해시값으로 변환
	return hash_bytes(&p->va, sizeof p->va);

}

/* 비교 함수: 두 페이지를 비교 (a < b?) */
static bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  // 1. 두 hash_elem으로부터 각각 struct page 얻기
	const struct page *p1= hash_entry(a, struct page, hash_elem);
	const struct page *p2= hash_entry(b, struct page, hash_elem);
    
	// 2. va 주소 비교
	return p1->va < p2->va;

}

// SPT 초기화
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED) {
    // hash_init(해시테이블, 해시함수, 비교함수, aux)
    // aux는 NULL로 두면 됨 (지금은 안 씀)
	hash_init(&spt->pages, page_hash, page_less, NULL);
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
  // 1. 임시 page 구조체 만들기 (키로 사용)
  // 2. hash_find()로 찾기 - 해시 테이블에 저장된 struct page들 중에서, va가 일치하는 페이지
  // 3. 결과가 NULL이면 NULL 리턴
  // 4. hash_elem → struct page 변환해서 리턴
  struct page *page = NULL;
  page = malloc(sizeof(struct page));
  struct hash_elem *e;
  
  // va에 해당하는 hash_elem 찾기
  page->va = va;
  e = hash_find(&spt, &page->hash_elem);
  
  // 있으면 e 에 해당하는 페이지 반환
  return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {
  // 1. hash_insert()로 삽입
  // 2. 반환값이 NULL이면 성공 (기존에 없었음)
  // 3. 반환값이 NULL 아니면 실패 (이미 존재)
  return hash_insert(&spt, &page->hash_elem) == NULL ? true : false;
}
