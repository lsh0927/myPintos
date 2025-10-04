/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
// #include <cstdlib>

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
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux) {

	ASSERT(VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current()->spt;
	upage = pg_round_down(upage);

	/* Check wheter the upage is already occupied or not. */
	if(spt_find_page(spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
    
    // 페이지 생성
    // struct page *p = (struct page *)malloc(sizeof(struct page));
		struct page *p = malloc(sizeof *p);
		if(p == NULL) { return false; }

    // type에 따라 초기화 함수를 가져옴
    bool (*page_initializer)(struct page *, enum vm_type, void *);

    switch(VM_TYPE(type)) {
      case VM_ANON:
        page_initializer = anon_initializer;
        break;
      case VM_FILE:
        page_initializer = file_backed_initializer;
        break;
			default:
				free(p);
				return false;
    }

    // uninit 타입의 페이지로 초기화
    uninit_new(p, upage, init, type, aux, page_initializer);

    // 필드 수정은 uninit_new를 호출한 이후에 해야 함
    p->writable = writable;

    // 생성한 페이지를 SPT에 추가
    // return spt_insert_page(spt, p);
		if(!spt_insert_page(spt, p)) {
			free(p);
			return false;
		}
		return true;
	}
// err:
	return false;
}

void spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	// vm_dealloc_page (page);
	// return true;

	hash_delete(&spt->spt_hash_table, &page->hash_elem);
	vm_dealloc_page(page);
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
  void *kva = palloc_get_page(PAL_USER);
  
  // page 할당 실패 -> 나중에 swap_out 처리
  // OS 중시, 소스 파일명, 라인 번호, 함수명 등의 정보와 함께 사용자 지정 메시지를 출력
  if (kva == NULL) {
    PANIC("todo");
  }
  
  // 프레임 할당
  // frame = malloc(sizeof(struct frame));
	frame = malloc(sizeof *frame);
	if (frame == NULL) { PANIC("frame alloc failed"); }
  // 프레임 멤버 초기화
  frame->kva = kva;
	frame->page = NULL;

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

	if(addr == NULL) { return false; }

	if(is_kernel_vaddr(addr)) { return false; }

	// 접근한 메모리의 physical page가 존재하지 않은 경우
	if(not_present) {
		page = spt_find_page(spt, addr);
		if(page == NULL) { return false; }
		if(write == 1 && page->writable == 0) { return false; }
		return vm_do_claim_page(page);
	}

	return false;
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
static bool vm_do_claim_page(struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
  // 가상 주소와 물리 주소를 매핑
  // struct thread *current = thread_current();
  // bool writable = (page->uninit.type & VM_MARKER_0) != 0;
  // pml4_set_page(current->pml4, page->va, frame->kva, writable);
	
  // return swap_in(page, frame->kva);

	struct thread *current = thread_current();
	bool ok = pml4_set_page(current->pml4, page->va, frame->kva, page->writable);
	ASSERT(ok);
	return swap_in(page, frame->kva);

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
// 새로운 프로세스가 시작되면(process.c:initd)와 프로세스 fork 시에 process.c:__do_fork 호출
// 빠른 검색을 위해 해쉬 테이블을 사용하지만, 다른 자료구조를 사용하는 경우도 많음
// 예를 들어서, 균형 잡힌 탐색 / 정렬 이 필요한 경우는 RB-Tree를 사용하는 경우도 있음
// void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED) {
// 	hash_init(spt, page_hash, page_less, NULL);
// }
void supplemental_page_table_init(struct supplemental_page_table *spt) {
	/* SPT 내부의 해시 테이블을 정확히 초기화 */
	hash_init(&spt->spt_hash_table, page_hash, page_less, NULL);
}

// SPT에서 va에 해당하는 구조체 페이지를 찾아 반환
// struct page *spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
//   // 1. 임시 page 구조체 만들기 (키로 사용)
//   // 2. hash_find()로 찾기 - 해시 테이블에 저장된 struct page들 중에서, va가 일치하는 페이지
//   // 3. 결과가 NULL이면 NULL 리턴
//   // 4. hash_elem → struct page 변환해서 리턴
//   struct page *page = NULL;
//   page = malloc(sizeof(struct page));
//   struct hash_elem *e;
  
//   // va에 해당하는 hash_elem 찾기
//   page->va = va;
//   e = hash_find(&spt, &page->hash_elem);
  
//   // 있으면 e 에 해당하는 페이지 반환
//   return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
// }
struct page *spt_find_page(struct supplemental_page_table *spt, void *va) {
  struct page key;
  struct hash_elem *e;
  /* 주소는 항상 페이지 경계로 맞춰서 검색 */
  key.va = pg_round_down(va);
  e = hash_find(&spt->spt_hash_table, &key.hash_elem);
  return e ? hash_entry(e, struct page, hash_elem) : NULL;
}

// bool spt_insert_page(struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {
//   // 1. hash_insert()로 삽입
//   // 2. 반환값이 NULL이면 성공 (기존에 없었음)
//   // 3. 반환값이 NULL 아니면 실패 (이미 존재)
//   return hash_insert(&spt, &page->hash_elem) == NULL ? true : false;
// }

bool spt_insert_page(struct supplemental_page_table *spt, struct page *page) {
  return hash_insert(&spt->spt_hash_table, &page->hash_elem) == NULL;
}
