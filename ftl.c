#include <stdio.h>
#include <stdlib.h>

#define TOTAL_BLOCKS 10
#define PAGES_PER_BLOCK 4
#define LOGICAL_PAGES 20

typedef enum
{
	FREE,
	VALID,
	INVALID
} PageState;

typedef struct
{
	int data;
	PageState state;
} Page;

typedef struct
{
	Page pages[PAGES_PER_BLOCK];
	int valid_count;
} Block;

Block nand[TOTAL_BLOCKS];
int l2p_table[LOGICAL_PAGES]; // L2P 映射表：下标是LBA，值是全局物理页号

void init()
{
	for (int i = 0; i < TOTAL_BLOCKS; i++)
	{
		nand[i].valid_count = 0;
		for (int j = 0; j < PAGES_PER_BLOCK; j++)
		{
			nand[i].pages[j].state = FREE;
			nand[i].pages[j].data = -1;
		}
	}
	for (int i = 0; i < LOGICAL_PAGES; i++)
	{
		l2p_table[i] = -1;
	}
	printf("V1.0 Init Done.\n");
}

void write_data(int lba, int data)
{
	int found = 0;
	int global_page = -1;

	// 1. 寻找空闲页 (从头到尾找 FREE 的页)
	for (int i = 0; i < TOTAL_BLOCKS; i++)
	{
		for (int j = 0; j < PAGES_PER_BLOCK; j++)
		{
			if (nand[i].pages[j].state == FREE)
			{
				nand[i].pages[j].state = VALID;
				nand[i].pages[j].data = data;
				nand[i].valid_count++;

				// 计算全局页号
				global_page = i * PAGES_PER_BLOCK + j;
				found = 1;
				break;
			}
		}
		if (found)
			break;
	}

	// 2. 如果没找到空闲页，报错返回（V1.0 没有垃圾回收，写满就死）
	if (global_page == -1)
	{
		printf("[ERROR] Disk full! Cannot write LBA %d.\n", lba);
		return;
	}

	// 3. 写新覆旧：如果这个 LBA 以前写过，把旧位置标记为 INVALID (垃圾)
	int old_p = l2p_table[lba];
	if (old_p != -1)
	{
		int old_block = old_p / PAGES_PER_BLOCK;
		int old_page = old_p % PAGES_PER_BLOCK;
		nand[old_block].pages[old_page].state = INVALID; // 旧数据变垃圾
		nand[old_block].valid_count--;					 // 有效页减1
		printf("  [Update] Old data of LBA %d becomes garbage.\n", lba);
	}

	// 4. 更新映射表，指向新位置
	l2p_table[lba] = global_page;
	printf("[Write] LBA %d -> Phys Page %d, Data: %d\n", lba, global_page, data);
}

int main()
{
	init();

	printf("\n--- Start V1.0 Write Test ---\n");
	// 模拟连续写入
	write_data(0, 100); // LBA0 写入 100
	write_data(1, 200); // LBA1 写入 200
	write_data(0, 999); // LBA0 覆盖写 999 (此时原来的 100 变成垃圾)

	return 0;
}