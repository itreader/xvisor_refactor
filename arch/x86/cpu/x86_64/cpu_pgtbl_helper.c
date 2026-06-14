/**
 * Copyright (c) 2013 Himanshu Chauhan.
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
 * @file cpu_page_table_helper.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic Pagetable handling code. It is shared both by
 *        host MMU code and code that handles guest.
 */

#include <arch_cpu.h>
#include <arch_sections.h>
#include <cpu_mmu.h>
#include <cpu_page_table_helper.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_stdio.h>
#include <vmm_types.h>

static struct page_table *mmu_page_table_find(struct page_table_ctrl *ctrl, physical_addr_t table_pa)
{
    int index;

    table_pa &= ~(PAGE_TABLE_SIZE - 1);

    if (table_pa == ctrl->page_table_pml4.table_pa) {
        return &ctrl->page_table_pml4;
    } else if (table_pa == ctrl->page_table_pgdp.table_pa) {
        return &ctrl->page_table_pgdp;
    } else if (table_pa == ctrl->page_table_pgdi.table_pa) {
        return &ctrl->page_table_pgdi;
    } else if (table_pa >= ctrl->page_table_pgti.table_pa) {
        return &ctrl->page_table_pgti;
    }

    if ((ctrl->page_table_base_pa <= table_pa) && (table_pa <= (ctrl->page_table_base_pa + ctrl->page_table_max_size))) {
        table_pa = table_pa - ctrl->page_table_base_pa;
        index    = table_pa >> PAGE_TABLE_SIZE_SHIFT;

        if (index < ctrl->page_table_max_count) {
            return &ctrl->page_table_array[index];
        }
    }

    return NULL;
}

static inline bool mmu_page_table_isattached(struct page_table *child)
{
    return ((child != NULL) && (child->parent != NULL));
}

static int mmu_page_table_attach(struct page_table *parent, physical_addr_t map_ia, struct page_table *child)
{
    int         index;
    union page *pg;
    irq_flags_t flags;

    if (!parent || !child) {
        return VMM_ERR_FAIL;
    }

    if (mmu_page_table_isattached(child)) {
        return VMM_ERR_FAIL;
    }

    if ((parent->level == PAGE_TABLE_LAST_LEVEL) || (child->stage != parent->stage)) {
        return VMM_ERR_FAIL;
    }

    index = mmu_level_index(map_ia, parent->level);
    pg    = &((union page *)parent->table_va)[index];

    vmm_spin_lock_irq_save(&parent->table_lock, flags);

    if (pg->bits.present) {
        vmm_spin_unlock_irq_restore(&parent->table_lock, flags);
        return VMM_ERR_FAIL;
    }

    pg->bits.paddr   = (child->table_pa & PAGE_MASK) >> PAGE_SHIFT;
    pg->bits.present = 1;
    pg->bits.rw      = 1;

    /* FIXME: flush cache */

    child->parent    = parent;
    child->level     = parent->level + 1;
    child->map_ia    = map_ia & mmu_level_map_mask(parent->level);
    parent->pte_count++;
    parent->child_count++;
    list_add(&child->head, &parent->child_list);

    vmm_spin_unlock_irq_restore(&parent->table_lock, flags);

    return VMM_OK;
}

static int mmu_page_table_deattach(struct page_table *child)
{
    int                index;
    union page        *pg;
    irq_flags_t        flags;
    struct page_table *parent;

    if (!child || !mmu_page_table_isattached(child)) {
        return VMM_ERR_FAIL;
    }

    parent = child->parent;
    index  = mmu_level_index(child->map_ia, parent->level);
    pg     = &((union page *)parent->table_va)[index];

    vmm_spin_lock_irq_save(&parent->table_lock, flags);

    if (!pg->bits.present) {
        vmm_spin_unlock_irq_restore(&parent->table_lock, flags);
        return VMM_ERR_FAIL;
    }

    pg->_val      = 0x0;

    /* FIXME: flush cache */

    child->parent = NULL;
    parent->pte_count--;
    parent->child_count--;
    list_del(&child->head);

    vmm_spin_unlock_irq_restore(&parent->table_lock, flags);

    return VMM_OK;
}

struct page_table *mmu_page_table_alloc(struct page_table_ctrl *ctrl, int stage)
{
    irq_flags_t        flags;
    double_list_t     *l;
    struct page_table *page_table;

    vmm_spin_lock_irq_save(&ctrl->alloc_lock, flags);

    if (list_empty(&ctrl->free_page_table_list)) {
        vmm_spin_unlock_irq_restore(&ctrl->alloc_lock, flags);
        return NULL;
    }

    l          = list_pop(&ctrl->free_page_table_list);
    page_table = list_entry(l, struct page_table, head);
    ctrl->page_table_alloc_count++;

    vmm_spin_unlock_irq_restore(&ctrl->alloc_lock, flags);

    page_table->parent = NULL;
    page_table->stage  = stage;
    page_table->level  = PAGE_TABLE_FIRST_LEVEL;
    page_table->map_ia = 0;
    INIT_SPIN_LOCK(&page_table->table_lock);
    page_table->pte_count   = 0;
    page_table->child_count = 0;
    INIT_LIST_HEAD(&page_table->child_list);

    return page_table;
}

int mmu_page_table_free(struct page_table_ctrl *ctrl, struct page_table *page_table)
{
    int                rc = VMM_OK;
    irq_flags_t        flags;
    double_list_t     *l;
    struct page_table *child;

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
        child = list_entry(l, struct page_table, head);

        if ((rc = mmu_page_table_deattach(child))) {
            return rc;
        }

        if ((rc = mmu_page_table_free(ctrl, child))) {
            return rc;
        }
    }

    vmm_spin_lock_irq_save(&page_table->table_lock, flags);
    page_table->pte_count = 0;
    memset((void *)page_table->table_va, 0, PAGE_TABLE_SIZE);
    vmm_spin_unlock_irq_restore(&page_table->table_lock, flags);

    page_table->level  = PAGE_TABLE_FIRST_LEVEL;
    page_table->map_ia = 0;

    vmm_spin_lock_irq_save(&ctrl->alloc_lock, flags);
    list_add_tail(&page_table->head, &ctrl->free_page_table_list);
    ctrl->page_table_alloc_count--;
    vmm_spin_unlock_irq_restore(&ctrl->alloc_lock, flags);

    return VMM_OK;
}

struct page_table *mmu_page_table_get_child(struct page_table_ctrl *ctrl, struct page_table *parent, physical_addr_t map_ia, bool create)
{
    int                rc, index;
    union page        *pg, pgt;
    irq_flags_t        flags;
    physical_addr_t    table_pa;
    struct page_table *child;

    if (!parent) {
        return NULL;
    }

    index = mmu_level_index(map_ia, parent->level);
    pg    = &((union page *)parent->table_va)[index];

    vmm_spin_lock_irq_save(&parent->table_lock, flags);
    pgt._val = pg->_val;
    vmm_spin_unlock_irq_restore(&parent->table_lock, flags);

    if (pgt.bits.present) {
        table_pa = pgt._val & PAGE_MASK;
        child    = mmu_page_table_find(ctrl, table_pa);

        if (child->parent == parent) {
            return child;
        }

        return NULL;
    }

    if (!create) {
        return NULL;
    }

    child = mmu_page_table_alloc(ctrl, parent->stage);

    if (!child) {
        return NULL;
    }

    if ((rc = mmu_page_table_attach(parent, map_ia, child))) {
        mmu_page_table_free(ctrl, child);
    }

    return child;
}

int mmu_get_page(struct page_table_ctrl *ctrl, struct page_table *page_table, physical_addr_t ia, union page *pg)
{
    int                index, pre_index;
    irq_flags_t        flags;
    union page        *pgt;
    struct page_table *child;
    virtual_addr_t     pgt_va;

    if (!page_table || !pg) {
        return VMM_ERR_FAIL;
    }

    index = mmu_level_index(ia, page_table->level);

    if (page_table->level == PAGE_TABLE_LAST_LEVEL) {
        pre_index = mmu_level_index(ia, (page_table->level - 1));
        pgt_va    = page_table->table_va + (pre_index * PAGE_SIZE);
    } else {
        pgt_va = page_table->table_va;
    }

    pgt = &((union page *)pgt_va)[index];

    vmm_spin_lock_irq_save(&page_table->table_lock, flags);

    if (!pgt->bits.present) {
        vmm_spin_unlock_irq_restore(&page_table->table_lock, flags);
        return VMM_ERR_FAIL;
    }

    if (page_table->level < PAGE_TABLE_LAST_LEVEL) {
        vmm_spin_unlock_irq_restore(&page_table->table_lock, flags);
        child = mmu_page_table_get_child(ctrl, page_table, ia, FALSE);

        if (!child) {
            return VMM_ERR_FAIL;
        }

        return mmu_get_page(ctrl, child, ia, pg);
    }

    pg->_val = pgt->_val;

    vmm_spin_unlock_irq_restore(&page_table->table_lock, flags);

    return VMM_OK;
}

int mmu_unmap_page(struct page_table_ctrl *ctrl, struct page_table *page_table, physical_addr_t ia)
{
    int                index, rc, pre_index;
    bool               free_page_table;
    union page        *pgt;
    irq_flags_t        flags;
    struct page_table *child;
    virtual_addr_t     pgt_va;

    if (!page_table) {
        return VMM_ERR_FAIL;
    }

    if (page_table->level < PAGE_TABLE_LAST_LEVEL) {
        child = mmu_page_table_get_child(ctrl, page_table, ia, FALSE);

        if (!child) {
            return VMM_ERR_FAIL;
        }

        rc = mmu_unmap_page(ctrl, child, ia);

        if ((page_table->pte_count == 0) && (page_table->level > PAGE_TABLE_FIRST_LEVEL)) {
            mmu_page_table_free(ctrl, page_table);
        }

        return rc;
    }

    index     = mmu_level_index(ia, page_table->level);
    pre_index = mmu_level_index(ia, (page_table->level - 1));
    pgt_va    = page_table->table_va + (pre_index * PAGE_SIZE);
    pgt       = &((union page *)pgt_va)[index];

    vmm_spin_lock_irq_save(&page_table->table_lock, flags);

    if (!pgt->bits.present) {
        vmm_spin_unlock_irq_restore(&page_table->table_lock, flags);
        return VMM_ERR_FAIL;
    }

    pgt->_val = 0x0;

    invalidate_vaddr_tlb(ia);

    page_table->pte_count--;
    free_page_table = FALSE;

    if ((page_table->pte_count == 0) && (page_table->level > PAGE_TABLE_FIRST_LEVEL)) {
        free_page_table = TRUE;
    }

    vmm_spin_unlock_irq_restore(&page_table->table_lock, flags);

    if (free_page_table) {
        mmu_page_table_free(ctrl, page_table);
    }

    return VMM_OK;
}

int mmu_map_page(struct page_table_ctrl *ctrl, struct page_table *page_table, physical_addr_t ia, union page *pg)
{
    int                index, pre_index;
    union page        *pgt;
    irq_flags_t        flags;
    struct page_table *child;
    virtual_addr_t     pgt_va;

    if (!page_table || !pg) {
        return VMM_ERR_FAIL;
    }

    if (page_table->level < PAGE_TABLE_LAST_LEVEL) {
        child = mmu_page_table_get_child(ctrl, page_table, ia, TRUE);

        if (!child) {
            return VMM_ERR_FAIL;
        }

        return mmu_map_page(ctrl, child, ia, pg);
    }

    index     = mmu_level_index(ia, page_table->level);
    pre_index = mmu_level_index(ia, (page_table->level - 1));
    pgt_va    = page_table->table_va + (pre_index * PAGE_SIZE);
    pgt       = &((union page *)pgt_va)[index];

    vmm_spin_lock_irq_save(&page_table->table_lock, flags);

    if (pgt->bits.present) {
        vmm_spin_unlock_irq_restore(&page_table->table_lock, flags);
        return VMM_ERR_FAIL;
    }

    pgt->_val = 0x0;
    pgt->_val = pg->_val;

    /* FIXME: flush cache */

    page_table->pte_count++;

    vmm_spin_unlock_irq_restore(&page_table->table_lock, flags);

    return VMM_OK;
}
