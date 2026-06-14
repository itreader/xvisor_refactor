/**
 * Copyright (c) 2020 Anup Patel.
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
 * @file generic_mmu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic MMU interface header
 */

#ifndef __GENERIC_MMU_H__
#define __GENERIC_MMU_H__

#include <arch_mmu.h>
#include <libs/list.h>
#include <vmm_spinlocks.h>

/** MMU page/block */
struct mmu_page {
    physical_addr_t   ia;
    physical_addr_t   oa;
    physical_size_t   size;
    arch_page_flags_t flags;
};

/** MMU stages */
enum mmu_stage {
    MMU_STAGE_UNKNOWN = 0,
    MMU_STAGE1,
    MMU_STAGE2,
    MMU_STAGE_MAX
};

/** MMU page table attributes */
#define MMU_ATTR_REMOTE_TLB_FLUSH (1 << 0)
#define MMU_ATTR_HW_TAG_VALID     (1 << 1)

/** MMU page table */
struct mmu_page_table {
    double_list_t          head;
    struct mmu_page_table *parent;
    enum mmu_stage         stage;
    int                    level;
    uint32_t               attr;
    uint32_t               hw_tag;
    physical_addr_t        map_ia;
    physical_addr_t        table_phy_addr;/* 页表起始地址 */
    vmm_spinlock_t         table_lock; /*< Lock to protect table contents,
                              pte_count, child_count, and child_list
                          */
    virtual_addr_t         table_va;
    virtual_size_t         table_size;
    uint32_t               pte_count;
    uint32_t               child_count;
    double_list_t          child_list;
};

/* MMU stage1 page table symbols
 *
 * Note: Architecture specific initial page table setup can use this symbols
 * as hypervisor (i.e. stage1) page table.
 */
extern uint8_t stage1_page_table_root[];
extern uint8_t stage1_page_table_nonroot[];

uint64_t mmu_page_table_count(int stage, int level);

struct mmu_page_table *mmu_page_table_find(int stage, physical_addr_t table_phy_addr);

struct mmu_page_table *mmu_page_table_alloc(int stage, int level, uint32_t attr, uint32_t hw_tag);

int mmu_page_table_free(struct mmu_page_table *page_table);

static inline enum mmu_stage mmu_page_table_stage(struct mmu_page_table *page_table)
{
    return (page_table) ? page_table->stage : MMU_STAGE_UNKNOWN;
}

static inline int mmu_page_table_level(struct mmu_page_table *page_table)
{
    return (page_table) ? page_table->level : -1;
}

static inline bool mmu_page_table_need_remote_tlbflush(struct mmu_page_table *page_table)
{
    return (page_table && (page_table->attr & MMU_ATTR_REMOTE_TLB_FLUSH)) ? TRUE : FALSE;
}

static inline bool mmu_page_table_has_hw_tag(struct mmu_page_table *page_table)
{
    return (page_table && (page_table->attr & MMU_ATTR_HW_TAG_VALID)) ? TRUE : FALSE;
}

static inline uint32_t mmu_page_table_hw_tag(struct mmu_page_table *page_table)
{
    return (page_table) ? page_table->hw_tag : 0;
}

static inline physical_addr_t mmu_page_table_map_addr(struct mmu_page_table *page_table)
{
    return (page_table) ? page_table->map_ia : 0;
}

static inline physical_addr_t mmu_page_table_map_addr_end(struct mmu_page_table *page_table)
{
    if (!page_table) {
        return 0;
    }

    return (page_table->map_ia + ((page_table->table_size / sizeof(arch_pte_t)) * arch_mmu_level_block_size(page_table->stage, page_table->level))) -
           1;
}

static inline physical_addr_t mmu_page_table_physical_addr(struct mmu_page_table *page_table)
{
    return (page_table) ? page_table->table_phy_addr : 0;
}

static inline virtual_size_t mmu_page_table_size(struct mmu_page_table *page_table)
{
    return (page_table) ? page_table->table_size : 0;
}

struct mmu_page_table *mmu_page_table_get_child(struct mmu_page_table *parent, physical_addr_t map_ia, bool create);

int mmu_get_page(struct mmu_page_table *page_table, physical_addr_t ia, struct mmu_page *pg);

int mmu_unmap_page(struct mmu_page_table *page_table, struct mmu_page *pg);

int mmu_map_page(struct mmu_page_table *page_table, struct mmu_page *pg);

int mmu_find_pte(struct mmu_page_table *page_table, physical_addr_t ia, arch_pte_t **ptep, struct mmu_page_table **page_tablep);

struct mmu_get_guest_page_ops {
    void (*setfault)(void *opaque, int stage, int level, physical_addr_t guest_ia);
    int (*gpa2hpa)(void *opaque, int stage, int level, physical_addr_t guest_pa, physical_addr_t *out_host_pa);
};

/**
 * Get guest page table entry
 *
 * Returns VMM_OK on success, VMM_ERR_FAULT on trap and VMM_ERR_xxx on failure.
 */
int mmu_get_guest_page(
    physical_addr_t page_table_guest_ia, int stage, int level, const struct mmu_get_guest_page_ops *ops, void *opaque, physical_addr_t guest_ia,
    struct mmu_page *pg);

void mmu_walk_address(struct mmu_page_table *page_table, physical_addr_t ia, void (*fn)(struct mmu_page_table *, arch_pte_t *, void *), void *opaque);

void mmu_walk_tables(struct mmu_page_table *page_table, void (*fn)(struct mmu_page_table *page_table, void *), void *opaque);

int mmu_find_free_address(struct mmu_page_table *page_table, physical_addr_t min_addr, int page_order, physical_addr_t *addr);

int mmu_idmap_nested_page_table(
    struct mmu_page_table *s2_page_table, struct mmu_page_table *s1_page_table, physical_size_t map_size, uint32_t reg_flags);

#define MMU_TEST_WIDTH_8BIT    (1UL << 0)
#define MMU_TEST_WIDTH_16BIT   (1UL << 1)
#define MMU_TEST_WIDTH_32BIT   (1UL << 2)
#define MMU_TEST_WRITE         (1UL << 3)
#define MMU_TEST_VALID_MASK    0xfUL

#define MMU_TEST_FAULT_S1      (1UL << 0)
#define MMU_TEST_FAULT_NOMAP   (1UL << 1)
#define MMU_TEST_FAULT_READ    (1UL << 2)
#define MMU_TEST_FAULT_WRITE   (1UL << 3)
#define MMU_TEST_FAULT_UNKNOWN (1UL << 4)

int mmu_test_nested_page_table(
    struct mmu_page_table *s2_page_table, struct mmu_page_table *s1_page_table, uint32_t flags, virtual_addr_t addr,
    physical_addr_t expected_output_addr, uint32_t expected_fault_flags);

int mmu_get_hypervisor_page(virtual_addr_t va, struct mmu_page *pg);

int mmu_unmap_hypervisor_page(struct mmu_page *pg);

int mmu_map_hypervisor_page(struct mmu_page *pg);

struct mmu_page_table *mmu_hypervisor_page_table(void);

static inline struct mmu_page_table *mmu_stage2_current_page_table(void)
{
    physical_addr_t table_phy_addr = arch_mmu_stage2_current_page_table_addr();
    return mmu_page_table_find(MMU_STAGE2, table_phy_addr);
}

static inline uint32_t mmu_stage2_current_vmid(void)
{
    return arch_mmu_stage2_current_vmid();
}

static inline int mmu_stage2_change_page_table(struct mmu_page_table *page_table)
{
    return arch_mmu_stage2_change_page_table(mmu_page_table_has_hw_tag(page_table), mmu_page_table_hw_tag(page_table), page_table->table_phy_addr);
}

#endif
