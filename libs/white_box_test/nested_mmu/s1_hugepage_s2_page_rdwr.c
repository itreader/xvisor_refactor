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
 * @file s1_huge_page_s2_page_rdwr.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief s1_huge_page_s2_page_rdwr test implementation
 *
 * This tests the handling of read-write huge_pages in stage1 page tables
 * and pages in stage2 page tables.
 */

#include <vmm_error.h>
#include <vmm_modules.h>

#undef DEBUG

#include "nested_mmu_test.h"

#define MODULE_DESC      "s1_huge_page_s2_page_rdwr test"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (WBOXTEST_IPRIORITY + 1)
#define MODULE_INIT      s1_huge_page_s2_page_rdwr_init
#define MODULE_EXIT      s1_huge_page_s2_page_rdwr_exit

static int s1_huge_page_s2_page_rdwr_run(struct white_box_test *test, vmm_char_device_t *cdev, uint32_t test_hcpu)
{
    int                    rc = VMM_OK;
    struct mmu_page_table *s1_page_table;
    struct mmu_page_table *s2_page_table;
    virtual_addr_t         map_host_va;
    physical_addr_t        map_host_pa;
    physical_addr_t        map_guest_va;
    physical_addr_t        map_guest_pa;
    physical_addr_t        map_nomap_s2_guest_va;
    physical_addr_t        map_nomap_s2_guest_pa;
    physical_addr_t        nomap_guest_va;

    nested_mmu_test_alloc_pages(cdev, test, rc, fail, 1, NESTED_MMU_TEST_RDWR_MEM_FLAGS, &map_host_va, &map_host_pa);

    nested_mmu_test_alloc_page_table(cdev, test, rc, fail_free_host_huge_page, MMU_STAGE1, &s1_page_table);

    nested_mmu_test_alloc_page_table(cdev, test, rc, fail_free_s1_page_table, MMU_STAGE2, &s2_page_table);

    nested_mmu_test_find_free_addr(
        cdev, test, rc, fail_free_s2_page_table, s1_page_table, nested_mmu_test_best_min_addr(s1_page_table), vmm_host_huge_page_shift(),
        &map_guest_va);

    nested_mmu_test_find_free_addr(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, nested_mmu_test_best_min_addr(s2_page_table), vmm_host_huge_page_shift(),
        &map_guest_pa);

    map_nomap_s2_guest_va = map_guest_va + vmm_host_huge_page_size();
    map_nomap_s2_guest_pa = map_guest_pa + vmm_host_huge_page_size();

    nested_mmu_test_map_page_table(
        cdev, test, rc, fail_free_s2_page_table, s1_page_table, map_guest_va, map_guest_pa, vmm_host_huge_page_size(), NESTED_MMU_TEST_RDWR_MEM_FLAGS);

    nested_mmu_test_map_page_table(
        cdev, test, rc, fail_free_s2_page_table, s1_page_table, map_nomap_s2_guest_va, map_nomap_s2_guest_pa, vmm_host_huge_page_size(),
        NESTED_MMU_TEST_RDWR_MEM_FLAGS);

    nested_mmu_test_idmap_stage1(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, vmm_host_huge_page_size(), NESTED_MMU_TEST_RDWR_REG_FLAGS);

    nested_mmu_test_map_page_table(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, map_guest_pa, map_host_pa, VMM_PAGE_SIZE, NESTED_MMU_TEST_RDWR_REG_FLAGS);

    nested_mmu_test_find_free_addr(
        cdev, test, rc, fail_free_s2_page_table, s1_page_table, map_nomap_s2_guest_va + vmm_host_huge_page_size(), vmm_host_huge_page_shift(),
        &nomap_guest_va);

#define chunk_s2_nomap_offset VMM_PAGE_SIZE

#define chunk_start           0
#define chunk_end             (chunk_start + (VMM_PAGE_SIZE / 4))

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_guest_va + chunk_start + sizeof(uint8_t), MMU_TEST_WIDTH_8BIT,
        map_host_pa + chunk_start + sizeof(uint8_t), 0);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_guest_va + chunk_start + chunk_s2_nomap_offset + sizeof(uint8_t),
        MMU_TEST_WIDTH_8BIT, map_guest_pa + chunk_start + chunk_s2_nomap_offset + sizeof(uint8_t), MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_READ);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_nomap_s2_guest_va + chunk_start + sizeof(uint8_t),
        MMU_TEST_WIDTH_8BIT, map_nomap_s2_guest_pa + chunk_start + sizeof(uint8_t), MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_READ);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, nomap_guest_va + chunk_start + sizeof(uint8_t), MMU_TEST_WIDTH_8BIT,
        nomap_guest_va + chunk_start + sizeof(uint8_t), MMU_TEST_FAULT_S1 | MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_READ);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_guest_va + chunk_end - sizeof(uint8_t),
        MMU_TEST_WIDTH_8BIT | MMU_TEST_WRITE, map_host_pa + chunk_end - sizeof(uint8_t), 0);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_guest_va + chunk_end + chunk_s2_nomap_offset - sizeof(uint8_t),
        MMU_TEST_WIDTH_8BIT | MMU_TEST_WRITE, map_guest_pa + chunk_end + chunk_s2_nomap_offset - sizeof(uint8_t),
        MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_WRITE);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_nomap_s2_guest_va + chunk_end - sizeof(uint8_t),
        MMU_TEST_WIDTH_8BIT | MMU_TEST_WRITE, map_nomap_s2_guest_pa + chunk_end - sizeof(uint8_t), MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_WRITE);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, nomap_guest_va + chunk_end - sizeof(uint8_t),
        MMU_TEST_WIDTH_8BIT | MMU_TEST_WRITE, nomap_guest_va + chunk_end - sizeof(uint8_t),
        MMU_TEST_FAULT_S1 | MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_WRITE);

#undef chunk_start
#undef chunk_end

    nested_mmu_test_find_free_addr(
        cdev, test, rc, fail_free_s2_page_table, s1_page_table, nomap_guest_va + vmm_host_huge_page_size(), vmm_host_huge_page_shift(),
        &nomap_guest_va);

#define chunk_start (1 * (VMM_PAGE_SIZE / 4))
#define chunk_end   (chunk_start + (VMM_PAGE_SIZE / 4))

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_guest_va + chunk_start + sizeof(uint16_t), MMU_TEST_WIDTH_16BIT,
        map_host_pa + chunk_start + sizeof(uint16_t), 0);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_guest_va + chunk_start + chunk_s2_nomap_offset + sizeof(uint16_t),
        MMU_TEST_WIDTH_16BIT, map_guest_pa + chunk_start + chunk_s2_nomap_offset + sizeof(uint16_t), MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_READ);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_nomap_s2_guest_va + chunk_start + sizeof(uint16_t),
        MMU_TEST_WIDTH_16BIT, map_nomap_s2_guest_pa + chunk_start + sizeof(uint16_t), MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_READ);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, nomap_guest_va + chunk_start + sizeof(uint16_t), MMU_TEST_WIDTH_16BIT,
        nomap_guest_va + chunk_start + sizeof(uint16_t), MMU_TEST_FAULT_S1 | MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_READ);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_guest_va + chunk_end - sizeof(uint16_t),
        MMU_TEST_WIDTH_16BIT | MMU_TEST_WRITE, map_host_pa + chunk_end - sizeof(uint16_t), 0);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_guest_va + chunk_end + chunk_s2_nomap_offset - sizeof(uint16_t),
        MMU_TEST_WIDTH_16BIT | MMU_TEST_WRITE, map_guest_pa + chunk_end + chunk_s2_nomap_offset - sizeof(uint16_t),
        MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_WRITE);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_nomap_s2_guest_va + chunk_end - sizeof(uint16_t),
        MMU_TEST_WIDTH_16BIT | MMU_TEST_WRITE, map_nomap_s2_guest_pa + chunk_end - sizeof(uint16_t), MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_WRITE);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, nomap_guest_va + chunk_end - sizeof(uint16_t),
        MMU_TEST_WIDTH_16BIT | MMU_TEST_WRITE, nomap_guest_va + chunk_end - sizeof(uint16_t),
        MMU_TEST_FAULT_S1 | MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_WRITE);

#undef chunk_start
#undef chunk_end

    nested_mmu_test_find_free_addr(
        cdev, test, rc, fail_free_s2_page_table, s1_page_table, nomap_guest_va + vmm_host_huge_page_size(), vmm_host_huge_page_shift(),
        &nomap_guest_va);

#define chunk_start (2 * (VMM_PAGE_SIZE / 4))
#define chunk_end   (chunk_start + (VMM_PAGE_SIZE / 4))

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_guest_va + chunk_start + sizeof(uint32_t), MMU_TEST_WIDTH_32BIT,
        map_host_pa + chunk_start + sizeof(uint32_t), 0);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_guest_va + chunk_start + chunk_s2_nomap_offset + sizeof(uint32_t),
        MMU_TEST_WIDTH_32BIT, map_guest_pa + chunk_start + chunk_s2_nomap_offset + sizeof(uint32_t), MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_READ);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_nomap_s2_guest_va + chunk_start + sizeof(uint32_t),
        MMU_TEST_WIDTH_32BIT, map_nomap_s2_guest_pa + chunk_start + sizeof(uint32_t), MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_READ);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, nomap_guest_va + chunk_start + sizeof(uint32_t), MMU_TEST_WIDTH_32BIT,
        nomap_guest_va + chunk_start + sizeof(uint32_t), MMU_TEST_FAULT_S1 | MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_READ);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_guest_va + chunk_end - sizeof(uint32_t),
        MMU_TEST_WIDTH_32BIT | MMU_TEST_WRITE, map_host_pa + chunk_end - sizeof(uint32_t), 0);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_guest_va + chunk_end + chunk_s2_nomap_offset - sizeof(uint32_t),
        MMU_TEST_WIDTH_32BIT | MMU_TEST_WRITE, map_guest_pa + chunk_end + chunk_s2_nomap_offset - sizeof(uint32_t),
        MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_WRITE);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, map_nomap_s2_guest_va + chunk_end - sizeof(uint32_t),
        MMU_TEST_WIDTH_32BIT | MMU_TEST_WRITE, map_nomap_s2_guest_pa + chunk_end - sizeof(uint32_t), MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_WRITE);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, s1_page_table, nomap_guest_va + chunk_end - sizeof(uint32_t),
        MMU_TEST_WIDTH_32BIT | MMU_TEST_WRITE, nomap_guest_va + chunk_end - sizeof(uint32_t),
        MMU_TEST_FAULT_S1 | MMU_TEST_FAULT_NOMAP | MMU_TEST_FAULT_WRITE);

#undef chunk_start
#undef chunk_end

fail_free_s2_page_table:
    nested_mmu_test_free_page_table(cdev, test, s2_page_table);
fail_free_s1_page_table:
    nested_mmu_test_free_page_table(cdev, test, s1_page_table);
fail_free_host_huge_page:
    nested_mmu_test_free_pages(cdev, test, &map_host_va, &map_host_pa, 1);
fail:
    return rc;
}

static struct white_box_test s1_huge_page_s2_page_rdwr = {
    .name = "s1_huge_page_s2_page_rdwr",
    .run  = s1_huge_page_s2_page_rdwr_run,
};

static int __init s1_huge_page_s2_page_rdwr_init(void)
{
    return wboxtest_register("nested_mmu", &s1_huge_page_s2_page_rdwr);
}

static void __exit s1_huge_page_s2_page_rdwr_exit(void)
{
    wboxtest_unregister(&s1_huge_page_s2_page_rdwr);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
