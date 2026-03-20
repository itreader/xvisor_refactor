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
 * @brief: MMU related definition and structures.
 */

#ifndef __CPU_MMU_H_
#define __CPU_MMU_H_

#define NR_PML4_PAGES    1
#define NR_PGDP_PAGES    4
#define NR_PGDI_PAGES    8
#define NR_PGTI_PAGES    32
#define NR_IODEV_PAGES   4

#define VMM_CODE_SEG_SEL 0x08
#define VMM_DATA_SEG_SEL 0x10
#define VMM_TSS_SEG_SEL  0x18

/*
 * Bit width and mask for 4 tree levels used in
 * virtual address mapping. 4 tree levels, 9 bit
 * each cover 36-bit of virtual address and reset
 * of the lower 12-bits out of total 48 bits are
 * used as is for page offset.
 *
 *   63-48   47-39  38-30  29-21  20-12   11-0
 * +---------------------------------------------+
 * | UNSED | PML4 | PGDP | PGDI | PGTI | PG OFSET|
 * +---------------------------------------------+
 */
#define PGTREE_BIT_WIDTH 9
#if !defined(__ASSEMBLY__)
#define PGTREE_MASK ~((0x01UL << PGTREE_BIT_WIDTH) - 1)
#else
#define PGTREE_MASK ~((0x01 << PGTREE_BIT_WIDTH) - 1)
#endif

#define PML4_SHIFT    39
#define PML4_MASK     (PGTREE_MASK << PML4_SHIFT)
#define PML4_MAP_MASK (~((1ULL << PML4_SHIFT) - 1))

#define PGDP_SHIFT    30
#define PGDP_MASK     (PGTREE_MASK << PGDP_SHIFT)
#define PGDP_MAP_MASK (~((1ULL << PGDP_SHIFT) - 1))

#define PGDI_SHIFT    21
#define PGDI_MASK     (PGTREE_MASK << PGDI_SHIFT)
#define PGDI_MAP_MASK (~((1ULL << PGDI_SHIFT) - 1))

#define PGTI_SHIFT    12
#define PGTI_MASK     (PGTREE_MASK << PGTI_SHIFT)
#define PGTI_MAP_MASK (~((1ULL << PGTI_SHIFT) - 1))

#define PAGE_SHIFT    12
#define PAGE_SIZE     (0x01 << PAGE_SHIFT)
#define PAGE_MASK     ~(PAGE_SIZE - 1)

#if !defined(__ASSEMBLY__)

#include <libs/list.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>
#include <vmm_types.h>

extern struct page_table_ctrl host_page_table_ctl;

#define PAGE_SIZE_1G (0x40000000UL)
#define PAGE_SIZE_2M (0x200000UL)
#define PAGE_SIZE_4K (0x1000UL)

static inline void invalidate_vaddr_tlb(virtual_addr_t vaddr)
{
    __asm__ __volatile__("invlpg (%0)\n\t" ::"r"(vaddr) : "memory");
}

union page32 {
    uint32_t _val;

    struct {
        uint32_t present       : 1;
        uint32_t rw            : 1;
        uint32_t priviledge    : 1;
        uint32_t write_through : 1;
        uint32_t cache_disable : 1;
        uint32_t accessed      : 1;
        uint32_t dirty         : 1;
        uint32_t pat           : 1;
        uint32_t global        : 1;
        uint32_t avl           : 3;
        uint32_t paddr         : 20;
    };
};

union page {
    uint64_t _val;

    struct {
        uint64_t present           : 1;
        uint64_t rw                : 1;
        uint64_t priviledge        : 1;
        uint64_t write_through     : 1;
        uint64_t cache_disable     : 1;
        uint64_t accessed          : 1;
        uint64_t dirty             : 1;
        uint64_t pat               : 1;
        uint64_t global            : 1;
        uint64_t ignored           : 3;
        uint64_t paddr             : 40;
        uint64_t reserved          : 11;
        uint64_t execution_disable : 1;
    } bits;
};

#define PGPROT_MASK (~PAGE_MASK)

/* Page Helpers */
static inline bool PageReadOnly(union page32 *pg)
{
    return (!pg->rw);
}

static inline bool PagePresent(union page32 *pg)
{
    return (pg->present);
}

static inline bool PageGlobal(union page32 *pg)
{
    return (pg->global);
}

static inline bool PageCacheable(union page32 *pg)
{
    return (!pg->cache_disable);
}

static inline bool PageDirty(union page32 *pg)
{
    return (pg->dirty);
}

static inline bool PageAccessed(union page32 *pg)
{
    return (pg->accessed);
}

static inline bool PageWriteThrough(union page32 *pg)
{
    return (pg->write_through);
}

static inline void SetPageReadOnly(union page32 *pg)
{
    pg->rw = 0;
}

static inline void SetPageReadWrite(union page32 *pg)
{
    pg->rw = 1;
}

static inline void SetPagePresent(union page32 *pg)
{
    pg->present = 1;
}

static inline void SetPageAbsent(union page32 *pg)
{
    pg->present = 0;
}

static inline void SetPageGlobal(union page32 *pg)
{
    pg->global = 1;
}

static inline void SetPageLocal(union page32 *pg)
{
    pg->global = 0;
}

static inline void SetPageCacheable(union page32 *pg)
{
    pg->cache_disable = 0;
}

static inline void SetPageUncacheable(union page32 *pg)
{
    pg->cache_disable = 1;
}

static inline void SetPageDirty(union page32 *pg)
{
    pg->dirty = 1;
}

static inline void SetPageWashed(union page32 *pg)
{
    pg->dirty = 0;
}

static inline void SetPageAccessed(union page32 *pg)
{
    pg->accessed = 1;
}

static inline void SetPageUnaccessed(union page32 *pg)
{
    pg->accessed = 0;
}

static inline void SetPageWriteThrough(union page32 *pg)
{
    pg->write_through = 1;
}

static inline void SetPageNoWriteThrough(union page32 *pg)
{
    pg->write_through = 0;
}

static inline void SetPageProt(union page32 *pg, uint32_t pgprot)
{
    pg->_val &= ~PGPROT_MASK;
    pg->_val |= pgprot;
}

struct page_table {
    double_list_t      head;
    struct page_table *parent;
    int                level;
    int                stage;
    physical_addr_t    map_ia;
    physical_addr_t    table_phy_addr;
    vmm_spinlock_t     table_lock; /*< Lock to protect table contents,
                          entry_cnt, child_count, and child_list
                      */
    virtual_addr_t     table_va;
    uint32_t           pte_count;
    uint32_t           child_count;
    double_list_t      child_list;
};

union seg_attrs {
    uint16_t bytes;

    struct {
        uint16_t type : 4;
        uint16_t s    : 1;
        uint16_t dpl  : 2;
        uint16_t p    : 1;
        uint16_t avl  : 1;
        uint16_t l    : 1;
        uint16_t db   : 1;
        uint16_t g    : 1;
    } fields;
} __packed;

struct seg_selector {
    uint16_t        sel;
    union seg_attrs attrs;
    uint32_t        limit;
    uint64_t        base;
} __packed;

static inline void dump_seg_selector(const char *seg_name, struct seg_selector *ss)
{
    vmm_printf(
        "%-6s: Sel: 0x%08x Limit: 0x%08x Base: 0x%08lx (G: %2u DB: %2u L: %2u AVL: %2u P: %2u DPL: %2u S: %2u Type: %2d)\n", seg_name, ss->sel,
        ss->limit, ss->base, ss->attrs.fields.g, ss->attrs.fields.db, ss->attrs.fields.l, ss->attrs.fields.avl, ss->attrs.fields.p,
        ss->attrs.fields.dpl, ss->attrs.fields.s, ss->attrs.fields.type);
}

#endif

#endif /* __CPU_MMU_H_ */
