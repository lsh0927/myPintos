/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */

#include "vm/vm.h"
#include "vm/uninit.h"
#include <string.h>    // memset을 위해 추가
#include "vm/anon.h"   // anon_initializer를 위해 추가
#include "vm/file.h"   // file_initializer를 위해 추가
#include "threads/vaddr.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
void
uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);

	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* no frame for now */
		.uninit = (struct uninit_page) {
			.init = init,
			.type = type,
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}

/* uninit 페이지를 복제하는 함수, fork 시 사용 */
bool
uninit_copy (struct page *dst, struct page *src) {
	struct uninit_page *src_uninit = &src->uninit;
	
	vm_initializer *init = src_uninit->init;
	void *aux = src_uninit->aux;

	// src의 초기화 정보를 그대로 사용하여 dst를 uninit 페이지로 설정
	uninit_new(dst, src->va, init, src_uninit->type, aux, NULL);
	dst->writable = src->writable;
	return true;
}

/* Initalize the page on first fault */
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit;

	/* Fetch first, page_initialize may overwrite the values */
	vm_initializer *init = uninit->init;
	enum vm_type type = uninit->type;
	void *aux = uninit->aux;

	/* TODO: You may need to fix this function. */
	/* 페이지 구조체에서 uninit 관련 정보를 제거합니다. */
	memset(uninit, 0, sizeof(struct uninit_page));

	/* 1. 페이지 타입을 변경(transmute)합니다. */
	switch (VM_TYPE(type)) {
		case VM_ANON:
			if (!anon_initializer (page, type, kva))
				return false;
			break;
		case VM_FILE:
			if (!file_backed_initializer (page, type, kva))
				return false;
			break;
		default:
			 // VM_PAGE_CACHE 등
			 break;
	}

	/* 2. 타입 변경 후, 페이지 내용을 채우는 초기화 함수를 호출합니다. */
	return init ? init (page, aux) : true;
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit UNUSED = &page->uninit;
	/* TODO: Fill this function.
	 * TODO: If you don't have anything to do, just return. */
}
