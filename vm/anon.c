/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "lib/kernel/bitmap.h"
#include "threads/mmu.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
/* Project 3 */
static struct bitmap *disk_bitmap;
static struct lock bitmap_lock;

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

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	// disk_init();
	// 스왑 디스크 세팅
	swap_disk = disk_get(1,1);
	// 디스크 사이즈만큼 비트맵 생성
 	disk_bitmap = bitmap_create(disk_size(swap_disk)/SECTORS_PER_PAGE);
	// 비트맵 락 초기화
	lock_init(&bitmap_lock);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;

	anon_page->anon_swap_sector_no = SIZE_MAX;
	anon_page->thread = thread_current();

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;

	// 유효값 검사
	if(anon_page->anon_swap_sector_no == SIZE_MAX) return false;

	lock_acquire(&bitmap_lock);
	// anon_swap_sector_no ~ disk_size 까지의 범위 안에 비트 값이 음수인 비트가 있는지 검사
	bool check = bitmap_contains(disk_bitmap, anon_page->anon_swap_sector_no, 8, false);
	lock_release(&bitmap_lock);

	// 하나라도 쓰여지지 않은 디스크가 있다면 실패
	if(check){
		return false;
	}

	// sector no ~ 마지막 섹터 까지 모든 섹터 읽음
	for (int i = 0; i < 8; i++)
	{	
		disk_read(swap_disk, anon_page->anon_swap_sector_no + i, kva + i * DISK_SECTOR_SIZE);
	}
	
	// 모든 섹터를 읽은 후 다시 0으로 초기화 해줌
	// 진짜 이해가 안되는데 왜 전부 다 셋 해주는지 모르겠다.
	lock_acquire(&bitmap_lock);
	bitmap_set_multiple(disk_bitmap, anon_page->anon_swap_sector_no, 8, false);
	lock_release(&bitmap_lock);

	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	// 디스크 스왑 아웃 
	// 물리 메모리 공간은 유지해야 한다.
	// 즉 매핑된 물리 메모리에 가상메모리에 있는 데이터를 써줘야 한다? 어떻게?
	// 특정한 물리 메모리에 매핑되어 있는, 모든 데이터와 이전 주소를 가진 테이블
	// 스왑 테이블. 1이 둘중 하나인것 같은데 잘 모르겠음
	// 1-. 초기화시 init에서 이미 초기화 된 스왑 테이블을 anon page에 연결
	// 1=. 초기화시 init에서 스왑 디스크를 초기화하고 그걸 anon page에 연결
	// 2.
	// 1. 디스크를 스캔하여 0, 즉 할당안된 비트들을 찾고 
	// 2. 비트를 반전시킨 후
	// 3. 그중 첫 번째 비트의 인덱스를 반환한다. 그룹이 없을경우 bitmap err 반환
	// 왜 반전시켜야 하나?
	lock_acquire(&bitmap_lock);
	disk_sector_t sec_no = (disk_sector_t)bitmap_scan_and_flip(disk_bitmap,0,SECTORS_PER_PAGE, false);
	lock_release(&bitmap_lock);

	if(sec_no == BITMAP_ERROR) return false;

	anon_page->anon_swap_sector_no = sec_no;
	// 모든 디스크를 순회하며, 디스크에 써준다 
	// 왜 모든 디스크에 써줄까?
	// 스왑 아웃 할때마다 모든 디스크에 써주면 오히려 낭비가 아닌가?
	for (int i = 0; i < SECTORS_PER_PAGE; ++i)
	{
		disk_write(swap_disk, sec_no + i , page->frame->kva + i * DISK_SECTOR_SIZE);
	}

	// 디스크에 썼으니 해당 페이지 할당 해제
	pml4_clear_page(anon_page->thread->pml4, page->va);
	// 스왑 아웃하면서 dirty 비트 초기화
	pml4_set_dirty(anon_page->thread->pml4, page->va, false);
	page->frame = NULL;

	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	if(page->frame != NULL){
		lock_acquire(&frame_lock);
		list_remove(&page->frame->f_elem);
		lock_release(&frame_lock);
		free(page->frame);
	}
	if(anon_page->anon_swap_sector_no != SIZE_MAX)
		bitmap_set_multiple(disk_bitmap, anon_page->anon_swap_sector_no, SECTORS_PER_PAGE, false);
}
