/**
 * Copyright (c) 2018 Anup Patel.
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
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of Arch MMU for RISC-V CPUs
 */

#include <arch_barrier.h>
#include <arch_cpu_irq.h>
#include <generic_mmu.h>
#include <vmm_error.h>
#include <vmm_guest_address_space.h>
#include <vmm_host_address_space.h>
#include <vmm_smp.h>
#include <vmm_types.h>

#include <cpu_hwcap.h>
#include <cpu_sbi.h>
#include <cpu_tlb.h>
#include <cpu_vcpu_trap.h>
#include <cpu_vcpu_unprivate.h>

#ifdef CONFIG_64BIT
/* Assume Sv39 */
uint64_t riscv_stage1_mode = SATP_MODE_SV39;
#else
/* Assume Sv32 */
uint64_t riscv_stage1_mode = SATP_MODE_SV32;
#endif

int arch_mmu_page_table_min_align_order(int stage)
{
    return PAGE_TABLE_PAGE_SIZE_SHIFT;
}

int arch_mmu_page_table_align_order(int stage, int level)
{
    if (stage != MMU_STAGE1 && level == arch_mmu_start_level(stage)) {
        return PAGE_TABLE_PAGE_SIZE_SHIFT + 2;
    }

    return PAGE_TABLE_PAGE_SIZE_SHIFT;
}

int arch_mmu_page_table_size_order(int stage, int level)
{
    if (stage != MMU_STAGE1 && level == arch_mmu_start_level(stage)) {
        return PAGE_TABLE_PAGE_SIZE_SHIFT + 2;
    }

    return PAGE_TABLE_PAGE_SIZE_SHIFT;
}

void arch_mmu_stage2_tlbflush(bool remote, bool use_vmid, uint32_t vmid, physical_addr_t gpa, physical_size_t gsz)
{
    physical_addr_t off;

    if (remote) {
        if (use_vmid) {
            sbi_remote_hfence_gvma_vmid(NULL, gpa, gsz, vmid);
        } else {
            sbi_remote_hfence_gvma(NULL, gpa, gsz);
        }
    } else {
        if (use_vmid) {
            for (off = 0; off < gsz; off += VMM_PAGE_SIZE) {
                __hfence_gvma_vmid_gpa((gpa + off) >> 2, vmid);
            }
        } else {
            for (off = 0; off < gsz; off += VMM_PAGE_SIZE) {
                __hfence_gvma_gpa((gpa + off) >> 2);
            }
        }
    }
}

void arch_mmu_stage1_tlbflush(bool remote, bool use_asid, uint32_t asid, virtual_addr_t va, virtual_size_t size)
{
    virtual_addr_t off;

    if (remote) {
        if (use_asid) {
            sbi_remote_sfence_vma_asid(NULL, va, size, asid);
        } else {
            sbi_remote_sfence_vma(NULL, va, size);
        }
    } else {
        if (use_asid) {
            for (off = 0; off < size; off += VMM_PAGE_SIZE) {
                __sfence_vma_asid_va(asid, va + off);
            }
        } else {
            for (off = 0; off < size; off += VMM_PAGE_SIZE) {
                __sfence_vma_va(va + off);
            }
        }
    }
}

bool arch_mmu_valid_block_size(physical_size_t size)
{
    if (
#ifdef CONFIG_64BIT
        (size == PAGE_TABLE_L4_BLOCK_SIZE) || (size == PAGE_TABLE_L3_BLOCK_SIZE) || (size == PAGE_TABLE_L2_BLOCK_SIZE) ||
#endif
        (size == PAGE_TABLE_L1_BLOCK_SIZE) || (size == PAGE_TABLE_L0_BLOCK_SIZE)) {
        return TRUE;
    }

    return FALSE;
}

int arch_mmu_start_level(int stage)
{
    if (stage == MMU_STAGE1) {
        switch (riscv_stage1_mode) {
            case SATP_MODE_SV32:
                return 1;
#ifdef CONFIG_64BIT

            case SATP_MODE_SV39:
                return 2;

            case SATP_MODE_SV48:
                return 3;

            case SATP_MODE_SV57:
                return 4;
#endif

            default:
                return 0;
        };
    } else {
        switch (riscv_stage2_mode) {
            case HGATP_MODE_SV32X4:
                return 1;
#ifdef CONFIG_64BIT

            case HGATP_MODE_SV39X4:
                return 2;

            case HGATP_MODE_SV48X4:
                return 3;

            case HGATP_MODE_SV57X4:
                return 4;
#endif

            default:
                return 0;
        };
    }
}

physical_size_t arch_mmu_level_block_size(int stage, int level)
{
    switch (level) {
        case 0:
            return PAGE_TABLE_L0_BLOCK_SIZE;

        case 1:
            return PAGE_TABLE_L1_BLOCK_SIZE;
#ifdef CONFIG_64BIT

        case 2:
            return PAGE_TABLE_L2_BLOCK_SIZE;

        case 3:
            return PAGE_TABLE_L3_BLOCK_SIZE;

        case 4:
            return PAGE_TABLE_L4_BLOCK_SIZE;
#endif

        default:
            break;
    };

    return PAGE_TABLE_L0_BLOCK_SIZE;
}

int arch_mmu_level_block_shift(int stage, int level)
{
    switch (level) {
        case 0:
            return PAGE_TABLE_L0_BLOCK_SHIFT;

        case 1:
            return PAGE_TABLE_L1_BLOCK_SHIFT;
#ifdef CONFIG_64BIT

        case 2:
            return PAGE_TABLE_L2_BLOCK_SHIFT;

        case 3:
            return PAGE_TABLE_L3_BLOCK_SHIFT;

        case 4:
            return PAGE_TABLE_L4_BLOCK_SHIFT;
#endif

        default:
            break;
    };

    return PAGE_TABLE_L0_BLOCK_SHIFT;
}

physical_addr_t arch_mmu_level_map_mask(int stage, int level)
{
    switch (level) {
        case 0:
            return PAGE_TABLE_L0_MAP_MASK;

        case 1:
            return PAGE_TABLE_L1_MAP_MASK;
#ifdef CONFIG_64BIT

        case 2:
            return PAGE_TABLE_L2_MAP_MASK;

        case 3:
            return PAGE_TABLE_L3_MAP_MASK;

        case 4:
            return PAGE_TABLE_L4_MAP_MASK;
#endif

        default:
            break;
    };

    return PAGE_TABLE_L0_MAP_MASK;
}

int arch_mmu_level_index(physical_addr_t ia, int stage, int level)
{
    int             shift = PAGE_TABLE_L0_INDEX_SHIFT;
    physical_addr_t mask  = PAGE_TABLE_L0_INDEX_MASK;

    switch (level) {
        case 0:
            mask  = PAGE_TABLE_L0_INDEX_MASK;
            shift = PAGE_TABLE_L0_INDEX_SHIFT;
            break;

        case 1:
            mask  = PAGE_TABLE_L1_INDEX_MASK;
            shift = PAGE_TABLE_L1_INDEX_SHIFT;
            break;
#ifdef CONFIG_64BIT

        case 2:
            mask  = PAGE_TABLE_L2_INDEX_MASK;
            shift = PAGE_TABLE_L2_INDEX_SHIFT;
            break;

        case 3:
            mask  = PAGE_TABLE_L3_INDEX_MASK;
            shift = PAGE_TABLE_L3_INDEX_SHIFT;
            break;

        case 4:
            mask  = PAGE_TABLE_L4_INDEX_MASK;
            shift = PAGE_TABLE_L4_INDEX_SHIFT;
            break;
#endif

        default:
            break;
    };

    if (stage != MMU_STAGE1 && level == arch_mmu_start_level(stage)) {
        mask = (mask << 2) | (0x3ULL << shift);
    }

    return (ia & mask) >> shift;
}

int arch_mmu_level_index_shift(int stage, int level)
{
    switch (level) {
        case 0:
            return PAGE_TABLE_L0_INDEX_SHIFT;

        case 1:
            return PAGE_TABLE_L1_INDEX_SHIFT;
#ifdef CONFIG_64BIT

        case 2:
            return PAGE_TABLE_L2_INDEX_SHIFT;

        case 3:
            return PAGE_TABLE_L3_INDEX_SHIFT;

        case 4:
            return PAGE_TABLE_L4_INDEX_SHIFT;
#endif

        default:
            break;
    };

    return PAGE_TABLE_L0_INDEX_SHIFT;
}

void arch_mmu_pgflags_set(arch_page_flags_t *flags, int stage, uint32_t mflags)
{
    if (stage == MMU_STAGE2) {
        flags->rsw      = 0;
        flags->accessed = 1;
        flags->dirty    = 1;
        flags->global   = 0;
        flags->user     = 1;

        if (mflags & VMM_REGION_VIRTUAL) {
            flags->read    = 0;
            flags->write   = 0;
            flags->execute = 1;
        } else if (mflags & VMM_REGION_READONLY) {
            flags->read    = 1;
            flags->write   = 0;
            flags->execute = 1;
        } else {
            flags->read    = 1;
            flags->write   = 1;
            flags->execute = 1;
        }

        flags->valid = 1;
    } else {
        flags->rsw      = 0;
        flags->accessed = 1;
        flags->dirty    = 1;
        flags->global   = 1;
        flags->user     = 0;
        flags->execute  = (mflags & VMM_MEMORY_EXECUTABLE) ? 1 : 0;
        flags->write    = (mflags & VMM_MEMORY_WRITEABLE) ? 1 : 0;
        flags->read     = (mflags & VMM_MEMORY_READABLE) ? 1 : 0;
        flags->valid    = 1;

        /*
         * We ignore following flags:
         * VMM_MEMORY_CACHEABLE
         * VMM_MEMORY_BUFFERABLE
         * VMM_MEMORY_IO_DEVICE
         * VMM_MEMORY_DMA_COHERENT
         * VMM_MEMORY_DMA_NONCOHERENT
         */
    }
}

void arch_mmu_pte_sync(arch_pte_t *pte, int stage, int level)
{
    arch_smp_mb();
}

void arch_mmu_pte_clear(arch_pte_t *pte, int stage, int level)
{
    *pte = 0x0;
}

bool arch_mmu_pte_is_valid(arch_pte_t *pte, int stage, int level)
{
    return (*pte & PAGE_TABLE_PTE_VALID_MASK) ? TRUE : FALSE;
}

physical_addr_t arch_mmu_pte_addr(arch_pte_t *pte, int stage, int level)
{
    return ((*pte & PAGE_TABLE_PTE_ADDR_MASK) >> PAGE_TABLE_PTE_ADDR_SHIFT) << PAGE_TABLE_PAGE_SIZE_SHIFT;
}

void arch_mmu_pte_flags(arch_pte_t *pte, int stage, int level, arch_page_flags_t *out_flags)
{
    out_flags->rsw      = (*pte & PAGE_TABLE_PTE_RSW_MASK) >> PAGE_TABLE_PTE_RSW_SHIFT;
    out_flags->dirty    = (*pte & PAGE_TABLE_PTE_DIRTY_MASK) >> PAGE_TABLE_PTE_DIRTY_SHIFT;
    out_flags->accessed = (*pte & PAGE_TABLE_PTE_ACCESSED_MASK) >> PAGE_TABLE_PTE_ACCESSED_SHIFT;
    out_flags->global   = (*pte & PAGE_TABLE_PTE_GLOBAL_MASK) >> PAGE_TABLE_PTE_GLOBAL_SHIFT;
    out_flags->user     = (*pte & PAGE_TABLE_PTE_USER_MASK) >> PAGE_TABLE_PTE_USER_SHIFT;
    out_flags->execute  = (*pte & PAGE_TABLE_PTE_EXECUTE_MASK) >> PAGE_TABLE_PTE_EXECUTE_SHIFT;
    out_flags->write    = (*pte & PAGE_TABLE_PTE_WRITE_MASK) >> PAGE_TABLE_PTE_WRITE_SHIFT;
    out_flags->read     = (*pte & PAGE_TABLE_PTE_READ_MASK) >> PAGE_TABLE_PTE_READ_SHIFT;
    out_flags->valid    = (*pte & PAGE_TABLE_PTE_VALID_MASK) >> PAGE_TABLE_PTE_VALID_SHIFT;
}

void arch_mmu_pte_set(arch_pte_t *pte, int stage, int level, physical_addr_t pa, arch_page_flags_t *flags)
{
    *pte = pa & arch_mmu_level_map_mask(stage, level);
    *pte = *pte >> PAGE_TABLE_PAGE_SIZE_SHIFT;
    *pte = *pte << PAGE_TABLE_PTE_ADDR_SHIFT;
    *pte |= ((arch_pte_t)flags->rsw << PAGE_TABLE_PTE_RSW_SHIFT) & PAGE_TABLE_PTE_RSW_MASK;
    *pte |= ((arch_pte_t)flags->dirty << PAGE_TABLE_PTE_DIRTY_SHIFT) & PAGE_TABLE_PTE_DIRTY_MASK;
    *pte |= ((arch_pte_t)flags->accessed << PAGE_TABLE_PTE_ACCESSED_SHIFT) & PAGE_TABLE_PTE_ACCESSED_MASK;
    *pte |= ((arch_pte_t)flags->global << PAGE_TABLE_PTE_GLOBAL_SHIFT) & PAGE_TABLE_PTE_GLOBAL_MASK;
    *pte |= ((arch_pte_t)flags->user << PAGE_TABLE_PTE_USER_SHIFT) & PAGE_TABLE_PTE_USER_MASK;
    *pte |= ((arch_pte_t)flags->execute << PAGE_TABLE_PTE_EXECUTE_SHIFT) & PAGE_TABLE_PTE_EXECUTE_MASK;
    *pte |= ((arch_pte_t)flags->write << PAGE_TABLE_PTE_WRITE_SHIFT) & PAGE_TABLE_PTE_WRITE_MASK;
    *pte |= ((arch_pte_t)flags->read << PAGE_TABLE_PTE_READ_SHIFT) & PAGE_TABLE_PTE_READ_MASK;
    *pte |= PAGE_TABLE_PTE_VALID_MASK;
}

bool arch_mmu_pte_is_table(arch_pte_t *pte, int stage, int level)
{
    return (*pte & PAGE_TABLE_PTE_PERM_MASK) ? FALSE : TRUE;
}

physical_addr_t arch_mmu_pte_table_addr(arch_pte_t *pte, int stage, int level)
{
    return ((*pte & PAGE_TABLE_PTE_ADDR_MASK) >> PAGE_TABLE_PTE_ADDR_SHIFT) << PAGE_TABLE_PAGE_SIZE_SHIFT;
}

void arch_mmu_pte_set_table(arch_pte_t *pte, int stage, int level, physical_addr_t table_pa)
{
    *pte = table_pa >> PAGE_TABLE_PAGE_SIZE_SHIFT;
    *pte = *pte << PAGE_TABLE_PTE_ADDR_SHIFT;
    *pte |= PAGE_TABLE_PTE_VALID_MASK;
}

int arch_mmu_test_nested_page_table(
    physical_addr_t s2_table_pa, bool s1_avail, physical_addr_t s1_table_pa, uint32_t flags, virtual_addr_t addr, physical_addr_t *out_addr,
    uint32_t *out_fault_flags)
{
    int                    rc = VMM_OK;
    irq_flags_t            f;
    struct mmu_page        pg;
    struct mmu_page_table *s1_page_table, *s2_page_table;
    struct cpu_vcpu_trap   trap      = {0};
    struct cpu_vcpu_trap  *trap_info = &trap;
    physical_addr_t        trap_gpa;
    uint64_t               tmp = -1UL, trap_gva;
    uint64_t               hstatus, stvec, vsatp, hgatp;

    hgatp = riscv_stage2_mode << HGATP_MODE_SHIFT;
    hgatp |= (s2_table_pa >> PAGE_TABLE_PAGE_SIZE_SHIFT) & HGATP_PPN;

    if (s1_avail) {
        vsatp = riscv_stage1_mode << SATP_MODE_SHIFT;
        vsatp |= (s1_table_pa >> PAGE_TABLE_PAGE_SIZE_SHIFT) & SATP_PPN;
    } else {
        vsatp = 0;
    }

    stvec = (uint64_t)&__cpu_vcpu_unpriv_trap_handler;

    arch_cpu_irq_save(f);

    hstatus = csr_read(CSR_HSTATUS);
    csr_set(CSR_HSTATUS, HSTATUS_SPVP);
    csr_clear(CSR_HSTATUS, HSTATUS_GVA);

    stvec = csr_swap(CSR_STVEC, stvec);
    vsatp = csr_swap(CSR_VSATP, vsatp);
    hgatp = csr_swap(CSR_HGATP, hgatp);

    if (flags & MMU_TEST_WRITE) {
        /*
         * t0 is register 5
         * t1 is register 6
         * t2 is register 7
         */
        if (flags & MMU_TEST_WIDTH_8BIT) {
            /*
             * HSV.B rs2, (rs1) instruction
             * 0110001 rs2 rs1 100 00000 1110011
             */
            asm volatile("\n"
                         ".option push\n"
                         ".option norvc\n"
                         "add t0, %[tmp], zero\n"
                         "add t1, %[trap_info], zero\n"
                         "add t2, %[addr], zero\n"
                         /*
                          * HSV.B t0, (t2)
                          * 0110001 00101 00111 100 00000 1110011
                          */
                         ".word 0x6253c073\n"
                         ".option pop"
                         : [trap_info] "+&r"(trap_info)
                         : [tmp] "r"(tmp), [addr] "r"(addr)
                         : "t0", "t1", "t2", "memory");
        } else if (flags & MMU_TEST_WIDTH_16BIT) {
            /*
             * HSV.H rs2, (rs1) instruction
             * 0110011 rs2 rs1 100 00000 1110011
             */
            asm volatile("\n"
                         ".option push\n"
                         ".option norvc\n"
                         "add t0, %[tmp], zero\n"
                         "add t1, %[trap_info], zero\n"
                         "add t2, %[addr], zero\n"
                         /*
                          * HSV.H t0, (t2)
                          * 0110011 00101 00111 100 00000 1110011
                          */
                         ".word 0x6653c073\n"
                         ".option pop"
                         : [trap_info] "+&r"(trap_info)
                         : [tmp] "r"(tmp), [addr] "r"(addr)
                         : "t0", "t1", "t2", "memory");
        } else if (flags & MMU_TEST_WIDTH_32BIT) {
            /*
             * HSV.W rs2, (rs1) instruction
             * 0110101 rs2 rs1 100 00000 1110011
             */
            asm volatile("\n"
                         ".option push\n"
                         ".option norvc\n"
                         "add t0, %[tmp], zero\n"
                         "add t1, %[trap_info], zero\n"
                         "add t2, %[addr], zero\n"
                         /*
                          * HSV.W t0, (t2)
                          * 0110101 00101 00111 100 00000 1110011
                          */
                         ".word 0x6a53c073\n"
                         ".option pop"
                         : [trap_info] "+&r"(trap_info)
                         : [tmp] "r"(tmp), [addr] "r"(addr)
                         : "t0", "t1", "t2", "memory");
        } else {
            rc = VMM_EINVALID;
        }
    } else {
        if (flags & MMU_TEST_WIDTH_8BIT) {
            /*
             * HLV.BU rd, (rs1) instruction
             * 0110000 00001 rs1 100 rd 1110011
             */
            asm volatile("\n"
                         ".option push\n"
                         ".option norvc\n"
                         "add t1, %[trap_info], zero\n"
                         "add t2, %[addr], zero\n"
                         /*
                          * HLV.BU t0, (t2)
                          * 0110000 00001 00111 100 00101 1110011
                          */
                         ".word 0x6013c2f3\n"
                         "add %[tmp], t0, zero\n"
                         ".option pop"
                         : [trap_info] "+&r"(trap_info), [tmp] "=&r"(tmp)
                         : [addr] "r"(addr)
                         : "t0", "t1", "t2", "memory");
        } else if (flags & MMU_TEST_WIDTH_16BIT) {
            /*
             * HLV.HU rd, (rs1) instruction
             * 0110010 00001 rs1 100 rd 1110011
             */
            asm volatile("\n"
                         ".option push\n"
                         ".option norvc\n"
                         "add t1, %[trap_info], zero\n"
                         "add t2, %[addr], zero\n"
                         /*
                          * HLV.HU t0, (t2)
                          * 0110010 00001 00111 100 00101 1110011
                          */
                         ".word 0x6413c2f3\n"
                         "add %[tmp], t0, zero\n"
                         ".option pop"
                         : [trap_info] "+&r"(trap_info), [tmp] "=&r"(tmp)
                         : [addr] "r"(addr)
                         : "t0", "t1", "t2", "memory");
        } else if (flags & MMU_TEST_WIDTH_32BIT) {
            /*
             * HLV.WU rd, (rs1) instruction
             * 0110100 00001 rs1 100 rd 1110011
             *
             * HLV.W rd, (rs1) instruction
             * 0110100 00000 rs1 100 rd 1110011
             */
            asm volatile("\n"
                         ".option push\n"
                         ".option norvc\n"
                         "add t1, %[trap_info], zero\n"
                         "add t2, %[addr], zero\n"
#ifdef CONFIG_64BIT
                         /*
                          * HLV.WU t0, (t2)
                          * 0110100 00001 00111 100 00101 1110011
                          */
                         ".word 0x6813c2f3\n"
#else
                         /*
                          * HLV.W t0, (t2)
                          * 0110100 00000 00111 100 00101 1110011
                          */
                         ".word 0x6803c2f3\n"
#endif
                         "add %[tmp], t0, zero\n"
                         ".option pop"
                         : [trap_info] "+&r"(trap_info), [tmp] "=&r"(tmp)
                         : [addr] "r"(addr)
                         : "t0", "t1", "t2", "memory");
        } else {
            rc = VMM_EINVALID;
        }
    }

    csr_write(CSR_HGATP, hgatp);
    csr_write(CSR_VSATP, vsatp);
    csr_write(CSR_STVEC, stvec);
    hstatus = csr_swap(CSR_HSTATUS, hstatus);

    arch_cpu_irq_restore(f);

    /*
     * We just polluted TLB by running HSV/HLV instructions so let's
     * cleanup by invalidating all Guest and Host TLB entries.
     */
    __hfence_gvma_all();
    __hfence_vvma_all();

    if (rc) {
        return rc;
    }

    *out_fault_flags = 0;
    *out_addr        = 0;

    if (trap.scause) {
        switch (trap.scause) {
            case CAUSE_LOAD_PAGE_FAULT:
                *out_fault_flags |= MMU_TEST_FAULT_S1;
                *out_fault_flags |= MMU_TEST_FAULT_READ;
                break;

            case CAUSE_STORE_PAGE_FAULT:
                *out_fault_flags |= MMU_TEST_FAULT_S1;
                *out_fault_flags |= MMU_TEST_FAULT_WRITE;
                break;

            case CAUSE_LOAD_GUEST_PAGE_FAULT:
                *out_fault_flags |= MMU_TEST_FAULT_READ;
                break;

            case CAUSE_STORE_GUEST_PAGE_FAULT:
                *out_fault_flags |= MMU_TEST_FAULT_WRITE;
                break;

            default:
                *out_fault_flags |= MMU_TEST_FAULT_UNKNOWN;
                break;
        };

        if (!(*out_fault_flags & MMU_TEST_FAULT_UNKNOWN)) {
            if (!(hstatus & HSTATUS_GVA)) {
                return VMM_EFAIL;
            }
        }

        trap_gva = trap.stval;
        trap_gpa = ((physical_addr_t)trap.htval << 2);
        trap_gpa |= ((physical_addr_t)trap.stval & 0x3);

        if (*out_fault_flags & MMU_TEST_FAULT_S1) {
            if (!s1_avail) {
                return VMM_EFAIL;
            }

            s1_page_table = mmu_page_table_find(MMU_STAGE1, s1_table_pa);

            if (!s1_page_table) {
                return VMM_EFAIL;
            }

            if (mmu_get_page(s1_page_table, trap_gva, &pg)) {
                *out_fault_flags |= MMU_TEST_FAULT_NOMAP;
            }

            *out_addr = trap_gva;
        } else {
            s2_page_table = mmu_page_table_find(MMU_STAGE2, s2_table_pa);

            if (!s2_page_table) {
                return VMM_EFAIL;
            }

            if (mmu_get_page(s2_page_table, trap_gpa, &pg)) {
                *out_fault_flags |= MMU_TEST_FAULT_NOMAP;
            }

            *out_addr = trap_gpa;
        }
    } else {
        if (s1_avail) {
            s1_page_table = mmu_page_table_find(MMU_STAGE1, s1_table_pa);

            if (!s1_page_table) {
                return VMM_EFAIL;
            }

            rc = mmu_get_page(s1_page_table, addr, &pg);

            if (rc) {
                return rc;
            }

            *out_addr = pg.oa | (addr & (pg.size - 1));
        } else {
            *out_addr = addr;
        }

        s2_page_table = mmu_page_table_find(MMU_STAGE2, s2_table_pa);

        if (!s2_page_table) {
            return VMM_EFAIL;
        }

        rc = mmu_get_page(s2_page_table, *out_addr, &pg);

        if (rc) {
            return rc;
        }

        *out_addr = pg.oa | (*out_addr & (pg.size - 1));
    }

    return rc;
}

physical_addr_t arch_mmu_stage2_current_page_table_addr(void)
{
    uint64_t page_table_ppn = csr_read(CSR_HGATP) & HGATP_PPN;
    return page_table_ppn << PAGE_TABLE_PAGE_SIZE_SHIFT;
}

uint32_t arch_mmu_stage2_current_vmid(void)
{
    return (csr_read(CSR_HGATP) & HGATP_VMID) >> HGATP_VMID_SHIFT;
}

int arch_mmu_stage2_change_page_table(bool have_vmid, uint32_t vmid, physical_addr_t table_phys)
{
    uint64_t hgatp;

    hgatp = riscv_stage2_mode << HGATP_MODE_SHIFT;
    hgatp |= ((uint64_t)vmid << HGATP_VMID_SHIFT) & HGATP_VMID;
    hgatp |= (table_phys >> PAGE_TABLE_PAGE_SIZE_SHIFT) & HGATP_PPN;

    csr_write(CSR_HGATP, hgatp);

    return VMM_OK;
}
