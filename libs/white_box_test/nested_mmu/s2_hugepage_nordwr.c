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
 * @file s2_huge_page_nordwr.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief s2_huge_page_nordwr test implementation
 *
 * This tests the handling no-read-write huge_pages in stage2 page table.
 */

#include <vmm_error.h>
#include <vmm_modules.h>

#undef DEBUG

#include "nested_mmu_test.h"

#define MODULE_DESC      "s2_huge_page_nordwr test"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (WBOXTEST_IPRIORITY + 1)
#define MODULE_INIT      s2_huge_page_nordwr_init
#define MODULE_EXIT      s2_huge_page_nordwr_exit

static int s2_huge_page_nordwr_run(struct white_box_test *test, vmm_char_device_t *cdev, uint32_t test_hcpu)
{
    int                    rc = VMM_OK;
    struct mmu_page_table *s2_page_table;
    physical_addr_t        map_guest_pa;

    nested_mmu_test_alloc_page_table(cdev, test, rc, fail, MMU_STAGE2, &s2_page_table);

    nested_mmu_test_find_free_addr(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, nested_mmu_test_best_min_addr(s2_page_table), vmm_host_huge_page_shift(),
        &map_guest_pa);

    nested_mmu_test_map_page_table(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, map_guest_pa, 0, vmm_host_huge_page_size(), NESTED_MMU_TEST_NORDWR_REG_FLAGS);

#define chunk_start 0
#define chunk_end   (chunk_start + (vmm_host_huge_page_size() / 4))

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, NULL, map_guest_pa + chunk_start + sizeof(uint8_t), MMU_TEST_WIDTH_8BIT,
        map_guest_pa + chunk_start + sizeof(uint8_t), MMU_TEST_FAULT_READ);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, NULL, map_guest_pa + chunk_end - sizeof(uint8_t),
        MMU_TEST_WIDTH_8BIT | MMU_TEST_WRITE, map_guest_pa + chunk_end - sizeof(uint8_t), MMU_TEST_FAULT_WRITE);

#undef chunk_start
#undef chunk_end

#define chunk_start (1 * (vmm_host_huge_page_size() / 4))
#define chunk_end   (chunk_start + (vmm_host_huge_page_size() / 4))

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, NULL, map_guest_pa + chunk_start + sizeof(uint16_t), MMU_TEST_WIDTH_16BIT,
        map_guest_pa + chunk_start + sizeof(uint16_t), MMU_TEST_FAULT_READ);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, NULL, map_guest_pa + chunk_end - sizeof(uint16_t),
        MMU_TEST_WIDTH_16BIT | MMU_TEST_WRITE, map_guest_pa + chunk_end - sizeof(uint16_t), MMU_TEST_FAULT_WRITE);

#undef chunk_start
#undef chunk_end

#define chunk_start (2 * (vmm_host_huge_page_size() / 4))
#define chunk_end   (chunk_start + (vmm_host_huge_page_size() / 4))

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, NULL, map_guest_pa + chunk_start + sizeof(uint32_t), MMU_TEST_WIDTH_32BIT,
        map_guest_pa + chunk_start + sizeof(uint32_t), MMU_TEST_FAULT_READ);

    nested_mmu_test_execute(
        cdev, test, rc, fail_free_s2_page_table, s2_page_table, NULL, map_guest_pa + chunk_end - sizeof(uint32_t),
        MMU_TEST_WIDTH_32BIT | MMU_TEST_WRITE, map_guest_pa + chunk_end - sizeof(uint32_t), MMU_TEST_FAULT_WRITE);

#undef chunk_start
#undef chunk_end

fail_free_s2_page_table:
    nested_mmu_test_free_page_table(cdev, test, s2_page_table);
fail:
    return rc;
}

static struct white_box_test s2_huge_page_nordwr = {
    .name = "s2_huge_page_nordwr",
    .run  = s2_huge_page_nordwr_run,
};

static int __init s2_huge_page_nordwr_init(void)
{
    return wboxtest_register("nested_mmu", &s2_huge_page_nordwr);
}

static void __exit s2_huge_page_nordwr_exit(void)
{
    wboxtest_unregister(&s2_huge_page_nordwr);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
