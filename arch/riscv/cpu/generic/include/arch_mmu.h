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
 * @file arch_mmu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Arch MMU interface header
 */

#ifndef __ARCH_MMU_H__
#define __ARCH_MMU_H__

#include <vmm_const.h>

#define ARCH_MMU_STAGE1_ROOT_SIZE_ORDER       12
#define ARCH_MMU_STAGE1_ROOT_ALIGN_ORDER      12

#define ARCH_MMU_STAGE1_NONROOT_INITIAL_COUNT 8

#define ARCH_MMU_STAGE1_NONROOT_SIZE_ORDER    12
#define ARCH_MMU_STAGE1_NONROOT_ALIGN_ORDER   12

#ifdef CONFIG_64BIT
/* L4 index Bit[56:48] */
#define PGTBL_L4_INDEX_MASK  0x01FF000000000000ULL
#define PGTBL_L4_INDEX_SHIFT 48
#define PGTBL_L4_BLOCK_SHIFT 48
#define PGTBL_L4_BLOCK_SIZE  0x0001000000000000ULL
#define PGTBL_L4_MAP_MASK    (~(PGTBL_L4_BLOCK_SIZE - 1))
/* L3 index Bit[47:39] */
#define PGTBL_L3_INDEX_MASK  0x0000FF8000000000ULL
#define PGTBL_L3_INDEX_SHIFT 39
#define PGTBL_L3_BLOCK_SHIFT 39
#define PGTBL_L3_BLOCK_SIZE  0x0000008000000000ULL
#define PGTBL_L3_MAP_MASK    (~(PGTBL_L3_BLOCK_SIZE - 1))
/* L2 index Bit[38:30] */
#define PGTBL_L2_INDEX_MASK  0x0000007FC0000000ULL
#define PGTBL_L2_INDEX_SHIFT 30
#define PGTBL_L2_BLOCK_SHIFT 30
#define PGTBL_L2_BLOCK_SIZE  0x0000000040000000ULL
#define PGTBL_L2_MAP_MASK    (~(PGTBL_L2_BLOCK_SIZE - 1))
/* L1 index Bit[29:21] */
#define PGTBL_L1_INDEX_MASK  0x000000003FE00000ULL
#define PGTBL_L1_INDEX_SHIFT 21
#define PGTBL_L1_BLOCK_SHIFT 21
#define PGTBL_L1_BLOCK_SIZE  0x0000000000200000ULL
#define PGTBL_L1_MAP_MASK    (~(PGTBL_L1_BLOCK_SIZE - 1))
/* L0 index Bit[20:12] */
#define PGTBL_L0_INDEX_MASK  0x00000000001FF000ULL
#define PGTBL_L0_INDEX_SHIFT 12
#define PGTBL_L0_BLOCK_SHIFT 12
#define PGTBL_L0_BLOCK_SIZE  0x0000000000001000ULL
#define PGTBL_L0_MAP_MASK    (~(PGTBL_L0_BLOCK_SIZE - 1))
#else
/* L1 index Bit[31:22] */
#define PGTBL_L1_INDEX_MASK  0xFFC00000UL
#define PGTBL_L1_INDEX_SHIFT 22
#define PGTBL_L1_BLOCK_SHIFT 22
#define PGTBL_L1_BLOCK_SIZE  0x00400000UL
#define PGTBL_L1_MAP_MASK    (~(PGTBL_L1_BLOCK_SIZE - 1))
/* L0 index Bit[21:12] */
#define PGTBL_L0_INDEX_MASK  0x003FF000UL
#define PGTBL_L0_INDEX_SHIFT 12
#define PGTBL_L0_BLOCK_SHIFT 12
#define PGTBL_L0_BLOCK_SIZE  0x00001000UL
#define PGTBL_L0_MAP_MASK    (~(PGTBL_L0_BLOCK_SIZE - 1))
#endif

#define PGTBL_PTE_ADDR_MASK      0x003FFFFFFFFFFC00ULL
#define PGTBL_PTE_ADDR_SHIFT     10
#define PGTBL_PTE_RSW_MASK       0x0000000000000300ULL
#define PGTBL_PTE_RSW_SHIFT      8
#define PGTBL_PTE_DIRTY_MASK     0x0000000000000080ULL
#define PGTBL_PTE_DIRTY_SHIFT    7
#define PGTBL_PTE_ACCESSED_MASK  0x0000000000000040ULL
#define PGTBL_PTE_ACCESSED_SHIFT 6
#define PGTBL_PTE_GLOBAL_MASK    0x0000000000000020ULL
#define PGTBL_PTE_GLOBAL_SHIFT   5
#define PGTBL_PTE_USER_MASK      0x0000000000000010ULL
#define PGTBL_PTE_USER_SHIFT     4
#define PGTBL_PTE_EXECUTE_MASK   0x0000000000000008ULL
#define PGTBL_PTE_EXECUTE_SHIFT  3
#define PGTBL_PTE_WRITE_MASK     0x0000000000000004ULL
#define PGTBL_PTE_WRITE_SHIFT    2
#define PGTBL_PTE_READ_MASK      0x0000000000000002ULL
#define PGTBL_PTE_READ_SHIFT     1
#define PGTBL_PTE_PERM_MASK      (PGTBL_PTE_EXECUTE_MASK | PGTBL_PTE_WRITE_MASK | PGTBL_PTE_READ_MASK)
#define PGTBL_PTE_VALID_MASK     0x0000000000000001ULL
#define PGTBL_PTE_VALID_SHIFT    0

#define PGTBL_PAGE_SIZE          PGTBL_L0_BLOCK_SIZE
#define PGTBL_PAGE_SIZE_SHIFT    PGTBL_L0_BLOCK_SHIFT

#ifndef __ASSEMBLY__

#include <vmm_types.h>

extern uint64_t riscv_stage1_mode;

#ifdef CONFIG_64BIT
typedef uint64_t arch_pte_t;
#else
typedef uint32_t arch_pte_t;
#endif

struct arch_pgflags {
    uint8_t rsw;
    uint8_t dirty;
    uint8_t accessed;
    uint8_t global;
    uint8_t user;
    uint8_t execute;
    uint8_t write;
    uint8_t read;
    uint8_t valid;
};
typedef struct arch_pgflags arch_page_flags_t;

int arch_mmu_page_table_min_align_order(int stage);

int arch_mmu_page_table_align_order(int stage, int level);

int arch_mmu_page_table_size_order(int stage, int level);

void arch_mmu_stage2_tlbflush(bool remote, bool use_vmid, uint32_t vmid, physical_addr_t gpa, physical_size_t gsz);

void arch_mmu_stage1_tlbflush(bool remote, bool use_asid, uint32_t asid, virtual_addr_t va, virtual_size_t size);

bool arch_mmu_valid_block_size(physical_size_t size);

int arch_mmu_start_level(int stage);

physical_size_t arch_mmu_level_block_size(int stage, int level);

int arch_mmu_level_block_shift(int stage, int level);

physical_addr_t arch_mmu_level_map_mask(int stage, int level);

int arch_mmu_level_index(physical_addr_t ia, int stage, int level);

int arch_mmu_level_index_shift(int stage, int level);

void arch_mmu_pgflags_set(arch_page_flags_t *flags, int stage, uint32_t mflags);

void arch_mmu_pte_sync(arch_pte_t *pte, int stage, int level);

void arch_mmu_pte_clear(arch_pte_t *pte, int stage, int level);

bool arch_mmu_pte_is_valid(arch_pte_t *pte, int stage, int level);

physical_addr_t arch_mmu_pte_addr(arch_pte_t *pte, int stage, int level);

void arch_mmu_pte_flags(arch_pte_t *pte, int stage, int level, arch_page_flags_t *out_flags);

void arch_mmu_pte_set(arch_pte_t *pte, int stage, int level, physical_addr_t pa, arch_page_flags_t *flags);

bool arch_mmu_pte_is_table(arch_pte_t *pte, int stage, int level);

physical_addr_t arch_mmu_pte_table_addr(arch_pte_t *pte, int stage, int level);

void arch_mmu_pte_set_table(arch_pte_t *pte, int stage, int level, physical_addr_t table_pa);

int arch_mmu_test_nested_page_table(
    physical_addr_t s2_table_pa, bool s1_avail, physical_addr_t s1_table_pa, uint32_t flags, virtual_addr_t addr, physical_addr_t *out_addr,
    uint32_t *out_fault_flags);

physical_addr_t arch_mmu_stage2_current_page_table_addr(void);

uint32_t arch_mmu_stage2_current_vmid(void);

int arch_mmu_stage2_change_page_table(bool have_vmid, uint32_t vmid, physical_addr_t table_phys);

#endif

#endif
