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
	int erase_count; // 为后续的磨损均衡做准备
} Block;

Block nand[TOTAL_BLOCKS];
int l2p_table[LOGICAL_PAGES];

void init()
{
	for (int i = 0; i < TOTAL_BLOCKS; i++)
	{
		nand[i].valid_count = 0;
		nand[i].erase_count = 0;
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
	printf("V1.1 Init Done.\n");
}

void write_data(int lba, int data)
{
	int found = 0;
	int global_page = -1;

	// 1. 寻找空闲页
	for (int i = 0; i < TOTAL_BLOCKS; i++)
	{
		for (int j = 0; j < PAGES_PER_BLOCK; j++)
		{
			if (nand[i].pages[j].state == FREE)
			{
				nand[i].pages[j].state = VALID;
				nand[i].pages[j].data = data;
				nand[i].valid_count++;
				global_page = i * PAGES_PER_BLOCK + j;
				found = 1;
				break;
			}
		}
		if (found)
			break;
	}

	// 2. 空间不足判断（V1.1依然需要手动触发GC）
	if (global_page == -1)
	{
		printf("[ERROR] Disk full! Cannot write LBA %d.\n", lba);
		return;
	}

	// 3. 写新覆旧
	int old_p = l2p_table[lba];
	if (old_p != -1)
	{
		int old_block = old_p / PAGES_PER_BLOCK;
		int old_page = old_p % PAGES_PER_BLOCK;
		nand[old_block].pages[old_page].state = INVALID;
		nand[old_block].valid_count--;
	}

	// 4. 更新映射表
	l2p_table[lba] = global_page;
}

// ==========================================
// 新增：垃圾回收 (Garbage Collection)
// ==========================================
void gc()
{
	printf("\n--- Triggering GC ---\n");
	int victim = -1;
	int min_valid = PAGES_PER_BLOCK;

	// 【步骤一：挑目标】找无效数据（垃圾）最多，即有效页最少的块
	for (int i = 0; i < TOTAL_BLOCKS; i++)
	{
		int has_garbage = 0;
		for (int p = 0; p < PAGES_PER_BLOCK; p++)
		{
			if (nand[i].pages[p].state == INVALID)
			{
				has_garbage = 1;
				break;
			}
		}
		if (has_garbage && nand[i].valid_count < min_valid)
		{
			min_valid = nand[i].valid_count;
			victim = i;
		}
	}

	if (victim == -1)
	{
		printf("  [GC] No garbage found.\n");
		return;
	}

	printf("  [GC] Selected Block %d as victim (Valid: %d, Garbage: %d)\n",
		   victim, nand[victim].valid_count, PAGES_PER_BLOCK - nand[victim].valid_count);

	// 【步骤二：搬家（存到RAM）】把有效页暂存到内存，解决物理重叠问题
	int temp_lbas[PAGES_PER_BLOCK];
	int temp_datas[PAGES_PER_BLOCK];
	int count = 0;

	for (int p = 0; p < PAGES_PER_BLOCK; p++)
	{
		if (nand[victim].pages[p].state == VALID)
		{
			temp_datas[count] = nand[victim].pages[p].data;
			int old_gp = victim * PAGES_PER_BLOCK + p;

			// 反向查找：这个物理页原来存的是哪个 LBA 的数据？
			for (int l = 0; l < LOGICAL_PAGES; l++)
			{
				if (l2p_table[l] == old_gp)
				{
					temp_lbas[count] = l;
					break;
				}
			}
			count++;
		}
	}

	// 【步骤三：擦除垃圾块】把旧块全部清空（此时数据在RAM里安全暂存）
	for (int p = 0; p < PAGES_PER_BLOCK; p++)
	{
		nand[victim].pages[p].state = FREE;
		nand[victim].pages[p].data = -1;
	}
	nand[victim].valid_count = 0;
	nand[victim].erase_count++;
	printf("  [GC] Block %d has been erased.\n", victim);

	// 【步骤四：写回】把RAM里的数据写回新擦除出来的 FREE 块
	for (int i = 0; i < count; i++)
	{
		printf("  [GC] Restoring LBA %d (Data: %d)...\n", temp_lbas[i], temp_datas[i]);
		write_data(temp_lbas[i], temp_datas[i]);
	}
	printf("--- GC Finished ---\n\n");
}

int main()
{
	init();

	printf("--- Start V1.1 Write Test ---\n");
	// 模拟不停地覆盖写，产生大量垃圾
	for (int i = 0; i < 15; i++)
	{
		write_data(0, i * 100);
		write_data(1, i * 100 + 1);
		write_data(2, i * 100 + 2);
		write_data(3, i * 100 + 3);
	}

	// 此时盘已经满了，由于V1.1没有自动触发GC，需要手动触发
	gc();

	printf("\n--- Write after GC ---\n");
	write_data(4, 4000); // GC之后，应该能成功写入了

	return 0;
}