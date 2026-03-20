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
 * @file cpu_mmu.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Memory management code.
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

#define HOST_PAGE_TABLE_MAX_TABLE_COUNT (CONFIG_VAPOOL_SIZE_MB << (20 - 3 - PAGE_TABLE_SIZE_SHIFT))
#define HOST_PAGE_TABLE_MAX_TABLE_SIZE  (HOST_PAGE_TABLE_MAX_TABLE_COUNT * PAGE_TABLE_SIZE)

uint64_t __force_order;

struct page_table_ctrl host_page_table_ctl;

/* initial bootstrap page tables */
extern uint64_t __pml4[];
extern uint64_t __pgdp[];
extern uint64_t __pgdi[];
extern uint64_t __pgti[];
extern uint64_t __early_iodev_pages[];

static char      *early_iodev_pages      = (char *)__early_iodev_pages;
static int        early_iodev_pages_used = 0;
struct page_table host_page_table_array[HOST_PAGE_TABLE_MAX_TABLE_COUNT];

static char *alloc_iodev_page(void)
{
    char *iodev_page = NULL;

    if (early_iodev_pages_used >= NR_IODEV_PAGES) {
        return NULL;
    }

    iodev_page = (early_iodev_pages + (early_iodev_pages_used * PAGE_SIZE));
    early_iodev_pages_used++;

    return iodev_page;
}

static void arch_preinit_pgtable_entries(void)
{
    int   i;
    char *pgdp_base, *pgdi_base, *pgti_base;

    pgdp_base = (char *)__pgdp;
    pgdi_base = (char *)__pgdi;
    pgti_base = (char *)__pgti;

    for (i = 0; i < NR_PGDP_PAGES; i++) {
        __pml4[i] = ((uint64_t)(pgdp_base + (PAGE_SIZE * i)) & PAGE_MASK) + 3;
    }

    for (i = 0; i < NR_PGDI_PAGES; i++) {
        __pgdp[i] = ((uint64_t)(pgdi_base + (PAGE_SIZE * i)) & PAGE_MASK) + 3;
    }

    for (i = 0; i < NR_PGTI_PAGES; i++) {
        __pgdi[i] = ((uint64_t)(pgti_base + (PAGE_SIZE * i)) & PAGE_MASK) + 3;
    }
}

int __create_bootstrap_page_table_entry(uint64_t va, uint64_t pa, uint32_t page_size, uint8_t wt, uint8_t cd)
{
    static int preinit_pgtables = 0;
    union page ent;

    ent._val = 0;

    if ((page_size != PAGE_SIZE_2M) && (page_size != PAGE_SIZE_4K)) {
        return VMM_EFAIL;
    }

    if (!preinit_pgtables) {
        arch_preinit_pgtable_entries();
        preinit_pgtables = 1;
    }

    uint64_t pml4_index = ((va >> PML4_SHIFT) & 0x1ff);
    uint64_t pgdp_index = ((va >> PGDP_SHIFT) & 0x1ff);
    uint64_t pgdi_index = ((va >> PGDI_SHIFT) & 0x1ff);
    uint64_t pgti_index = ((va >> PGTI_SHIFT) & 0x1ff);

    if (!(__pml4[pml4_index] & 0x3)) {
        return VMM_EFAIL;
    }

    uint64_t *pgdp_base = (uint64_t *)(((uint64_t)__pml4[pml4_index]) & ~0xff);

    if (pgdp_base[pgdp_index] == 0) {
        pgdp_base[pgdp_index] = (uint64_t)alloc_iodev_page();

        if ((void *)(pgdp_base[pgdp_index]) == NULL) {
            return VMM_EFAIL;
        }

        pgdp_base[pgdp_index] |= 0x3;
    } else {
        if (!(pgdp_base[pgdp_index] & 0x3)) {
            return VMM_EFAIL;
        }
    }

    uint64_t *pgdi_base = (uint64_t *)(((uint64_t)pgdp_base[pgdp_index]) & ~0xff);

    if (pgdi_base[pgdi_index] == 0) {
        if (page_size == PAGE_SIZE_2M) {
            pgdi_base[pgdi_index] |= ((pa >> 21) << 21);
            pgdi_base[pgdi_index] |= (0x1UL << 7);
            pgdi_base[pgdi_index] |= (wt ? (0x1UL << 3) : 0);
            pgdi_base[pgdi_index] |= (cd ? (0x1UL << 4) : 0);
            pgdi_base[pgdi_index] |= 0x3;
            return VMM_OK;
        } else {
            pgdi_base[pgdi_index] = (uint64_t)alloc_iodev_page();

            if ((void *)pgdi_base[pgdi_index] == NULL) {
                return VMM_EFAIL;
            }

            pgdi_base[pgdi_index] |= 0x3;
        }
    } else {
        if (!(pgdi_base[pgdi_index] & 0x3)) {
            return VMM_EFAIL;
        }
    }

    uint64_t *pgti_base = (uint64_t *)(((uint64_t)pgdi_base[pgdi_index]) & ~0xff);

    if (pgti_base[pgti_index] == 0) {
        pgti_base[pgti_index] = (uint64_t)alloc_iodev_page();

        if ((void *)pgti_base[pgti_index] == NULL) {
            return VMM_EFAIL;
        }

        pgti_base[pgti_index] |= 0x3;
    } else {
        if (pgti_base[pgti_index] & 0x3) {
            return VMM_EFAIL;
        }
    }

    ent.bits.paddr        = pa >> PAGE_SHIFT;
    ent.bits.present      = 1;
    ent.bits.rw           = 1;
    pgti_base[pgti_index] = ent._val;
    pgti_base[pgti_index] |= (wt ? (0x1UL << 3) : 0);
    pgti_base[pgti_index] |= (cd ? (0x1UL << 4) : 0);

    invalidate_vaddr_tlb(va);

    return VMM_OK;
}

int __delete_bootstrap_page_table_entry(uint64_t va)
{
    union page ent;

    ent._val            = 0;

    uint64_t pml4_index = ((va >> PML4_SHIFT) & 0x1ff);
    uint64_t pgdp_index = ((va >> PGDP_SHIFT) & 0x1ff);
    uint64_t pgdi_index = ((va >> PGDI_SHIFT) & 0x1ff);
    uint64_t pgti_index = ((va >> PGTI_SHIFT) & 0x1ff);

    if (!(__pml4[pml4_index] & 0x3)) {
        return VMM_EFAIL;
    }

    uint64_t *pgdp_base = (uint64_t *)(((uint64_t)__pml4[pml4_index]) & ~0xff);

    if (!(pgdp_base[pgdp_index] & 0x3)) {
        return VMM_EFAIL;
    }

    uint64_t *pgdi_base = (uint64_t *)(((uint64_t)pgdp_base[pgdp_index]) & ~0xff);

    if (!(pgdi_base[pgdp_index] & 0x3)) {
        return VMM_EFAIL;
    }

    uint64_t *pgti_base = (uint64_t *)(((uint64_t)pgdi_base[pgdi_index]) & ~0xff);

    if (!(pgti_base[pgti_index] & 0x3)) {
        return VMM_EFAIL;
    }

    ent._val              = pgti_base[pgti_index];
    ent.bits.paddr        = 0;
    ent.bits.present      = 0;
    ent.bits.rw           = 0;
    pgti_base[pgti_index] = ent._val;

    invalidate_vaddr_tlb(va);

    return VMM_OK;
}

void arch_cpu_addr_space_print_info(vmm_char_device_t *cdev)
{
    /* Nothing to do here. */
}

uint32_t arch_cpu_addr_space_hugepage_log2size(void)
{
    /* FIXME: hugepage support will be added in-future */
    return PAGE_SHIFT;
}

/* mmu inline asm routines */
int arch_cpu_addr_space_map(virtual_addr_t page_va, virtual_size_t page_sz, physical_addr_t page_pa, uint32_t mem_flags)
{
    union page pg;

    if (page_sz != PAGE_SIZE) {
        return VMM_EINVALID;
    }

    /* FIXME: more specific page attributes */
    pg._val         = 0x0;
    pg.bits.paddr   = (page_pa >> PAGE_SHIFT);
    pg.bits.present = 1;
    pg.bits.rw      = 1;

    if (!(mem_flags & VMM_MEMORY_CACHEABLE)) {
        pg.bits.cache_disable = 1;
    }

    if (!(mem_flags & VMM_MEMORY_WRITEABLE)) {
        pg.bits.rw = 0;
    }

    return mmu_map_page(&host_page_table_ctl, host_page_table_ctl.base_page_table, page_va, &pg);
}

int arch_cpu_addr_space_unmap(virtual_addr_t page_va)
{
    return mmu_unmap_page(&host_page_table_ctl, host_page_table_ctl.base_page_table, page_va);
}

int arch_cpu_addr_space_va2pa(virtual_addr_t va, physical_addr_t *pa)
{
    int        rc;
    union page pg;
    uint64_t   fpa;

    rc = mmu_get_page(&host_page_table_ctl, host_page_table_ctl.base_page_table, va, &pg);

    if (rc) {
        return rc;
    }

    fpa = (pg.bits.paddr << PAGE_SHIFT);
    fpa |= va & ~PAGE_MASK;

    *pa = fpa;

    return VMM_OK;
}

virtual_addr_t __init arch_cpu_addr_space_virtual_address_pool_start(void)
{
    return arch_code_vaddr_start();
}

virtual_size_t __init arch_cpu_addr_space_virtual_address_pool_estimate_size(physical_size_t total_ram)
{
    return CONFIG_VAPOOL_SIZE_MB << 20;
}

int __init arch_cpu_addr_space_primary_init(
    physical_addr_t *core_resv_pa, virtual_addr_t *core_resv_va, virtual_size_t *core_resv_sz, physical_addr_t *arch_resv_pa,
    virtual_addr_t *arch_resv_va, virtual_size_t *arch_resv_sz)
{
    int                i;
    int                t;
    int                rc = VMM_EFAIL;
    virtual_addr_t     va;
    virtual_addr_t     resv_va = *core_resv_va;
    virtual_size_t     size;
    virtual_size_t     resv_sz = *core_resv_sz;
    physical_addr_t    pa;
    physical_addr_t    resv_pa = *core_resv_pa;
    union page        *pg;
    union page         hyppg;
    struct page_table *page_table;

    /*
     * Boot code didn't populate all the entries in the
     * page tables. Initialize all of them now so that
     * later we only have to handle PTE mappings. This
     * means that the code can map uptil PTEs for all
     * code and virtual_address_pool addresses.
     */
    arch_preinit_pgtable_entries();

    /* Check & setup core reserved space and update the
     * core_resv_pa, core_resv_va, and core_resv_sz parameters
     * to inform host aspace about correct placement of the
     * core reserved space.
     */
    pa      = arch_code_paddr_start();
    va      = arch_code_vaddr_start();
    size    = arch_code_size();
    resv_va = va + size;
    resv_pa = pa + size;

    if (resv_va & (PAGE_SIZE - 1)) {
        resv_va += PAGE_SIZE - (resv_va & (PAGE_SIZE - 1));
    }

    if (resv_pa & (PAGE_SIZE - 1)) {
        resv_pa += PAGE_SIZE - (resv_pa & (PAGE_SIZE - 1));
    }

    if (resv_sz & (PAGE_SIZE - 1)) {
        resv_sz += PAGE_SIZE - (resv_sz & (PAGE_SIZE - 1));
    }

    *core_resv_pa = resv_pa;
    *core_resv_va = resv_va;
    *core_resv_sz = resv_sz;

    /* Initialize MMU control and allocate arch reserved space and
     * update the *arch_resv_pa, *arch_resv_va, and *arch_resv_sz
     * parameters to inform host aspace about the arch reserved space.
     */
    memset(&host_page_table_ctl, 0, sizeof(host_page_table_ctl));
    memset(host_page_table_array, 0, sizeof(host_page_table_array));
    host_page_table_ctl.page_table_array     = &host_page_table_array[0];
    host_page_table_ctl.page_table_max_size  = HOST_PAGE_TABLE_MAX_TABLE_SIZE;
    host_page_table_ctl.page_table_max_count = HOST_PAGE_TABLE_MAX_TABLE_COUNT;
    *arch_resv_va                            = (resv_va + resv_sz);
    *arch_resv_pa                            = (resv_pa + resv_sz);
    *arch_resv_sz                            = resv_sz;
    host_page_table_ctl.page_table_base_va   = resv_va + resv_sz;
    host_page_table_ctl.page_table_base_pa   = resv_pa + resv_sz;
    resv_sz += PAGE_TABLE_SIZE * HOST_PAGE_TABLE_MAX_TABLE_COUNT;
    *arch_resv_sz = resv_sz - *arch_resv_sz;
    INIT_SPIN_LOCK(&host_page_table_ctl.alloc_lock);
    host_page_table_ctl.page_table_alloc_count = 0x0;
    INIT_LIST_HEAD(&host_page_table_ctl.free_page_table_list);

    for (i = 0; i < HOST_PAGE_TABLE_MAX_TABLE_COUNT; i++) {
        page_table = &host_page_table_ctl.page_table_array[i];
        memset(page_table, 0, sizeof(struct page_table));
        page_table->table_pa = host_page_table_ctl.page_table_base_pa + i * PAGE_TABLE_SIZE;
        INIT_SPIN_LOCK(&page_table->table_lock);
        page_table->table_va = host_page_table_ctl.page_table_base_va + i * PAGE_TABLE_SIZE;
        INIT_LIST_HEAD(&page_table->head);
        INIT_LIST_HEAD(&page_table->child_list);
        list_add_tail(&page_table->head, &host_page_table_ctl.free_page_table_list);
    }

    /* Handcraft bootstrap pml4 */
    page_table = &host_page_table_ctl.page_table_pml4;
    memset(page_table, 0, sizeof(struct page_table));
    page_table->level    = 0;
    page_table->stage    = 0;
    page_table->parent   = NULL;
    page_table->map_ia   = 0;
    page_table->table_pa = (virtual_addr_t)__pml4 - arch_code_vaddr_start() + arch_code_paddr_start();
    INIT_SPIN_LOCK(&page_table->table_lock);
    page_table->table_va = (virtual_addr_t)__pml4;
    INIT_LIST_HEAD(&page_table->head);
    INIT_LIST_HEAD(&page_table->child_list);
    host_page_table_ctl.page_table_alloc_count++;

    for (t = 0; t < PAGE_TABLE_ENTCNT; t++) {
        pg = &((union page *)page_table->table_va)[t];

        if (pg->bits.present) {
            page_table->pte_count++;
        }
    }

    /* Handcraft bootstrap pgdp */
    page_table = &host_page_table_ctl.page_table_pgdp;
    memset(page_table, 0, sizeof(struct page_table));
    page_table->level    = 1;
    page_table->stage    = 0;
    page_table->parent   = &host_page_table_ctl.page_table_pml4;
    page_table->map_ia   = arch_code_vaddr_start() & mmu_level_map_mask(0);
    page_table->table_pa = (virtual_addr_t)__pgdp - arch_code_vaddr_start() + arch_code_paddr_start();
    INIT_SPIN_LOCK(&page_table->table_lock);
    page_table->table_va = (virtual_addr_t)__pgdp;
    INIT_LIST_HEAD(&page_table->head);
    INIT_LIST_HEAD(&page_table->child_list);
    host_page_table_ctl.page_table_alloc_count++;

    for (t = 0; t < PAGE_TABLE_ENTCNT; t++) {
        pg = &((union page *)page_table->table_va)[t];

        if (pg->bits.present) {
            page_table->pte_count++;
        }
    }

    list_add_tail(&page_table->head, &host_page_table_ctl.page_table_pml4.child_list);
    host_page_table_ctl.page_table_pml4.child_count++;

    /* Handcraft bootstrap pgdi */
    page_table = &host_page_table_ctl.page_table_pgdi;
    memset(page_table, 0, sizeof(struct page_table));
    page_table->level    = 2;
    page_table->stage    = 0;
    page_table->parent   = &host_page_table_ctl.page_table_pgdp;
    page_table->map_ia   = arch_code_vaddr_start() & mmu_level_map_mask(1);
    page_table->table_pa = (virtual_addr_t)__pgdi - arch_code_vaddr_start() + arch_code_paddr_start();
    INIT_SPIN_LOCK(&page_table->table_lock);
    page_table->table_va = (virtual_addr_t)__pgdi;
    INIT_LIST_HEAD(&page_table->head);
    INIT_LIST_HEAD(&page_table->child_list);
    host_page_table_ctl.page_table_alloc_count++;

    for (t = 0; t < PAGE_TABLE_ENTCNT; t++) {
        pg = &((union page *)page_table->table_va)[t];

        if (pg->bits.present) {
            page_table->pte_count++;
        }
    }

    list_add_tail(&page_table->head, &host_page_table_ctl.page_table_pgdp.child_list);
    host_page_table_ctl.page_table_pgdp.child_count++;

    /* Handcraft bootstrap pgti */
    page_table = &host_page_table_ctl.page_table_pgti;
    memset(page_table, 0, sizeof(struct page_table));
    page_table->level    = 3;
    page_table->stage    = 0;
    page_table->parent   = &host_page_table_ctl.page_table_pgdi;
    page_table->map_ia   = arch_code_vaddr_start() & mmu_level_map_mask(2);
    page_table->table_pa = (virtual_addr_t)__pgti - arch_code_vaddr_start() + arch_code_paddr_start();
    INIT_SPIN_LOCK(&page_table->table_lock);
    page_table->table_va = (virtual_addr_t)__pgti;
    INIT_LIST_HEAD(&page_table->head);
    INIT_LIST_HEAD(&page_table->child_list);
    host_page_table_ctl.page_table_alloc_count++;

    for (t = 0; t < PAGE_TABLE_ENTCNT; t++) {
        pg = &((union page *)page_table->table_va)[t];

        if (pg->bits.present) {
            page_table->pte_count++;
        }
    }

    list_add_tail(&page_table->head, &host_page_table_ctl.page_table_pgdi.child_list);
    host_page_table_ctl.page_table_pgdi.child_count++;

    /* Point hypervisor table to bootstrap pml4 */
    host_page_table_ctl.base_page_table = &host_page_table_ctl.page_table_pml4;

    /* Map reserved space (core reserved + arch reserved)
     * We have kept our page table pool in reserved area pages
     * as cacheable and write-back. We will clean data cache every
     * time we modify a page table (or translation table) entry.
     */
    pa                                  = resv_pa;
    va                                  = resv_va;
    size                                = resv_sz;

    while (size) {
        /* FIXME: more specific page attributes */
        hyppg._val         = 0x0;
        hyppg.bits.paddr   = (pa >> PAGE_SHIFT);
        hyppg.bits.present = 1;
        hyppg.bits.rw      = 1;

        if ((rc = mmu_map_page(&host_page_table_ctl, host_page_table_ctl.base_page_table, va, &hyppg))) {
            goto mmu_init_error;
        }

        size -= PAGE_SIZE;
        pa += PAGE_SIZE;
        va += PAGE_SIZE;
    }

    /* Clear memory of free translation tables. This cannot be done before
     * we map reserved space (core reserved + arch reserved).
     */
    list_for_each_entry(page_table, &host_page_table_ctl.free_page_table_list, head)
    {
        memset((void *)page_table->table_va, 0, PAGE_TABLE_SIZE);
    }

    return VMM_OK;

mmu_init_error:
    return rc;
}

int __cpuinit arch_cpu_addr_space_secondary_init(void)
{
    /* FIXME: For now nothing to do here. */
    return VMM_OK;
}
