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
 * @file generic_mmu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of Generic MMU
 */

#include <arch_barrier.h>
#include <arch_config.h>
#include <arch_sections.h>
#include <libs/radix-tree.h>
#include <libs/stringlib.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_types.h>

#include <generic_mmu.h>

#define STAGE1_ROOT_ORDER           ARCH_MMU_STAGE1_ROOT_SIZE_ORDER
#define STAGE1_ROOT_SIZE            (1UL << STAGE1_ROOT_ORDER)
#define STAGE1_ROOT_ALIGN_ORDER     ARCH_MMU_STAGE1_ROOT_ALIGN_ORDER
#define STAGE1_ROOT_ALIGN           (1UL << STAGE1_ROOT_ALIGN_ORDER)

#define STAGE1_NONROOT_ORDER        ARCH_MMU_STAGE1_NONROOT_SIZE_ORDER
#define STAGE1_NONROOT_SIZE         (1UL << STAGE1_NONROOT_ORDER)
#define STAGE1_NONROOT_ALIGN_ORDER  ARCH_MMU_STAGE1_NONROOT_ALIGN_ORDER
#define STAGE1_NONROOT_ALIGN        (1UL << STAGE1_NONROOT_ALIGN_ORDER)

/*
 * NOTE: we use 1/64th or 1.5625% of VIRTUAL_ADDR_POOL memory as translation table pool.
 * For example if VIRTUAL_ADDR_POOL is 8 MB and page table size is 4KB then page table
 * pool will be 128 KB or 32 (= 128 KB / 4 KB) page tables.
 */
#define PAGE_TABLE_POOL_COUNT       (CONFIG_VIRTUAL_ADDR_POOL_SIZE_MB << (20 - 6 - ARCH_MMU_STAGE1_NONROOT_SIZE_ORDER))
#define PAGE_TABLE_POOL_SIZE        (PAGE_TABLE_POOL_COUNT * STAGE1_NONROOT_SIZE)

#define INIT_PAGE_TABLE_COUNT       ARCH_MMU_STAGE1_NONROOT_INITIAL_COUNT
#define INIT_PAGE_TABLE_SIZE        (INIT_PAGE_TABLE_COUNT * STAGE1_NONROOT_SIZE)

#define PAGE_TABLE_POOL_TOTAL_COUNT (INIT_PAGE_TABLE_COUNT + PAGE_TABLE_POOL_COUNT)
#define PAGE_TABLE_POOL_TOTAL_SIZE  (INIT_PAGE_TABLE_SIZE + PAGE_TABLE_POOL_SIZE)

struct mmu_ctrl {
    struct mmu_page_table  hyp_page_table;
    virtual_addr_t         page_table_base_va;
    physical_addr_t        page_table_base_pa;
    virtual_addr_t         ipage_table_base_va;
    physical_addr_t        ipage_table_base_pa;
    vmm_rwlock_t           page_table_pool_lock;
    struct mmu_page_table  page_table_pool_array[PAGE_TABLE_POOL_COUNT];
    struct mmu_page_table  ipage_table_pool_array[INIT_PAGE_TABLE_COUNT];
    uint64_t               page_table_pool_alloc_count;
    double_list_t          page_table_pool_free_list;
    vmm_rwlock_t           page_table_nonpool_lock;
    double_list_t          page_table_nonpool_list;
    struct radix_tree_root page_table_nonpool_tree;
};

static struct mmu_ctrl mmuctrl;

uint8_t __aligned(STAGE1_ROOT_ALIGN) stage1_page_table_root[STAGE1_ROOT_SIZE]           = {0};
uint8_t __aligned(STAGE1_NONROOT_ALIGN) stage1_page_table_nonroot[INIT_PAGE_TABLE_SIZE] = {0};

static struct mmu_page_table *mmu_page_table_pool_alloc(int stage, int level)
{
    irq_flags_t            flags;
    double_list_t         *l;
    struct mmu_page_table *page_table;

    vmm_write_lock_irq_save_lite(&mmuctrl.page_table_pool_lock, flags);

    if (list_empty(&mmuctrl.page_table_pool_free_list)) {
        vmm_write_unlock_irq_restore_lite(&mmuctrl.page_table_pool_lock, flags);
        return NULL;
    }

    l          = list_pop(&mmuctrl.page_table_pool_free_list);
    page_table = list_entry(l, struct mmu_page_table, head);
    mmuctrl.page_table_pool_alloc_count++;

    vmm_write_unlock_irq_restore_lite(&mmuctrl.page_table_pool_lock, flags);

    return page_table;
}

static struct mmu_page_table *mmu_page_table_pool_find(int stage, physical_addr_t table_pa)
{
    int index;

    table_pa &= ~(STAGE1_NONROOT_SIZE - 1);

    if ((mmuctrl.ipage_table_base_pa <= table_pa) && (table_pa <= (mmuctrl.ipage_table_base_pa + INIT_PAGE_TABLE_SIZE))) {
        table_pa = table_pa - mmuctrl.ipage_table_base_pa;
        index    = table_pa >> ARCH_MMU_STAGE1_NONROOT_SIZE_ORDER;

        if (index < INIT_PAGE_TABLE_COUNT) {
            return &mmuctrl.ipage_table_pool_array[index];
        }
    }

    if ((mmuctrl.page_table_base_pa <= table_pa) && (table_pa <= (mmuctrl.page_table_base_pa + PAGE_TABLE_POOL_SIZE))) {
        table_pa = table_pa - mmuctrl.page_table_base_pa;
        index    = table_pa >> ARCH_MMU_STAGE1_NONROOT_SIZE_ORDER;

        if (index < PAGE_TABLE_POOL_COUNT) {
            return &mmuctrl.page_table_pool_array[index];
        }
    }

    return NULL;
}

static uint64_t mmu_page_table_pool_count(int stage, int level)
{
    int         i;
    uint64_t    count = 0;
    irq_flags_t flags;

    vmm_read_lock_irq_save_lite(&mmuctrl.page_table_pool_lock, flags);

    for (i = 0; i < INIT_PAGE_TABLE_COUNT; i++) {
        if (mmuctrl.ipage_table_pool_array[i].stage == stage && mmuctrl.ipage_table_pool_array[i].level == level) {
            count++;
        }
    }

    for (i = 0; i < PAGE_TABLE_POOL_COUNT; i++) {
        if (mmuctrl.page_table_pool_array[i].stage == stage && mmuctrl.page_table_pool_array[i].level == level) {
            count++;
        }
    }

    vmm_read_unlock_irq_restore_lite(&mmuctrl.page_table_pool_lock, flags);

    return count;
}

static uint64_t mmu_page_table_pool_alloc_count(void)
{
    uint64_t    count;
    irq_flags_t flags;

    vmm_read_lock_irq_save_lite(&mmuctrl.page_table_pool_lock, flags);
    count = mmuctrl.page_table_pool_alloc_count;
    vmm_read_unlock_irq_restore_lite(&mmuctrl.page_table_pool_lock, flags);

    return count;
}

static void mmu_page_table_pool_free(int stage, struct mmu_page_table *page_table)
{
    irq_flags_t flags;

    vmm_write_lock_irq_save_lite(&mmuctrl.page_table_pool_lock, flags);
    list_add_tail(&page_table->head, &mmuctrl.page_table_pool_free_list);
    mmuctrl.page_table_pool_alloc_count--;
    vmm_write_unlock_irq_restore_lite(&mmuctrl.page_table_pool_lock, flags);
}

struct mmu_page_table_nonpool {
    double_list_t         head;
    struct mmu_page_table page_table;
};

static struct mmu_page_table *mmu_page_table_nonpool_alloc(int stage, int level)
{
    irq_flags_t                    flags;
    struct mmu_page_table         *page_table;
    struct mmu_page_table_nonpool *npage_table;

    npage_table = vmm_zalloc(sizeof(*npage_table));

    if (!npage_table) {
        return NULL;
    }

    INIT_LIST_HEAD(&npage_table->head);
    page_table             = &npage_table->page_table;

    page_table->table_size = 1UL << arch_mmu_page_table_size_order(stage, level);
    page_table->table_va   = vmm_host_alloc_aligned_pages(
        VMM_SIZE_TO_PAGE(page_table->table_size), arch_mmu_page_table_align_order(stage, level), VMM_MEMORY_FLAGS_NORMAL);

    if (vmm_host_virtualAddr_to_physicalAddr(page_table->table_va, &page_table->table_phy_addr)) {
        vmm_host_free_pages(page_table->table_va, VMM_SIZE_TO_PAGE(page_table->table_size));
        vmm_free(npage_table);
        return NULL;
    }

    vmm_write_lock_irq_save_lite(&mmuctrl.page_table_nonpool_lock, flags);

    if (radix_tree_insert(&mmuctrl.page_table_nonpool_tree, page_table->table_phy_addr >> arch_mmu_page_table_min_align_order(stage), npage_table)) {
        vmm_write_unlock_irq_restore_lite(&mmuctrl.page_table_nonpool_lock, flags);
        vmm_host_free_pages(page_table->table_va, VMM_SIZE_TO_PAGE(page_table->table_size));
        vmm_free(npage_table);
        return NULL;
    }

    list_add_tail(&npage_table->head, &mmuctrl.page_table_nonpool_list);

    vmm_write_unlock_irq_restore_lite(&mmuctrl.page_table_nonpool_lock, flags);

    return page_table;
}

static struct mmu_page_table *mmu_page_table_nonpool_find(int stage, physical_addr_t table_pa)
{
    irq_flags_t                    flags;
    struct mmu_page_table         *page_table = NULL;
    struct mmu_page_table_nonpool *npage_table;

    vmm_read_lock_irq_save_lite(&mmuctrl.page_table_nonpool_lock, flags);

    npage_table = radix_tree_lookup(&mmuctrl.page_table_nonpool_tree, table_pa >> arch_mmu_page_table_min_align_order(stage));

    if (npage_table) {
        page_table = &npage_table->page_table;
    }

    vmm_read_unlock_irq_restore_lite(&mmuctrl.page_table_nonpool_lock, flags);

    return page_table;
}

static uint64_t mmu_page_table_nonpool_count(int stage, int level)
{
    uint64_t                       count = 0;
    irq_flags_t                    flags;
    struct mmu_page_table_nonpool *npage_table;

    vmm_read_lock_irq_save_lite(&mmuctrl.page_table_nonpool_lock, flags);

    list_for_each_entry(npage_table, &mmuctrl.page_table_nonpool_list, head)
    {
        if (npage_table->page_table.stage == stage && npage_table->page_table.level == level) {
            count++;
        }
    }

    vmm_read_unlock_irq_restore_lite(&mmuctrl.page_table_nonpool_lock, flags);

    return count;
}

static void mmu_page_table_nonpool_free(int stage, struct mmu_page_table *page_table)
{
    irq_flags_t                    flags;
    struct mmu_page_table_nonpool *npage_table = container_of(page_table, struct mmu_page_table_nonpool, page_table);

    vmm_write_lock_irq_save_lite(&mmuctrl.page_table_nonpool_lock, flags);

    list_del_init(&npage_table->head);

    radix_tree_delete(&mmuctrl.page_table_nonpool_tree, page_table->table_phy_addr >> arch_mmu_page_table_min_align_order(stage));

    vmm_write_unlock_irq_restore_lite(&mmuctrl.page_table_nonpool_lock, flags);

    vmm_host_free_pages(page_table->table_va, VMM_SIZE_TO_PAGE(page_table->table_size));

    vmm_free(npage_table);
}

uint64_t mmu_page_table_count(int stage, int level)
{
    if (stage == MMU_STAGE1) {
        return mmu_page_table_pool_count(stage, level) + ((level == arch_mmu_start_level(stage)) ? 1 : 0);
    }

    return mmu_page_table_nonpool_count(stage, level);
}

struct mmu_page_table *mmu_page_table_find(int stage, physical_addr_t table_pa)
{
    if (stage == MMU_STAGE1) {
        return mmu_page_table_pool_find(stage, table_pa);
    }

    return mmu_page_table_nonpool_find(stage, table_pa);
}

static inline bool mmu_page_table_isattached(struct mmu_page_table *child)
{
    return ((child != NULL) && (child->parent != NULL));
}

static int mmu_page_table_attach(struct mmu_page_table *parent, physical_addr_t map_ia, struct mmu_page_table *child)
{
    int         index;
    arch_pte_t *pte;
    irq_flags_t flags;

    if (!parent || !child) {
        return VMM_ERR_FAIL;
    }

    if (mmu_page_table_isattached(child)) {
        return VMM_ERR_FAIL;
    }

    if ((parent->level == 0) || (child->stage != parent->stage)) {
        return VMM_ERR_FAIL;
    }

    index = arch_mmu_level_index(map_ia, parent->stage, parent->level);
    pte   = (arch_pte_t *)parent->table_va;

    vmm_spin_lock_irq_save_lite(&parent->table_lock, flags);

    if (arch_mmu_pte_is_valid(&pte[index], parent->stage, parent->level)) {
        vmm_spin_unlock_irq_restore_lite(&parent->table_lock, flags);
        return VMM_ERR_EXIST;
    }

    arch_mmu_pte_set_table(&pte[index], parent->stage, parent->level, child->table_phy_addr);
    arch_mmu_pte_sync(&pte[index], parent->stage, parent->level);

    child->parent = parent;
    child->level  = parent->level - 1;
    child->map_ia = map_ia & arch_mmu_level_map_mask(parent->stage, parent->level);
    parent->pte_count++;
    parent->child_count++;
    list_add(&child->head, &parent->child_list);

    vmm_spin_unlock_irq_restore_lite(&parent->table_lock, flags);

    return VMM_OK;
}

static int mmu_page_table_deattach(struct mmu_page_table *child)
{
    int                    index;
    arch_pte_t            *pte;
    irq_flags_t            flags;
    struct mmu_page_table *parent;

    if (!child || !mmu_page_table_isattached(child)) {
        return VMM_ERR_FAIL;
    }

    parent = child->parent;
    index  = arch_mmu_level_index(child->map_ia, parent->stage, parent->level);
    pte    = (arch_pte_t *)parent->table_va;

    vmm_spin_lock_irq_save_lite(&parent->table_lock, flags);

    if (!arch_mmu_pte_is_valid(&pte[index], parent->stage, parent->level)) {
        vmm_spin_unlock_irq_restore_lite(&parent->table_lock, flags);
        return VMM_ERR_FAIL;
    }

    arch_mmu_pte_clear(&pte[index], parent->stage, parent->level);
    arch_mmu_pte_sync(&pte[index], parent->stage, parent->level);

    child->parent = NULL;
    parent->pte_count--;
    parent->child_count--;
    list_del(&child->head);

    vmm_spin_unlock_irq_restore_lite(&parent->table_lock, flags);

    return VMM_OK;
}

struct mmu_page_table *mmu_page_table_alloc(int stage, int level, uint32_t attr, uint32_t hw_tag)
{
    struct mmu_page_table *page_table;

    if (stage <= MMU_STAGE_UNKNOWN || MMU_STAGE_MAX <= stage) {
        return NULL;
    }

    if (level < 0) {
        level = arch_mmu_start_level(stage);
    }

    if (stage == MMU_STAGE1) {
        page_table = mmu_page_table_pool_alloc(stage, level);
    } else {
        page_table = mmu_page_table_nonpool_alloc(stage, level);
    }

    if (!page_table) {
        return NULL;
    }

    page_table->parent = NULL;
    page_table->stage  = stage;
    page_table->level  = level;
    page_table->attr   = attr;
    page_table->hw_tag = hw_tag;
    page_table->map_ia = 0;
    INIT_SPIN_LOCK(&page_table->table_lock);
    page_table->pte_count   = 0;
    page_table->child_count = 0;
    INIT_LIST_HEAD(&page_table->child_list);
    memset((void *)page_table->table_va, 0, page_table->table_size);

    return page_table;
}

int mmu_page_table_free(struct mmu_page_table *page_table)
{
    int                    stage, rc = VMM_OK;
    irq_flags_t            flags;
    double_list_t         *l;
    struct mmu_page_table *child;

    if (!page_table) {
        return VMM_ERR_FAIL;
    }

    if (mmu_page_table_isattached(page_table)) {
        if ((rc = mmu_page_table_deattach(page_table))) {
            return rc;
        }
    }

    while (!list_empty(&page_table->child_list)) {
        l     = list_first(&page_table->child_list);
        child = list_entry(l, struct mmu_page_table, head);

        if ((rc = mmu_page_table_deattach(child))) {
            return rc;
        }

        if ((rc = mmu_page_table_free(child))) {
            return rc;
        }
    }

    vmm_spin_lock_irq_save_lite(&page_table->table_lock, flags);
    page_table->pte_count = 0;
    vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);

    stage              = page_table->stage;
    page_table->stage  = 0;
    page_table->level  = 0;
    page_table->attr   = 0;
    page_table->hw_tag = 0;
    page_table->map_ia = 0;

    if (stage == MMU_STAGE1) {
        mmu_page_table_pool_free(stage, page_table);
    } else {
        mmu_page_table_nonpool_free(stage, page_table);
    }

    return VMM_OK;
}

struct mmu_page_table *mmu_page_table_get_child(struct mmu_page_table *parent, physical_addr_t map_ia, bool create)
{
    int                    rc, index;
    arch_pte_t            *pte, pte_val;
    irq_flags_t            flags;
    physical_addr_t        table_pa;
    struct mmu_page_table *child;

    if (!parent || !parent->level) {
        return NULL;
    }

    index = arch_mmu_level_index(map_ia, parent->stage, parent->level);
    pte   = (arch_pte_t *)parent->table_va;

    vmm_spin_lock_irq_save_lite(&parent->table_lock, flags);
    pte_val = pte[index];
    vmm_spin_unlock_irq_restore_lite(&parent->table_lock, flags);

    if (arch_mmu_pte_is_valid(&pte_val, parent->stage, parent->level)) {
        child = NULL;

        if ((parent->level > 0) && arch_mmu_pte_is_table(&pte_val, parent->stage, parent->level)) {
            table_pa = arch_mmu_pte_table_addr(&pte_val, parent->stage, parent->level);
            child    = mmu_page_table_find(parent->stage, table_pa);

            if (!child || child->parent != parent) {
                vmm_printf(
                    "%s: invalid child for address "
                    "0x%" PRIPADDR " in page table at "
                    "0x%" PRIPADDR " stage=%d level=%d\n",
                    __func__, map_ia, parent->table_phy_addr, parent->stage, parent->level);
                child = NULL;
            }
        }

        return child;
    }

    if (!create) {
        return NULL;
    }

    child = mmu_page_table_alloc(parent->stage, parent->level - 1, parent->attr, parent->hw_tag);

    if (!child) {
        vmm_printf(
            "%s: failed to alloc child for address "
            "0x%" PRIPADDR " in page table at "
            "0x%" PRIPADDR " stage=%d level=%d\n",
            __func__, map_ia, parent->table_phy_addr, parent->stage, parent->level);
        return NULL;
    }

    if ((rc = mmu_page_table_attach(parent, map_ia, child))) {
        if (rc != VMM_ERR_EXIST) {
            vmm_printf(
                "%s: failed to attach child for address "
                "0x%" PRIPADDR " in page table at "
                "0x%" PRIPADDR " stage=%d level=%d\n",
                __func__, map_ia, parent->table_phy_addr, parent->stage, parent->level);
        }

        mmu_page_table_free(child);
        child = NULL;
    }

    return child;
}

int mmu_get_page(struct mmu_page_table *page_table, physical_addr_t ia, struct mmu_page *pg)
{
    int                    index;
    arch_pte_t            *pte;
    irq_flags_t            flags;
    struct mmu_page_table *child;

    if (!page_table || !pg) {
        return VMM_ERR_FAIL;
    }

    index = arch_mmu_level_index(ia, page_table->stage, page_table->level);
    pte   = (arch_pte_t *)page_table->table_va;

    vmm_spin_lock_irq_save_lite(&page_table->table_lock, flags);

    if (!arch_mmu_pte_is_valid(&pte[index], page_table->stage, page_table->level)) {
        vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);
        return VMM_ERR_FAIL;
    }

    if ((page_table->level == 0) && arch_mmu_pte_is_table(&pte[index], page_table->stage, page_table->level)) {
        vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);
        return VMM_ERR_FAIL;
    }

    if ((page_table->level > 0) && arch_mmu_pte_is_table(&pte[index], page_table->stage, page_table->level)) {
        vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);
        child = mmu_page_table_get_child(page_table, ia, FALSE);

        if (!child) {
            return VMM_ERR_FAIL;
        }

        return mmu_get_page(child, ia, pg);
    }

    memset(pg, 0, sizeof(struct mmu_page));

    pg->ia   = ia & arch_mmu_level_map_mask(page_table->stage, page_table->level);
    pg->oa   = arch_mmu_pte_addr(&pte[index], page_table->stage, page_table->level);
    pg->size = arch_mmu_level_block_size(page_table->stage, page_table->level);
    arch_mmu_pte_flags(&pte[index], page_table->stage, page_table->level, &pg->flags);

    vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);

    return VMM_OK;
}

int mmu_unmap_page(struct mmu_page_table *page_table, struct mmu_page *pg)
{
    int                    start_level;
    int                    index, rc;
    bool                   free_page_table;
    arch_pte_t            *pte;
    irq_flags_t            flags;
    struct mmu_page_table *child;
    physical_size_t        blksz;

    if (!page_table || !pg) {
        return VMM_ERR_FAIL;
    }

    if (!arch_mmu_valid_block_size(pg->size)) {
        return VMM_ERR_FAIL;
    }

    blksz = arch_mmu_level_block_size(page_table->stage, page_table->level);

    if (pg->size > blksz) {
        return VMM_ERR_FAIL;
    }

    start_level = arch_mmu_start_level(page_table->stage);

    if (pg->size < blksz) {
        child = mmu_page_table_get_child(page_table, pg->ia, FALSE);

        if (!child) {
            return VMM_ERR_FAIL;
        }

        rc = mmu_unmap_page(child, pg);

        if ((page_table->pte_count == 0) && (page_table->level < start_level)) {
            mmu_page_table_free(page_table);
        }

        return rc;
    }

    index = arch_mmu_level_index(pg->ia, page_table->stage, page_table->level);
    pte   = (arch_pte_t *)page_table->table_va;

    vmm_spin_lock_irq_save_lite(&page_table->table_lock, flags);

    if (!arch_mmu_pte_is_valid(&pte[index], page_table->stage, page_table->level)) {
        vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);
        return VMM_ERR_FAIL;
    }

    if ((page_table->level == 0) && arch_mmu_pte_is_table(&pte[index], page_table->stage, page_table->level)) {
        vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);
        return VMM_ERR_FAIL;
    }

    arch_mmu_pte_clear(&pte[index], page_table->stage, page_table->level);
    arch_mmu_pte_sync(&pte[index], page_table->stage, page_table->level);

    if (page_table->stage == MMU_STAGE1) {
        arch_mmu_stage1_tlbflush(
            mmu_page_table_need_remote_tlbflush(page_table), mmu_page_table_has_hw_tag(page_table), mmu_page_table_hw_tag(page_table), pg->ia, blksz);
    } else {
        arch_mmu_stage2_tlbflush(
            mmu_page_table_need_remote_tlbflush(page_table), mmu_page_table_has_hw_tag(page_table), mmu_page_table_hw_tag(page_table), pg->ia, blksz);
    }

    page_table->pte_count--;
    free_page_table = FALSE;

    if ((page_table->pte_count == 0) && (page_table->level < start_level)) {
        free_page_table = TRUE;
    }

    vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);

    if (free_page_table) {
        mmu_page_table_free(page_table);
    }

    return VMM_OK;
}

int mmu_map_page(struct mmu_page_table *page_table, struct mmu_page *pg)
{
    int                    index;
    arch_pte_t            *pte;
    irq_flags_t            flags;
    struct mmu_page_table *child;
    physical_size_t        blksz;

    if (!page_table || !pg) {
        return VMM_ERR_FAIL;
    }

    if (!arch_mmu_valid_block_size(pg->size)) {
        return VMM_ERR_INVALID;
    }

    blksz = arch_mmu_level_block_size(page_table->stage, page_table->level);

    if (pg->size > blksz) {
        return VMM_ERR_FAIL;
    }

    if (pg->size < blksz) {
        child = mmu_page_table_get_child(page_table, pg->ia, TRUE);

        if (!child) {
            return VMM_ERR_FAIL;
        }

        return mmu_map_page(child, pg);
    }

    index = arch_mmu_level_index(pg->ia, page_table->stage, page_table->level);
    pte   = (arch_pte_t *)page_table->table_va;

    vmm_spin_lock_irq_save_lite(&page_table->table_lock, flags);

    if (arch_mmu_pte_is_valid(&pte[index], page_table->stage, page_table->level)) {
        vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);
        return VMM_ERR_FAIL;
    }

    arch_mmu_pte_set(&pte[index], page_table->stage, page_table->level, pg->oa, &pg->flags);
    arch_mmu_pte_sync(&pte[index], page_table->stage, page_table->level);

    if (page_table->stage == MMU_STAGE1) {
        arch_mmu_stage1_tlbflush(
            mmu_page_table_need_remote_tlbflush(page_table), mmu_page_table_has_hw_tag(page_table), mmu_page_table_hw_tag(page_table), pg->ia, blksz);
    } else {
        arch_mmu_stage2_tlbflush(
            mmu_page_table_need_remote_tlbflush(page_table), mmu_page_table_has_hw_tag(page_table), mmu_page_table_hw_tag(page_table), pg->ia, blksz);
    }

    page_table->pte_count++;

    vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);

    return VMM_OK;
}

int mmu_find_pte(struct mmu_page_table *page_table, physical_addr_t ia, arch_pte_t **ptep, struct mmu_page_table **page_tablep)
{
    int                    index;
    arch_pte_t            *pte;
    irq_flags_t            flags;
    physical_size_t        map_last;
    struct mmu_page_table *child;

    if (!page_table || !ptep || !page_tablep) {
        return VMM_ERR_FAIL;
    }

    map_last = arch_mmu_level_block_size(page_table->stage, page_table->level);
    map_last *= (page_table->table_size / sizeof(arch_pte_t));
    map_last -= 1;

    if ((ia < page_table->map_ia) || ((page_table->map_ia + map_last) < ia)) {
        return VMM_ERR_FAIL;
    }

    index = arch_mmu_level_index(ia, page_table->stage, page_table->level);
    pte   = (arch_pte_t *)page_table->table_va;

    vmm_spin_lock_irq_save_lite(&page_table->table_lock, flags);

    if (!arch_mmu_pte_is_valid(&pte[index], page_table->stage, page_table->level)) {
        vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);
        return VMM_ERR_FAIL;
    }

    if ((page_table->level == 0) && arch_mmu_pte_is_table(&pte[index], page_table->stage, page_table->level)) {
        vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);
        return VMM_ERR_FAIL;
    }

    if ((page_table->level > 0) && arch_mmu_pte_is_table(&pte[index], page_table->stage, page_table->level)) {
        vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);
        child = mmu_page_table_get_child(page_table, ia, FALSE);

        if (!child) {
            return VMM_ERR_FAIL;
        }

        return mmu_find_pte(child, ia, ptep, page_tablep);
    }

    *ptep        = &pte[index];
    *page_tablep = page_table;

    vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);

    return VMM_OK;
}

int mmu_get_guest_page(
    physical_addr_t page_table_guest_pa, int stage, int level, const struct mmu_get_guest_page_ops *ops, void *opaque, physical_addr_t guest_ia,
    struct mmu_page *pg)
{
    int             idx;
    arch_pte_t      pte;
    physical_addr_t pte_pa;

    if (stage <= MMU_STAGE_UNKNOWN || MMU_STAGE_MAX <= stage || arch_mmu_start_level(stage) < level || !ops || !pg) {
        return VMM_ERR_INVALID;
    }

    if (level < 0) {
        level = arch_mmu_start_level(stage);
    }

    idx = ops->gpa2hpa(opaque, stage, level, page_table_guest_pa, &pte_pa);

    if (idx) {
        if (idx == VMM_ERR_FAULT) {
            ops->setfault(opaque, stage, level, guest_ia);
        }

        return idx;
    }

    idx = arch_mmu_level_index(guest_ia, stage, level);

    if (vmm_host_memory_read(pte_pa + idx * sizeof(pte), &pte, sizeof(pte), TRUE) != sizeof(pte)) {
        ops->setfault(opaque, stage, level, guest_ia);
        return VMM_ERR_FAULT;
    }

    if (!arch_mmu_pte_is_valid(&pte, stage, level)) {
        ops->setfault(opaque, stage, level, guest_ia);
        return VMM_ERR_FAULT;
    }

    if ((level == 0) && arch_mmu_pte_is_table(&pte, stage, level)) {
        ops->setfault(opaque, stage, level, guest_ia);
        return VMM_ERR_FAULT;
    }

    if ((level > 0) && arch_mmu_pte_is_table(&pte, stage, level)) {
        pte_pa = arch_mmu_pte_table_addr(&pte, stage, level);
        return mmu_get_guest_page(pte_pa, stage, level - 1, ops, opaque, guest_ia, pg);
    }

    memset(pg, 0, sizeof(struct mmu_page));

    pg->ia   = guest_ia & arch_mmu_level_map_mask(stage, level);
    pg->oa   = arch_mmu_pte_addr(&pte, stage, level);
    pg->size = arch_mmu_level_block_size(stage, level);
    arch_mmu_pte_flags(&pte, stage, level, &pg->flags);

    return VMM_OK;
}

void mmu_walk_address(struct mmu_page_table *page_table, physical_addr_t ia, void (*fn)(struct mmu_page_table *, arch_pte_t *, void *), void *opaque)
{
    int                    index;
    arch_pte_t            *pte;
    irq_flags_t            flags;
    physical_size_t        map_last;
    struct mmu_page_table *child;

    if (!page_table || !fn) {
        return;
    }

    map_last = arch_mmu_level_block_size(page_table->stage, page_table->level);
    map_last *= (page_table->table_size / sizeof(arch_pte_t));
    map_last -= 1;

    if ((ia < page_table->map_ia) || ((page_table->map_ia + map_last) < ia)) {
        return;
    }

    index = arch_mmu_level_index(ia, page_table->stage, page_table->level);
    pte   = (arch_pte_t *)page_table->table_va;

    fn(page_table, &pte[index], opaque);

    vmm_spin_lock_irq_save_lite(&page_table->table_lock, flags);

    if (!arch_mmu_pte_is_valid(&pte[index], page_table->stage, page_table->level)) {
        vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);
        return;
    }

    if ((page_table->level == 0) && arch_mmu_pte_is_table(&pte[index], page_table->stage, page_table->level)) {
        vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);
        return;
    }

    if ((page_table->level > 0) && arch_mmu_pte_is_table(&pte[index], page_table->stage, page_table->level)) {
        vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);
        child = mmu_page_table_get_child(page_table, ia, FALSE);

        if (!child) {
            return;
        }

        mmu_walk_address(child, ia, fn, opaque);
        return;
    }

    vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);
}

void mmu_walk_tables(struct mmu_page_table *page_table, void (*fn)(struct mmu_page_table *page_table, void *), void *opaque)
{
    irq_flags_t            flags;
    struct mmu_page_table *child;

    if (!page_table || !fn) {
        return;
    }

    fn(page_table, opaque);

    vmm_spin_lock_irq_save_lite(&page_table->table_lock, flags);

    list_for_each_entry(child, &page_table->child_list, head)
    {
        vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);
        mmu_walk_tables(child, fn, opaque);
        vmm_spin_lock_irq_save_lite(&page_table->table_lock, flags);
    }

    vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);
}

struct free_address_walk {
    bool             found;
    int              level;
    physical_addr_t  min_addr;
    physical_addr_t *addr;
};

static void free_address_walk(struct mmu_page_table *page_table, void *opaque)
{
    arch_pte_t               *pte;
    irq_flags_t               flags;
    physical_addr_t           ia;
    int                       index, pte_count;
    struct free_address_walk *w = opaque;

    if (w->found || page_table->level != w->level) {
        return;
    }

    pte       = (arch_pte_t *)page_table->table_va;
    pte_count = page_table->table_size / sizeof(arch_pte_t);

    vmm_spin_lock_irq_save_lite(&page_table->table_lock, flags);

    for (index = 0; index < pte_count; index++) {
        if (arch_mmu_pte_is_valid(&pte[index], page_table->stage, page_table->level)) {
            continue;
        }

        ia = page_table->map_ia + index * arch_mmu_level_block_size(page_table->stage, page_table->level);

        if (ia < w->min_addr) {
            continue;
        }

        vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);
        w->found = TRUE;
        *w->addr = ia;
        return;
    }

    vmm_spin_unlock_irq_restore_lite(&page_table->table_lock, flags);

    return;
}

int mmu_find_free_address(struct mmu_page_table *page_table, physical_addr_t min_addr, int page_order, physical_addr_t *addr)
{
    int                      level;
    struct free_address_walk w;

    if (!page_table || !addr) {
        return VMM_ERR_INVALID;
    }

    for (level = 0; level <= page_table->level; level++) {
        if (arch_mmu_level_block_shift(page_table->stage, level) >= page_order) {
            break;
        }
    }

    if (page_table->level < level) {
        return VMM_ERR_INVALID;
    }

    while (level <= page_table->level) {
        w.found    = FALSE;
        w.level    = level;
        w.min_addr = min_addr;
        w.addr     = addr;

        mmu_walk_tables(page_table, free_address_walk, &w);

        if (w.found) {
            return VMM_OK;
        }

        level++;
    }

    return VMM_ERR_NOTAVAIL;
}

struct idmap_nested_page_table_walk {
    struct mmu_page_table *s2_page_table;
    int                    map_level;
    physical_size_t        map_size;
    uint32_t               reg_flags;
    int                    error;
};

static void idmap_nested_page_table_walk(struct mmu_page_table *page_table, void *opaque)
{
    int             rc;
    physical_addr_t ta;
    struct mmu_page pg                      = {0}, tpg;

    struct idmap_nested_page_table_walk *iw = opaque;

    if (iw->error) {
        return;
    }

    arch_mmu_pgflags_set(&pg.flags, MMU_STAGE2, iw->reg_flags);

    for (ta = 0; ta < page_table->table_size; ta += iw->map_size) {
        pg.ia = page_table->table_phy_addr + ta;
        pg.ia &= arch_mmu_level_map_mask(MMU_STAGE2, iw->map_level);
        pg.oa = page_table->table_phy_addr + ta;
        pg.oa &= arch_mmu_level_map_mask(MMU_STAGE2, iw->map_level);
        pg.size = iw->map_size;

        if (mmu_get_page(iw->s2_page_table, pg.ia, &tpg)) {
            rc = mmu_map_page(iw->s2_page_table, &pg);

            if (rc) {
                iw->error = rc;
                return;
            }
        } else {
            if (pg.ia != tpg.ia || pg.oa != tpg.oa || pg.size != tpg.size) {
                iw->error = VMM_ERR_FAIL;
                return;
            }
        }
    }
}

int mmu_idmap_nested_page_table(
    struct mmu_page_table *s2_page_table, struct mmu_page_table *s1_page_table, physical_size_t map_size, uint32_t reg_flags)
{
    int                                 level;
    struct idmap_nested_page_table_walk iw;

    if (!s2_page_table || (s2_page_table->stage != MMU_STAGE2)) {
        return VMM_ERR_INVALID;
    }

    if (!s1_page_table || (s1_page_table->stage != MMU_STAGE1)) {
        return VMM_ERR_INVALID;
    }

    for (level = 0; level <= s2_page_table->level; level++) {
        if (arch_mmu_level_block_size(s2_page_table->stage, level) == map_size) {
            break;
        }
    }

    if (s2_page_table->level < level) {
        return VMM_ERR_INVALID;
    }

    iw.s2_page_table = s2_page_table;
    iw.map_level     = level;
    iw.map_size      = map_size;
    iw.reg_flags     = reg_flags;
    iw.error         = VMM_OK;

    mmu_walk_tables(s1_page_table, idmap_nested_page_table_walk, &iw);

    return iw.error;
}

int mmu_test_nested_page_table(
    struct mmu_page_table *s2_page_table, struct mmu_page_table *s1_page_table, uint32_t flags, virtual_addr_t addr,
    physical_addr_t expected_output_addr, uint32_t expected_fault_flags)
{
    int             rc;
    physical_addr_t oaddr   = 0;
    uint32_t        offlags = 0;

    if (!s2_page_table || (s2_page_table->stage != MMU_STAGE2)) {
        return VMM_ERR_INVALID;
    }

    if (s1_page_table && (s1_page_table->stage != MMU_STAGE1)) {
        return VMM_ERR_INVALID;
    }

    if (flags & ~MMU_TEST_VALID_MASK) {
        return VMM_ERR_INVALID;
    }

    if ((flags & MMU_TEST_WIDTH_16BIT) && (addr & 0x1)) {
        return VMM_ERR_INVALID;
    }

    if ((flags & MMU_TEST_WIDTH_32BIT) && (addr & 0x3)) {
        return VMM_ERR_INVALID;
    }

    rc = arch_mmu_test_nested_page_table(
        s2_page_table->table_phy_addr, (s1_page_table) ? TRUE : FALSE, (s1_page_table) ? s1_page_table->table_phy_addr : 0, flags, addr, &oaddr,
        &offlags);

    if (rc) {
        return rc;
    }

    /* All expected fault bits should be set */
    if ((offlags & expected_fault_flags) ^ expected_fault_flags) {
        return VMM_ERR_FAIL;
    }

    /* No unexpected fault bit should be set */
    if (offlags & ~expected_fault_flags) {
        return VMM_ERR_FAIL;
    }

    /* Output address should match */
    if (oaddr != expected_output_addr) {
        return VMM_ERR_FAIL;
    }

    return VMM_OK;
}

int mmu_get_hypervisor_page(virtual_addr_t va, struct mmu_page *pg)
{
    return mmu_get_page(&mmuctrl.hyp_page_table, va, pg);
}

int mmu_unmap_hypervisor_page(struct mmu_page *pg)
{
    return mmu_unmap_page(&mmuctrl.hyp_page_table, pg);
}

int mmu_map_hypervisor_page(struct mmu_page *pg)
{
    return mmu_map_page(&mmuctrl.hyp_page_table, pg);
}

struct mmu_page_table *mmu_hypervisor_page_table(void)
{
    return &mmuctrl.hyp_page_table;
}

#ifdef ARCH_HAS_MEMORY_READWRITE

/* Initialized by memory read/write init */
static struct mmu_page_table *mem_rw_page_table[CONFIG_CPU_COUNT];
static arch_pte_t            *mem_rw_pte[CONFIG_CPU_COUNT];
static arch_page_flags_t      mem_rw_pgflags_cache[CONFIG_CPU_COUNT];
static arch_page_flags_t      mem_rw_pgflags_nocache[CONFIG_CPU_COUNT];

int arch_cpu_addr_space_memory_read(virtual_addr_t tmp_va, physical_addr_t src, void *dst, uint32_t len, bool cacheable)
{
    arch_pte_t         old_pte_val;
    uint32_t           cpu              = vmm_smp_processor_id();
    arch_pte_t        *pte              = mem_rw_pte[cpu];
    int                page_table_level = mem_rw_page_table[cpu]->level;
    arch_page_flags_t *flags            = (cacheable) ? &mem_rw_pgflags_cache[cpu] : &mem_rw_pgflags_nocache[cpu];
    virtual_addr_t     offset           = src & VMM_PAGE_MASK;

    old_pte_val                         = *pte;

    arch_mmu_pte_set(pte, MMU_STAGE1, page_table_level, src, flags);
    arch_mmu_pte_sync(pte, MMU_STAGE1, page_table_level);
    arch_mmu_stage1_tlbflush(FALSE, FALSE, 0, tmp_va, VMM_PAGE_SIZE);

    switch (len) {
        case 1:
            *((uint8_t *)dst) = *(uint8_t *)(tmp_va + offset);
            break;

        case 2:
            *((uint16_t *)dst) = *(uint16_t *)(tmp_va + offset);
            break;

        case 4:
            *((uint32_t *)dst) = *(uint32_t *)(tmp_va + offset);
            break;

        case 8:
            *((uint64_t *)dst) = *(uint64_t *)(tmp_va + offset);
            break;

        default:
            memcpy(dst, (void *)(tmp_va + offset), len);
            break;
    };

    *pte = old_pte_val;

    arch_mmu_pte_sync(pte, MMU_STAGE1, page_table_level);

    return VMM_OK;
}

int arch_cpu_addr_space_memory_write(virtual_addr_t tmp_va, physical_addr_t dst, void *src, uint32_t len, bool cacheable)
{
    arch_pte_t         old_pte_val;
    uint32_t           cpu              = vmm_smp_processor_id();
    arch_pte_t        *pte              = mem_rw_pte[cpu];
    int                page_table_level = mem_rw_page_table[cpu]->level;
    arch_page_flags_t *flags            = (cacheable) ? &mem_rw_pgflags_cache[cpu] : &mem_rw_pgflags_nocache[cpu];
    virtual_addr_t     offset           = dst & VMM_PAGE_MASK;

    old_pte_val                         = *pte;

    arch_mmu_pte_set(pte, MMU_STAGE1, page_table_level, dst, flags);
    arch_mmu_pte_sync(pte, MMU_STAGE1, page_table_level);
    arch_mmu_stage1_tlbflush(FALSE, FALSE, 0, tmp_va, VMM_PAGE_SIZE);

    switch (len) {
        case 1:
            *(uint8_t *)(tmp_va + offset) = *((uint8_t *)src);
            break;

        case 2:
            *(uint16_t *)(tmp_va + offset) = *((uint16_t *)src);
            break;

        case 4:
            *(uint32_t *)(tmp_va + offset) = *((uint32_t *)src);
            break;

        case 8:
            *(uint64_t *)(tmp_va + offset) = *((uint64_t *)src);
            break;

        default:
            memcpy((void *)(tmp_va + offset), src, len);
            break;
    };

    *pte = old_pte_val;

    arch_mmu_pte_sync(pte, MMU_STAGE1, page_table_level);

    return VMM_OK;
}

int __cpuinit arch_cpu_addr_space_memory_rwinit(virtual_addr_t tmp_va)
{
    int             rc;
    uint32_t        cpu = vmm_smp_processor_id();
    struct mmu_page p;

    memset(&p, 0, sizeof(p));
    p.ia   = tmp_va;
    p.oa   = 0x0;
    p.size = VMM_PAGE_SIZE;
    arch_mmu_pgflags_set(&p.flags, MMU_STAGE1, VMM_MEMORY_FLAGS_NORMAL);

    rc = mmu_map_hypervisor_page(&p);

    if (rc) {
        return rc;
    }

    mem_rw_pte[cpu]        = NULL;
    mem_rw_page_table[cpu] = NULL;

    rc                     = mmu_find_pte(mmu_hypervisor_page_table(), tmp_va, &mem_rw_pte[cpu], &mem_rw_page_table[cpu]);

    if (rc) {
        return rc;
    }

    arch_mmu_pgflags_set(&mem_rw_pgflags_cache[cpu], MMU_STAGE1, VMM_MEMORY_FLAGS_NORMAL);
    arch_mmu_pgflags_set(&mem_rw_pgflags_nocache[cpu], MMU_STAGE1, VMM_MEMORY_FLAGS_NORMAL_NOCACHE);

    return VMM_OK;
}

#endif

void arch_cpu_addr_space_print_info(vmm_char_device_t *cdev)
{
    uint64_t count, total;
    int      stage, level;

    vmm_cdev_printf(cdev, "Pool Page Tables\n");
    count = mmu_page_table_pool_alloc_count();
    vmm_cdev_printf(cdev, "    Used  : %" PRIu64 "\n", count);
    vmm_cdev_printf(cdev, "    Free  : %" PRIu64 "\n", ((uint64_t)PAGE_TABLE_POOL_TOTAL_COUNT - count));
    vmm_cdev_printf(cdev, "    Total : %" PRIu64 "\n", (uint64_t)PAGE_TABLE_POOL_TOTAL_COUNT);
    vmm_cdev_printf(cdev, "    Size  : %" PRIu64 " KB\n", (uint64_t)(PAGE_TABLE_POOL_TOTAL_SIZE / 1024));
    vmm_cdev_printf(cdev, "\n");

    for (stage = MMU_STAGE1; stage < MMU_STAGE_MAX; stage++) {
        vmm_cdev_printf(cdev, "Stage%d Page Tables\n", stage);
        total = 0;

        for (level = arch_mmu_start_level(stage); -1 < level; level--) {
            count = mmu_page_table_count(stage, level);
            vmm_cdev_printf(cdev, "    Level%d : %" PRIu64 "\n", level, count);
            total += count;
        }

        vmm_cdev_printf(cdev, "    Total  : %" PRIu64 "\n", total);
        vmm_cdev_printf(cdev, "\n");
    }
}

uint32_t arch_cpu_addr_space_huge_page_log2size(void)
{
    return arch_mmu_level_block_shift(MMU_STAGE1, 1);
}

int arch_cpu_addr_space_map(virtual_addr_t page_va, virtual_size_t page_sz, physical_addr_t page_pa, uint32_t memory_flags)
{
    struct mmu_page p;

    if (page_sz != arch_mmu_level_block_size(MMU_STAGE1, 0) && page_sz != arch_mmu_level_block_size(MMU_STAGE1, 1)) {
        return VMM_ERR_INVALID;
    }

    memset(&p, 0, sizeof(p));
    p.ia   = page_va;
    p.oa   = page_pa;
    p.size = page_sz;
    arch_mmu_pgflags_set(&p.flags, MMU_STAGE1, memory_flags);

    return mmu_map_hypervisor_page(&p);
}

int arch_cpu_addr_space_unmap(virtual_addr_t page_va)
{
    int             rc;
    struct mmu_page p;

    rc = mmu_get_hypervisor_page(page_va, &p);

    if (rc) {
        return rc;
    }

    return mmu_unmap_hypervisor_page(&p);
}

int arch_cpu_addr_space_virtualAddr_to_physicalAddr(virtual_addr_t va, physical_addr_t *pa)
{
    int             rc = VMM_OK;
    struct mmu_page p;

    if ((rc = mmu_get_hypervisor_page(va, &p))) {
        return rc;
    }

    *pa = p.oa + (va & (p.size - 1));

    return VMM_OK;
}

virtual_addr_t __init arch_cpu_addr_space_virtual_address_pool_start(void)
{
    return arch_code_vaddr_start();
}

virtual_size_t __init arch_cpu_addr_space_virtual_address_pool_estimate_size(physical_size_t total_ram)
{
    return CONFIG_VIRTUAL_ADDR_POOL_SIZE_MB << 20;
}

static void __init mmu_scan_initial_page_table(struct mmu_page_table *page_table)
{
    int                    i, child_idx;
    arch_pte_t            *pte;
    struct mmu_page_table *child;
    physical_addr_t        child_pa;
    physical_addr_t        ipage_table_start = mmuctrl.ipage_table_base_pa;
    physical_addr_t        ipage_table_end   = ipage_table_start + INIT_PAGE_TABLE_SIZE;

    /* Scan all page table entries */
    for (i = 0; i < (STAGE1_NONROOT_SIZE / sizeof(*pte)); i++) {
        pte = &((arch_pte_t *)page_table->table_va)[i];

        /* Check for valid page table entry */
        if (!arch_mmu_pte_is_valid(pte, page_table->stage, page_table->level)) {
            continue;
        }

        page_table->pte_count++;

        /* Check for child page table */
        if (!arch_mmu_pte_is_table(pte, page_table->stage, page_table->level)) {
            continue;
        }

        /* Current page table level has to be non-zero */
        if (!page_table->level) {
            while (1)
                ;
        }

        /* Find child page table address */
        child_pa = arch_mmu_pte_table_addr(pte, page_table->stage, page_table->level);

        if ((child_pa < ipage_table_start) || (ipage_table_end <= child_pa)) {
            while (1)
                ;
        }

        /* Find child page table pointer */
        child_idx = (child_pa - ipage_table_start) >> STAGE1_NONROOT_ORDER;

        if (INIT_PAGE_TABLE_COUNT <= child_idx) {
            while (1)
                ;
        }

        child = &mmuctrl.ipage_table_pool_array[child_idx];

        if (page_table == child) {
            while (1)
                ;
        }

        /* Handcraft child page table */
        child->parent = page_table;
        child->stage  = page_table->stage;
        child->level  = page_table->level - 1;
        child->attr   = page_table->attr;
        child->map_ia = page_table->map_ia;
        child->map_ia += ((arch_pte_t)i) << arch_mmu_level_index_shift(page_table->stage, page_table->level);

        /* Update page table children */
        page_table->child_count++;
        list_add_tail(&child->head, &page_table->child_list);

        /* Update alloc count */
        mmuctrl.page_table_pool_alloc_count++;

        /* Scan child page table */
        mmu_scan_initial_page_table(child);
    }
}

int __init arch_cpu_addr_space_primary_init(
    physical_addr_t *core_resv_pa, virtual_addr_t *core_resv_va, virtual_size_t *core_resv_sz, physical_addr_t *arch_resv_pa,
    virtual_addr_t *arch_resv_va, virtual_size_t *arch_resv_sz)
{
    int                    i, rc = VMM_ERR_FAIL;
    virtual_addr_t         va, resv_va;
    virtual_size_t         size, resv_sz;
    physical_addr_t        pa, resv_pa;
    struct mmu_page        hyppg;
    struct mmu_page_table *page_table;
    virtual_addr_t         l0_shift = arch_mmu_level_block_shift(MMU_STAGE1, 0);
    virtual_addr_t         l0_size  = arch_mmu_level_block_size(MMU_STAGE1, 0);

    /* Check constraints of generic MMU */
    if (STAGE1_NONROOT_SIZE != l0_size || STAGE1_NONROOT_ALIGN_ORDER > l0_shift) {
        return VMM_ERR_INVALID;
    }

    /* Initial values of resv_va, resv_pa, and resv_sz */
    pa      = arch_code_paddr_start();
    va      = arch_code_vaddr_start();
    size    = arch_code_size();
    resv_va = va + size;
    resv_pa = pa + size;
    resv_sz = 0;

    if (resv_va & (l0_size - 1)) {
        resv_va += l0_size - (resv_va & (l0_size - 1));
    }

    if (resv_pa & (l0_size - 1)) {
        resv_pa += l0_size - (resv_pa & (l0_size - 1));
    }

    /* Initialize MMU control and allocate arch reserved space and
     * update the *arch_resv_pa, *arch_resv_va, and *arch_resv_sz
     * parameters to inform host addr_space about the arch reserved space.
     */
    memset(&mmuctrl, 0, sizeof(mmuctrl));

    *arch_resv_va              = (resv_va + resv_sz);
    *arch_resv_pa              = (resv_pa + resv_sz);
    *arch_resv_sz              = resv_sz;
    mmuctrl.page_table_base_va = resv_va + resv_sz;
    mmuctrl.page_table_base_pa = resv_pa + resv_sz;
    resv_sz += PAGE_TABLE_POOL_SIZE;

    if (resv_sz & (l0_size - 1)) {
        resv_sz += l0_size - (resv_sz & (l0_size - 1));
    }

    *arch_resv_sz               = resv_sz - *arch_resv_sz;
    mmuctrl.ipage_table_base_va = (virtual_addr_t)&stage1_page_table_nonroot;
    mmuctrl.ipage_table_base_pa = mmuctrl.ipage_table_base_va - arch_code_vaddr_start() + arch_code_paddr_start();
    INIT_RW_LOCK(&mmuctrl.page_table_pool_lock);
    mmuctrl.page_table_pool_alloc_count = 0x0;
    INIT_LIST_HEAD(&mmuctrl.page_table_pool_free_list);
    INIT_RW_LOCK(&mmuctrl.page_table_nonpool_lock);
    INIT_LIST_HEAD(&mmuctrl.page_table_nonpool_list);
    INIT_RADIX_TREE(&mmuctrl.page_table_nonpool_tree, 0);

    for (i = 0; i < INIT_PAGE_TABLE_COUNT; i++) {
        page_table = &mmuctrl.ipage_table_pool_array[i];
        memset(page_table, 0, sizeof(struct mmu_page_table));
        page_table->table_phy_addr = mmuctrl.ipage_table_base_pa + i * STAGE1_NONROOT_SIZE;
        INIT_SPIN_LOCK(&page_table->table_lock);
        page_table->table_va   = mmuctrl.ipage_table_base_va + i * STAGE1_NONROOT_SIZE;
        page_table->table_size = STAGE1_NONROOT_SIZE;
        INIT_LIST_HEAD(&page_table->head);
        INIT_LIST_HEAD(&page_table->child_list);
    }

    for (i = 0; i < PAGE_TABLE_POOL_COUNT; i++) {
        page_table = &mmuctrl.page_table_pool_array[i];
        memset(page_table, 0, sizeof(struct mmu_page_table));
        page_table->table_phy_addr = mmuctrl.page_table_base_pa + i * STAGE1_NONROOT_SIZE;
        INIT_SPIN_LOCK(&page_table->table_lock);
        page_table->table_va   = mmuctrl.page_table_base_va + i * STAGE1_NONROOT_SIZE;
        page_table->table_size = STAGE1_NONROOT_SIZE;
        INIT_LIST_HEAD(&page_table->head);
        INIT_LIST_HEAD(&page_table->child_list);
        list_add_tail(&page_table->head, &mmuctrl.page_table_pool_free_list);
    }

    /* Handcraft hypervisor page table */
    page_table = &mmuctrl.hyp_page_table;
    memset(page_table, 0, sizeof(struct mmu_page_table));
    INIT_SPIN_LOCK(&page_table->table_lock);
    page_table->table_va       = (virtual_addr_t)&stage1_page_table_root;
    page_table->table_phy_addr = page_table->table_va - arch_code_vaddr_start() + arch_code_paddr_start();
    page_table->table_size     = STAGE1_ROOT_SIZE;
    INIT_LIST_HEAD(&page_table->head);
    INIT_LIST_HEAD(&page_table->child_list);
    page_table->parent = NULL;
    page_table->stage  = MMU_STAGE1;
    page_table->level  = arch_mmu_start_level(MMU_STAGE1);
    page_table->attr   = MMU_ATTR_REMOTE_TLB_FLUSH;
    page_table->map_ia = 0x0;
    mmu_scan_initial_page_table(page_table);

    for (i = 0; i < INIT_PAGE_TABLE_COUNT; i++) {
        page_table = &mmuctrl.ipage_table_pool_array[i];

        if (page_table->stage == MMU_STAGE_UNKNOWN) {
            list_add_tail(&page_table->head, &mmuctrl.page_table_pool_free_list);
        }
    }

    /* Check & setup core reserved space and update the
     * core_resv_pa, core_resv_va, and core_resv_sz parameters
     * to inform host addr_space about correct placement of the
     * core reserved space.
     */
    *core_resv_pa = resv_pa + resv_sz;
    *core_resv_va = resv_va + resv_sz;

    if (*core_resv_sz & (l0_size - 1)) {
        *core_resv_sz += l0_size - (*core_resv_sz & (l0_size - 1));
    }

    resv_sz += *core_resv_sz;

    /* Map reserved space (core reserved + arch reserved)
     * We have kept our page table pool in reserved area pages
     * as cacheable and write-back. We will clean data cache every
     * time we modify a page table (or translation table) entry.
     */
    pa   = resv_pa;
    va   = resv_va;
    size = resv_sz;

    while (size) {
        memset(&hyppg, 0, sizeof(hyppg));
        hyppg.oa   = pa;
        hyppg.ia   = va;
        hyppg.size = l0_size;
        arch_mmu_pgflags_set(&hyppg.flags, MMU_STAGE1, VMM_MEMORY_FLAGS_NORMAL);

        if ((rc = mmu_map_hypervisor_page(&hyppg))) {
            goto mmu_init_error;
        }

        size -= l0_size;
        pa += l0_size;
        va += l0_size;
    }

    /* Clear memory of free translation tables. This cannot be done before
     * we map reserved space (core reserved + arch reserved).
     */
    list_for_each_entry(page_table, &mmuctrl.page_table_pool_free_list, head)
    {
        memset((void *)page_table->table_va, 0, STAGE1_NONROOT_SIZE);
    }

    return VMM_OK;

mmu_init_error:
    return rc;
}

int __cpuinit arch_cpu_addr_space_secondary_init(void)
{
    /* Nothing to do here. */
    return VMM_OK;
}
