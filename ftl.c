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

void gc(); // 前向声明

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
	printf("V2.0 Init Done.\n");
}

int write_data(int lba, int data)
{
	int found = 0;
	int global_page = -1;

	// 寻找空闲页
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

	// 【改变】：返回 -1 静默失败，不再直接打屏幕报错，交由 main 处理
	if (global_page == -1)
		return -1;

	// 写新覆旧
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

	// 更新映射表
	l2p_table[lba] = global_page;
	return 0;
}

void gc()
{
	int victim = -1;
	int min_cost = 999999;

	// 【新特性：动态磨损均衡打分】
	for (int i = 0; i < TOTAL_BLOCKS; i++)
	{
		int has_garbage = 0;
		for (int p = 0; p < PAGES_PER_BLOCK; p++)
			if (nand[i].pages[p].state == INVALID)
				has_garbage = 1;

		if (has_garbage)
		{
			// 核心公式：有效页越少，EC越小，代价越低，越容易被选中
			int cost = nand[i].valid_count + (nand[i].erase_count * 2);
			if (cost < min_cost)
			{
				min_cost = cost;
				victim = i;
			}
		}
	}
	if (victim == -1)
		return;

	// 暂存有效页
	int temp_lbas[PAGES_PER_BLOCK];
	int temp_datas[PAGES_PER_BLOCK];
	int count = 0;

	for (int p = 0; p < PAGES_PER_BLOCK; p++)
	{
		if (nand[victim].pages[p].state == VALID)
		{
			temp_datas[count] = nand[victim].pages[p].data;
			int old_gp = victim * PAGES_PER_BLOCK + p;

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

	// 【致命漏洞在此】：擦除块之前，忘记清空旧的 l2p_table 映射了！

	// 擦除块
	for (int p = 0; p < PAGES_PER_BLOCK; p++)
	{
		nand[victim].pages[p].state = FREE;
		nand[victim].pages[p].data = -1;
	}
	nand[victim].valid_count = 0;
	nand[victim].erase_count++;

	// 写回
	for (int i = 0; i < count; i++)
	{
		write_data(temp_lbas[i], temp_datas[i]);
	}
}

int main()
{
	srand(time(NULL));
	init();

	printf("--- Start V2.0 Brutal Write Test ---\n");
	for (int i = 0; i < 100; i++)
	{
		int random_lba = rand() % LOGICAL_PAGES;
		int retry = 0;

		// 【新特性：自动触发前台 GC】
		while (write_data(random_lba, i * 10) == -1)
		{
			gc();
			if (retry++ > 100)
			{
				printf("Deadlock!\n");
				return -1;
			}
		}
	}

	printf("\n--- Final Read Verification ---\n");
	// 跑完后验证，会发现数据出现丢失或无效
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
			if (nand[b].pages[p].state == VALID)
			{
				printf("LBA %d: Data = %d\n", i, nand[b].pages[p].data);
			}
			else
			{
				printf("LBA %d: ERROR! Data is INVALID!\n", i); // 这里会报错
			}
		}
	}
	return 0;
}