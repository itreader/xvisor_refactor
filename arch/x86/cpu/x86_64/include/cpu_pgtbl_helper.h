/*
 * Copyright (c) 2010-2020 Himanshu Chauhan.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @author: Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief: Generic pagetable handling defines and externs.
 */

#ifndef __CPU_PAGE_TABLE_HELPER_H_
#define __CPU_PAGE_TABLE_HELPER_H_
#include <arch_types.h>
#include "vmm_spinlocks.h"

struct page_table;

/* Note: we use 1/8th or 12.5% of VIRTUAL_ADDR_POOL memory as page table pool.
 * For example if VIRTUAL_ADDR_POOL is 8 MB then page table pool will be 1 MB
 * or 1 MB / 4 KB = 256 page tables
 */
#define PAGE_TABLE_FIRST_LEVEL 0
#define PAGE_TABLE_LAST_LEVEL  3
#define PAGE_TABLE_SIZE_SHIFT  12
#define PAGE_TABLE_SIZE        4096
#define PAGE_TABLE_ENTCNT      512

/* Stage 2 is for shadow */
#define PAGE_TABLE_STAGE_1     0
#define PAGE_TABLE_STAGE_2     1

/**
 * 页表控制器结构体
 * 用于统一管理系统页表的分配、释放、基址映射及各级页表节点
 * 适配四级页表模型（PML4 -> PDPT -> PDT -> PT）
 */
struct page_table_ctrl {
    /**
     * @base_page_table: 系统根页表的指针
     * 指向当前正在使用的顶级页表（通常是PML4级），作为页表操作的入口
     */
    struct page_table *base_page_table;

    /**
     * @page_table_base_va: 页表基地址的虚拟地址
     * 页表自身在虚拟地址空间中的起始地址，供CPU通过虚拟地址访问页表
     */
    virtual_addr_t page_table_base_va;

    /**
     * @page_table_base_pa: 页表基地址的物理地址
     * 页表在物理内存中的起始地址，写入寄存器供MMU硬件寻址使用
     */
    physical_addr_t page_table_base_pa;

    /**
     * @page_table_array: 页表项数组指针
     * 指向预分配的页表结构体数组，用于批量管理页表项的内存
     */
    struct page_table *page_table_array;

    /**
     * @page_table_pml4: PML4级页表（Page Map Level 4）
     * x86_64四级页表的顶级页表，对应寄存器指向的页表结构
     */
    struct page_table page_table_pml4;

    /**
     * @page_table_pgdp: PDPT级页表（Page Directory Pointer Table）
     * 页目录指针表，作为PML4的下一级页表，用于映射PDT
     */
    struct page_table page_table_pgdp;

    /**
     * @page_table_pgdi: PDT级页表（Page Directory Table）
     * 页目录表，作为PDPT的下一级页表，用于映射PT
     */
    struct page_table page_table_pgdi;

    /**
     * @page_table_pgti: PT级页表（Page Table）
     * 最末级页表，直接映射物理内存页框
     */
    struct page_table page_table_pgti;

    /**
     * @alloc_lock: 页表分配自旋锁
     * 保护页表分配/释放操作的原子性，防止多CPU核并发访问冲突
     */
    vmm_spinlock_t alloc_lock;

    /**
     * @page_table_alloc_count: 已分配的页表数量
     * 记录当前系统中已分配的页表项总数，用于内存使用统计
     */
    uint32_t page_table_alloc_count;

    /**
     * @page_table_max_size: 页表总容量（字节数）
     * 配置的页表内存池最大总大小，超过该值则无法分配新页表
     */
    uint64_t page_table_max_size;

    /**
     * @page_table_size_shift: 页表大小移位值
     * 用于快速计算页表大小（size = 1 << shift），替代乘法提升效率
     * 例如shift=12表示页表大小为4KB（2^12）
     */
    uint32_t page_table_size_shift;

    /**
     * @page_table_max_count: 页表最大数量
     * 系统支持的最大页表项数量上限，与硬件/内存容量匹配
     */
    uint32_t page_table_max_count;

    /**
     * @free_page_table_list: 空闲页表链表
     * 管理未使用的页表项，采用双链表结构便于快速分配/回收
     */
    double_list_t free_page_table_list;
};

static inline physical_addr_t mmu_level_map_mask(int level)
{
    switch (level) {
        case 0:
            return PML4_MAP_MASK;

        case 1:
            return PGDP_MAP_MASK;

        case 2:
            return PGDI_MAP_MASK;

        default:
            break;
    };

    return PGTI_MAP_MASK;
}

static inline int mmu_level_index(physical_addr_t ia, int level)
{
    switch (level) {
        case 0:
            return (ia >> PML4_SHIFT) & ~PGTREE_MASK;

        case 1:
            return (ia >> PGDP_SHIFT) & ~PGTREE_MASK;

        case 2:
            return (ia >> PGDI_SHIFT) & ~PGTREE_MASK;

        default:
            break;
    };

    return (ia >> PGTI_SHIFT) & ~PGTREE_MASK;
}

extern int                mmu_get_page(struct page_table_ctrl *ctrl, struct page_table *page_table, physical_addr_t ia, union page *pg);
extern int                mmu_unmap_page(struct page_table_ctrl *ctrl, struct page_table *page_table, physical_addr_t ia);
extern int                mmu_map_page(struct page_table_ctrl *ctrl, struct page_table *page_table, physical_addr_t ia, union page *pg);
extern struct page_table *mmu_page_table_alloc(struct page_table_ctrl *ctrl, int stage);
extern int                mmu_page_table_free(struct page_table_ctrl *ctrl, struct page_table *page_table);

#endif /* __CPU_PAGE_TABLE_HELPER_H_ */
