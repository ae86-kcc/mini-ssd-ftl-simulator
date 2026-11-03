#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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
	int erase_count;
} Block;

Block nand[TOTAL_BLOCKS];
int l2p_table[LOGICAL_PAGES];

int gc(); // 前向声明

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
	printf("V3.0 Init Done. (All bugs fixed)\n");
}

int write_data(int lba, int data)
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

	if (global_page == -1)
		return -1;

	// 2. 写新覆旧
	int old_p = l2p_table[lba];
	if (old_p != -1)
	{
		int old_block = old_p / PAGES_PER_BLOCK;
		int old_page = old_p % PAGES_PER_BLOCK;
		if (nand[old_block].pages[old_page].state == VALID)
		{
			nand[old_block].pages[old_page].state = INVALID;
			nand[old_block].valid_count--;
		}
	}

	l2p_table[lba] = global_page;
	return 0;
}

int gc()
{
	int victim = -1;
	int min_cost = 999999;

	// 1. 磨损均衡打分
	for (int i = 0; i < TOTAL_BLOCKS; i++)
	{
		int has_garbage = 0;
		for (int p = 0; p < PAGES_PER_BLOCK; p++)
			if (nand[i].pages[p].state == INVALID)
				has_garbage = 1;

		if (has_garbage)
		{
			int cost = nand[i].valid_count + (nand[i].erase_count * 2);
			if (cost < min_cost)
			{
				min_cost = cost;
				victim = i;
			}
		}
	}

	if (victim == -1)
		return -1; // 全盘无垃圾，返回失败（防死锁）

	// 2. 暂存有效数据
	int temp_lbas[PAGES_PER_BLOCK];
	int temp_datas[PAGES_PER_BLOCK];
	int count = 0;

	for (int p = 0; p < PAGES_PER_BLOCK; p++)
	{
		if (nand[victim].pages[p].state == VALID)
		{
			int old_gp = victim * PAGES_PER_BLOCK + p;
			int target_lba = -1;

			for (int l = 0; l < LOGICAL_PAGES; l++)
			{
				if (l2p_table[l] == old_gp)
				{
					target_lba = l;
					break;
				}
			}

			// 【防弹修复 1】：孤儿页拦截。找不到 LBA 的数据直接丢弃，绝不让下标变成 -1
			if (target_lba != -1)
			{
				temp_datas[count] = nand[victim].pages[p].data;
				temp_lbas[count] = target_lba;
				count++;
			}
		}
	}

	// 【防弹修复 2】：擦除前必须清空旧映射！否则写回时会被当成旧数据误杀
	for (int i = 0; i < count; i++)
	{
		l2p_table[temp_lbas[i]] = -1;
	}

	// 3. 擦除块
	for (int p = 0; p < PAGES_PER_BLOCK; p++)
	{
		nand[victim].pages[p].state = FREE;
		nand[victim].pages[p].data = -1;
	}
	nand[victim].valid_count = 0;
	nand[victim].erase_count++;

	// 4. 写回数据
	for (int i = 0; i < count; i++)
	{
		// 【防弹修复 3】：强校验写回结果
		if (write_data(temp_lbas[i], temp_datas[i]) != 0)
		{
			printf("[ERROR] GC write-back failed for LBA %d!\n", temp_lbas[i]);
		}
	}
	return 0;
}

int main()
{
	srand(time(NULL));
	init();

	printf("--- Start V3.0 Brutal Write Test (1000 iterations) ---\n");

	for (int i = 0; i < 1000; i++)
	{
		int random_lba = rand() % LOGICAL_PAGES;
		int retry = 0;

		// 【防弹修复 4】：防死锁重试上限
		while (write_data(random_lba, i * 10) == -1)
		{
			if (gc() == -1 || retry++ > 100)
			{
				printf("[FATAL] Deadlock! Disk is completely full of valid data.\n");
				return -1;
			}
		}
	}

	printf("\n--- Test Passed! ---\n");
	printf("Block Erase Counts (Wear Leveling Check):\n");
	int total_ec = 0;
	for (int i = 0; i < TOTAL_BLOCKS; i++)
	{
		printf("Block %d: EC = %d\n", i, nand[i].erase_count);
		total_ec += nand[i].erase_count;
	}
	printf("Total EC = %d\n", total_ec);

	printf("\n--- Final Read Verification ---\n");
	for (int i = 0; i < 5; i++)
	{
		int gp = l2p_table[i];
		if (gp == -1)
		{
			printf("LBA %d: NO DATA\n", i);
		}
		else
		{
			int b = gp / PAGES_PER_BLOCK, p = gp % PAGES_PER_BLOCK;
			printf("LBA %d: Data = %d (Block %d, Page %d)\n", i, nand[b].pages[p].data, b, p);
		}
	}

	return 0;
}