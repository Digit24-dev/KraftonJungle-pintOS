/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/mmu.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Swap Table */
struct list swap_table;

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	list_init(&swap_table);
	//1.1채널 (= 스왑디스크 용도)로 가져옴
	swap_disk = disk_get(1, 1);
	const disk_sector_t max_sector_size = disk_size(swap_disk);
	//스왑 디스크를 8개의 섹터씩 그룹으로 초기화
	for (int i = 0; i < max_sector_size; i += 8)
	{
		struct swap_anon *swap_anon = malloc(sizeof(struct swap_anon));
		swap_anon->page = NULL;
		swap_anon->use = false;
		/*
			i = 0 swap_anon1|0 1 2 3 4 5 6 7|
			i = 1 swap_anon2|1 2 3 4 5 6 7 8|
						...
			i = max_sector_size-1
		*/
		for (int j = 0; j < 8; j++)
			swap_anon->sectors[j] = i + j;
		
		list_push_back(&swap_table, &swap_anon->s_elem);
	}
}

//swap_table에서 사용중이지 않는 페이지를 찾아 리턴
static struct swap_anon*
find_swap_table() {
	struct list_elem *e = list_begin(&swap_table);
	while (e != list_end(&swap_table))
	{
		struct swap_anon *temp_page = list_entry(e, struct swap_anon, s_elem);
		if (!temp_page->use)
			return temp_page;
		e = list_next(e);
	}
	return NULL;
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
}

/* Swap in the page by read contents from the swap disk. */
/* 스왑 디스크에서 내용을 읽어 페이지를 스왑 인합니다. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	struct swap_anon *swap_anon = anon_page->swap_anon;
	//swap_table에서 8개의 섹터를 읽어와 kva에 해당하는 page에 로드함
	for (size_t i = 0; i < 8; i++) {
		disk_read(swap_disk, swap_anon->sectors[i], kva + DISK_SECTOR_SIZE * i);
	}
	//swap_in으로 인해 swap_table에서 나갔으니
	//나간놈의 자리 초기화
	anon_page->swap_anon = NULL;
	swap_anon->use = false;

	return true;
}

/* Swap out the page by writing contents to the swap disk. */
/* 내용을 스왑 디스크에 작성하여 페이지를 스왑 아웃합니다. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	//find_swap_table()에서 사용중이지 않는 anon_page 1개를 들고옴
	struct swap_anon *swap_anon = find_swap_table();
	// 8개의 섹터에 페이지 데이터를 스왑 디스크에 씀
	for (int i = 0; i < 8; i++) {
		disk_write(swap_disk, swap_anon->sectors[i], page->frame->kva + DISK_SECTOR_SIZE * i);
	}
	/*
		swap_table에 들어왔으니 사용중으로 바꾸고
		anon_page의 swap_anon정보 최신화 해주고
		anon_page의 frame을 NULL로 swap_out으로 물리 메로리에 나왔으니
		그리고 그 쓰레드에 pml4테이블에 자기 자신 날림
	*/
	swap_anon->use = true;
	anon_page->swap_anon = swap_anon;
	page->frame = NULL;
	pml4_clear_page(thread_current()->pml4, page->va);
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
/* 익명 페이지를 삭제합니다. PAGE는 호출자에 의해 해제됩니다. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	// list_remove(&page->frame->f_elem);
}
