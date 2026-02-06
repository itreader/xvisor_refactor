/**
 * Copyright (c) 2018 Himanshu Chauhan.
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
 * @file ept.h
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief Definitions and structres related to EPT
 */

#ifndef __EPT_H
#define __EPT_H

#include <cpu_vm.h>
#include <vmm_types.h>

#define EPT_PROT_READ    (0x1 << 0)
#define EPT_PROT_WRITE   (0x1 << 1)
#define EPT_PROT_EXEC_S  (0x1 << 2)
#define EPT_PROT_EXEC_U  (0x1 << 10)
#define EPT_PROT_MASK    (~(EPT_PROT_READ | EPT_PROT_WRITE | EPT_PROT_EXEC_S | EPT_PROT_EXEC_U))

#define EPT_PAGE_SIZE_1G (0x1ULL << 30)
#define EPT_PAGE_SIZE_2M (0x1ULL << 21)
#define EPT_PAGE_SIZE_4K (0x1ULL << 12)

extern struct cpuinfo_x86 cpu_info;

#define PHYS_ADDR_BIT_MASK    ((0x1ul << cpu_info.phys_bits) - 1)
#define EPT_PAGE_MASK_2M      (PHYS_ADDR_BIT_MASK >> 21)
#define EPT_PAGE_MASK_4K      (PHYS_ADDR_BIT_MASK >> 12)
#define EPT_PAGE_MASK_1G      (PHYS_ADDR_BIT_MASK >> 30)

#define EPT_PHYS_FILTER(_p)   (_p & PHYS_ADDR_BIT_MASK)
#define EPT_PHYS_2MB_PFN(_p)  (EPT_PHYS_FILTER(_p) >> 21)
#define EPT_PHYS_1GB_PFN(_p)  (EPT_PHYS_FILTER(_p) >> 30)
#define EPT_PHYS_4KB_PFN(_p)  (EPT_PHYS_FILTER(_p) >> 12)

#define EPT_PHYS_2MB_PAGE(_p) ((_p & EPT_PAGE_MASK_2M) << 21)
#define EPT_PHYS_1GB_PAGE(_p) ((_p & EPT_PAGE_MASK_1G) << 30)
#define EPT_PHYS_4KB_PAGE(_p) ((_p & EPT_PAGE_MASK_4K) << 12)

typedef union {
    uint64_t val;

    struct {
        uint64_t mt     : 3;  /* Memory type: 0 Uncacheable 6 Writeback */
        uint64_t pgwl   : 3;  /* Pagewalk length */
        uint64_t en_ad  : 1;  /* Enable accessed/dirty flags for EPT structures */
        uint64_t en_ssr : 1;  /* Setting this control to 1 enables enforcement of
                            access rights for supervisor shadow-stack pages */
        uint64_t res    : 4;  /* reserved */
        uint64_t pml4   : 52; /* pml4 physical base, only bits N-1:12 are valid
                               * where N is the physical address width of the
                               * logical processor */
    } bits;
} eptp_t;

typedef union {
    uint64_t val;

    struct {
        uint64_t r         : 1;  /* Read access; Region 512 GiB */
        uint64_t w         : 1;  /* Write access */
        uint64_t x         : 1;  /* Execute access */
        uint64_t res       : 5;  /* Reserved */
        uint64_t accessed  : 1;  /* Depends on Bit 6 in EPTP. Currently not set */
        uint64_t ign       : 1;  /* Ignored */
        uint64_t mbe       : 1;  /* Mode based execution */
        uint64_t ign1      : 1;
        uint64_t pdpt_base : 40; /* Physical address of 4-KByte aligned EPT
                                  * page-directory-pointer table referenced by this entry */
        uint64_t ign2      : 12; /* Ignored */
    } bits;
} ept_pml4e_t;

typedef union {
    uint64_t val;

    struct {
        uint64_t r         : 1;  /* Read access; Region 1 GiB */
        uint64_t w         : 1;  /* Write access */
        uint64_t x         : 1;  /* Execute */
        uint64_t mt        : 3;  /* EPT memory type */
        uint64_t ign_pat   : 1;  /* Ignore PAT memory type for this 1 GiB page */
        uint64_t is_page   : 1;  /* Ignore */
        uint64_t accessed  : 1;  /* Accessed (If bit 6 set in EPTP) */
        uint64_t dirty     : 1;  /* Dirty (If bit 6 set in EPTP) */
        uint64_t mbe       : 1;
        uint64_t ign1      : 1;  /* Ignored */
        uint64_t res       : 18; /* Must be zero */
        uint64_t phys      : 22; /* physicall address of PD */
        uint64_t ign2      : 8;
        uint64_t superv_ss : 1;  /* supervisor shadow stack */
        uint64_t ign3      : 2;  /* ignored */
        uint64_t sup_ve    : 1;  /* suppress #VE exception */
    } pe;

    struct {
        uint64_t r        : 1;  /* Read access */
        uint64_t w        : 1;  /* Write */
        uint64_t x        : 1;  /* Execute */
        uint64_t res      : 5;  /* Reservd */
        uint64_t accessed : 1;  /* Accessed by software (if Bit 6 in EPTP is set) */
        uint64_t ign      : 1;  /* Ignored */
        uint64_t mbe      : 1;  /* mode based exec */
        uint64_t ign1     : 1;
        uint64_t pd_base  : 40; /* Page directory base */
        uint64_t ign2     : 12; /* Ignored */
    } te;
} ept_pdpte_t;

typedef union {
    uint64_t val;

    struct {
        uint64_t r         : 1;  /* Read; Region 2 MiB */
        uint64_t w         : 1;  /* Write */
        uint64_t x         : 1;  /* Execute */
        uint64_t mt        : 3;  /* Memory Type */
        uint64_t ign_pat   : 1;  /* Ignore PAT type for this region */
        uint64_t is_page   : 1;  /* Must be set to 1 */
        uint64_t accessed  : 1;  /* Region was accessed by software */
        uint64_t dirty     : 1;  /* Region was written to by software */
        uint64_t mbe       : 1;
        uint64_t ign       : 1;  /* Ignored */
        uint64_t res       : 9;  /* Must be zero */
        uint64_t phys      : 31; /* Physical address of 2MiB page */
        uint64_t ign1      : 8;  /* Ignored */
        uint64_t superv_ss : 1;
        uint64_t ign2      : 2;
        uint64_t sup_ve    : 1;  /* Suppress #VE */
    } pe;

    struct {
        uint64_t r        : 1;  /* Read */
        uint64_t w        : 1;  /* Written */
        uint64_t x        : 1;  /* Execute */
        uint64_t res      : 4;  /* Reserved */
        uint64_t is_page  : 1;  /* Must be zero */
        uint64_t accessed : 1;  /* Accessed by software (if bit 6 is set in EPTP) */
        uint64_t ign      : 1;  /* Ignore */
        uint64_t mbe      : 1;
        uint64_t ign1     : 1;
        uint64_t pt_base  : 40; /* Physical address of the page table */
        uint64_t res1     : 12; /* Reserved */
    } te;
} ept_pde_t;

typedef union {
    uint64_t val;

    struct {
        uint64_t r         : 1;  /* Read; Region 4KiB */
        uint64_t w         : 1;  /* Write */
        uint64_t x         : 1;  /* Execute */
        uint64_t mt        : 3;  /* Memory Type */
        uint64_t ign_pat   : 1;  /* Ignore PAT memory type */
        uint64_t ign       : 1;  /* Ignored */
        uint64_t accessed  : 1;  /* Accessed by software (if bit 6 in eptp set) */
        uint64_t dirty     : 1;  /* Written by software (if bit 6 in eptp set) */
        uint64_t mbe       : 1;
        uint64_t ign1      : 1;  /* Ignored */
        uint64_t phys      : 40; /* Physical address of 4 KiB page mapped */
        uint64_t ign2      : 8;  /* Ignored */
        uint64_t superv_ss : 1;
        uint64_t subpage_w : 1;
        uint64_t ign3      : 1;
        uint64_t sup_ve    : 1; /* Suppress #VE */
    } pe;
} ept_pte_t;

int setup_ept(struct vcpu_hw_context *context);
int ept_create_pte_map(struct vcpu_hw_context *context, physical_addr_t gphys, physical_addr_t hphys, size_t pg_size, uint32_t pg_prot);

#endif /* __EPT_H */
