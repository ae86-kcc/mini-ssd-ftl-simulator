# mini-ssd-ftl-simulator
A lightweight SSD FTL simulator in C

# 未来规划
*   性能优化：将空闲页分配由 O(N) 遍历替换为空闲块链表，将 GC 牺牲块选择替换为最小堆，降低算法时间复杂度。
*   元数据优化：引入反向映射表 (P2L)或有效页位图，加速 GC 搬移时查找对应 LBA 的效率。
*   特性支持：模拟实现TRIM 指令以降低写放大 (WAF)，并引入SLC 缓存机制模拟真实缓内/缓外性能差异。
