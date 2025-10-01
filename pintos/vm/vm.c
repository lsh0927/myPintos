/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "threads/mmu.h" // pml4_set_page를 위해 추가
#include "vm/uninit.h"
#include "lib/kernel/hash.h"

static void spt_kill_action_func (struct hash_elem *e, void *aux);

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
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
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* 해시 값을 계산, 가상 주소(va) 키로 사용 */
static unsigned
page_hash (const struct hash_elem *p_, void *aux){
	const struct page *p = hash_entry(p_, struct page, hash_elem);		//해시 요소를 통해 시작 주소 가져오기 
	return hash_bytes (&p->va, sizeof p->va);
}

/* 두 해시 요소 비교, 가상 주소(va) 기준 비교 */
static bool
page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux){
	const struct page *a = hash_entry (a_, struct page, hash_elem);
	const struct page *b = hash_entry (b_, struct page, hash_elem);

	return a->va < b->va;
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) != NULL) {
		return false;
	}
	/* TODO: Create the page, fetch the initialier according to the VM type,
		* TODO: and then create "uninit" page struct by calling uninit_new. You
		* TODO: should modify the field after calling the uninit_new. */

	/* TODO: Insert the page into the spt. */
	/* 1. struct page를 위한 메모리 할당 */
	struct page *p = (struct page *)malloc(sizeof(struct page));
	if (p == NULL){
		return false;
	}

	/* 2. uninit_new를 호출하여 uninit 페이지를 생성하고 초기화 */
	uninit_new(p, upage, init, type, aux, NULL);
	p->writable = writable;		//쓰기 가능 여부 설정

	/* 3. 생성된 페이지를 SPT에 삽입 */
	if (!spt_insert_page(spt, p)){
		free(p);	//삽입 실패 시 할당된 메모리 해제
		return false;
	}

	return true;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page p;
	struct hash_elem *e;
	/* TODO: Fill this function. */

	p.va = pg_round_down(va);		//va를 페이지 정렬 주소로 변환하여 검색 키로 사용
	e = hash_find(&spt->pages, &p.hash_elem);	//해시 테이블에서 va에 해당하는 요소 찾기

	/* 요소 찾암ㅆ으면, 해당 요소가 포함도니 page 구조체 포인터 반환, 못찾으면 NULL */
	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
			/* 사입 성공 시 NULL 반환 (== 이전에 같은 키 없었음) */
	return hash_insert(&spt->pages, &page->hash_elem) == NULL;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
/* 사용자 풀(User Pool)에서 새로운 물리 페이지를 가져와 struct frame으로 감싸서 반환하는 역할
   지금은 메모리가 부족한 경우(palloc 실패) 간단히 커널 패닉을 일으키도록 구현
   나중에 메모리가 부족해지면 페이지 교체(eviction) 로직 추가 */
static struct frame *
vm_get_frame (void) {
	/* 1. 사용자 풀에서 페이지 할당 받기 */
	void *kva = palloc_get_page(PAL_USER);
	if (kva == NULL){
		/* TODO: 나중에 페이지 교체(eviction) 로직 여기 구현
		   지금은 단순히 커널 패닉 구현*/
		   PANIC("Out of memory");
	}

	/* 2. 새로운 프레임 구조체를 위한 메모리 할당 */
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));
	if (frame == NULL){
		palloc_free_page(kva);
		PANIC("Failed to allocate frame structure");
	}

	/* 3. 프레임 멤버 초기화 */
	frame->kva = kva;	//커널 가상 주소 설정
	frame->page = NULL;	//아직 어떤 페이지와도 연결되지 않았음

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
		bool user, bool write, bool not_present) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	/* 1. 폴트가 발생한 주소가 유효한지 확인 */
	if (is_kernel_vaddr(addr) || !not_present){	//커널 영역에 대한 접근 시도거나, 존재하지 않음가 아니면 실패 반환
		return false;
	}

	/* 2. SPT에서 페이지를 찾음 */
	page = spt_find_page(spt, addr);
	if (page == NULL){		//SPT에 없다면 유효하지 않은 접근 (나중에 스택 확장 처리 추가)
		return false;
	}

	/* 3. 찾은 페이지를 물리 메모리에 올림 */
	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* vm_get_frame을 호출하여 실제 페이지와 프레임을 연결하는 역할 
   가상 페이지와 연결하고 CPU(MMU)가 알아볼 수 있도록 하드웨어 페이지 테이블에 등록하는 과정
   페이지를 메모리에 올리는(claim) 과정은 두 단계 */
/* Claim the page that allocate on VA. */
/* 가상 주소(va)만 받았을 때 SPT에서 struct page를 찾아 vm_do_claim_page를 호출해주는 헬퍼(helper) 함수 */
bool
vm_claim_page (void *va) {
	/* TODO: Fill this function */
	/* 가상 주소(va)에 해당하는 페이지를 spt에서 찾음 */
	struct page *page = spt_find_page(&thread_current()->spt, va);
	if (page == NULL){
		return false;
	}
	/* 실제 작업 수행 */
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
/* struct page를 받아 실제 물리 프레임을 할당하고 연결하는 핵심 작업을 수행 */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	/* 하드웨어 페이지 테이블(pml4)에 가상 주소(page->va)와 물리 프레임의 커널 가상 주소(frame->kva) 매핑 */
	if (pml4_set_page (thread_current()->pml4, page->va, frame->kva, page->writable)){
		return swap_in (page, frame->kva);
	}

	//매핑 실패 시 프레임 변환
	palloc_free_page(frame->kva);
	free(frame);
	return false;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init (&spt->pages, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst, struct supplemental_page_table *src) {
	// src의 해시테이블을 순회하며 각 엔트리에 대해 copy_spt_entry 호출
	struct hash_iterator i;
	hash_first(&i, &src->pages);
	while(hash_next(&i)){
    	struct page *parent_page = hash_entry (hash_cur (&i), struct page, hash_elem);
    	enum vm_type type = page_get_type(parent_page);
    	void *upage = parent_page->va;
    	bool writable = parent_page->writable;
    
    	/* 1. 부모 페이지의 타입에 따라 자식 페이지를 생성 */
    	if (type == VM_UNINIT) {
        	// uninit 페이지인 경우, 초기화 정보를 그대로 복사하는 약속을 만듦
        	if (!vm_alloc_page_with_initializer(parent_page->uninit.type, upage, writable, parent_page->uninit.init, parent_page->uninit.aux))
            	return false;
		}
     	else { /* VM_ANON 또는 VM_FILE인 경우 */
        	// 해당 타입의 페이지를 생성만 함, 내용은 아래에서 복사
        	if (!vm_alloc_page(type, upage, writable))
            	return false;
    	}

    	/* 2. 부모 페이지가 이미 물리 메모리에 있었다면, 자식도 즉시 프레임을 할당받아 내용을 복사 */
    	if (parent_page->frame) {
        	if (!vm_claim_page(upage)) {
            	return false;
        	}
        	struct page *child_page = spt_find_page(dst, upage);
        	memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
    	}
	}
	return true; // 루프가 모두 성공적으로 끝나면 true 반환
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	/* 해시 테이블의 모든 요소 정리, 각 요소에 대해 spt_kill_action_func를 호출해 페이지 파괴 */
	hash_clear(&spt->pages, spt_kill_action_func);
}

static void spt_kill_action_func (struct hash_elem *e, void *aux){
	struct page *page = hash_entry(e, struct page, hash_elem);
	destroy(page);
}