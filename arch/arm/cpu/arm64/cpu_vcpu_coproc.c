/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file cpu_vcpu_coproc.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for coprocessor access
 */

#include <cpu_defines.h>
#include <cpu_vcpu_coproc.h>
#include <cpu_vcpu_sysregs.h>

static bool cpu_vcpu_cpx_ldcstc_accept_nop(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t D, uint32_t CRd, uint32_t uopt, uint32_t imm8)
{
    return TRUE;
}

static bool cpu_vcpu_cpx_ldcstc_done_nop(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t index, uint32_t D, uint32_t CRd, uint32_t uopt, uint32_t imm8)
{
    return TRUE;
}

static uint32_t cpu_vcpu_cpx_ldcstc_read_zero(
    vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t index, uint32_t D, uint32_t CRd, uint32_t uopt, uint32_t imm8)
{
    return 0;
}

static void cpu_vcpu_cpx_ldcstc_ignore_write(
    vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t index, uint32_t D, uint32_t CRd, uint32_t uopt, uint32_t imm8, uint32_t data)
{
}

static bool cpu_vcpu_cpx_read2_zero(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t opc1, uint32_t CRm, uint32_t *data, uint32_t *data2)
{
    *data  = 0x0;
    *data2 = 0x0;

    return TRUE;
}

static bool cpu_vcpu_cpx_ignore_write2(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t opc1, uint32_t CRm, uint32_t data, uint32_t data2)
{
    return TRUE;
}

static bool cpu_vcpu_cpx_data_process_nop(vmm_vcpu_t *vcpu, arch_regs_t *regs, uint32_t opc1, uint32_t opc2, uint32_t CRd, uint32_t CRn, uint32_t CRm)
{
    return TRUE;
}

static struct cpu_vcpu_coproc cp_array[CPU_COPROC_COUNT] = {
    {
     .cpnum         = 0,
     .ldcstc_accept = NULL,
     .ldcstc_done   = NULL,
     .ldcstc_read   = NULL,
     .ldcstc_write  = NULL,
     .write2        = NULL,
     .read2         = NULL,
     .data_process  = NULL,
     .write         = NULL,
     .read          = NULL,
     },
    {
     .cpnum         = 1,
     .ldcstc_accept = NULL,
     .ldcstc_done   = NULL,
     .ldcstc_read   = NULL,
     .ldcstc_write  = NULL,
     .write2        = NULL,
     .read2         = NULL,
     .data_process  = NULL,
     .write         = NULL,
     .read          = NULL,
     },
    {
     .cpnum         = 2,
     .ldcstc_accept = NULL,
     .ldcstc_done   = NULL,
     .ldcstc_read   = NULL,
     .ldcstc_write  = NULL,
     .write2        = NULL,
     .read2         = NULL,
     .data_process  = NULL,
     .write         = NULL,
     .read          = NULL,
     },
    {
     .cpnum         = 3,
     .ldcstc_accept = NULL,
     .ldcstc_done   = NULL,
     .ldcstc_read   = NULL,
     .ldcstc_write  = NULL,
     .write2        = NULL,
     .read2         = NULL,
     .data_process  = NULL,
     .write         = NULL,
     .read          = NULL,
     },
    {
     .cpnum         = 4,
     .ldcstc_accept = NULL,
     .ldcstc_done   = NULL,
     .ldcstc_read   = NULL,
     .ldcstc_write  = NULL,
     .write2        = NULL,
     .read2         = NULL,
     .data_process  = NULL,
     .write         = NULL,
     .read          = NULL,
     },
    {
     .cpnum         = 5,
     .ldcstc_accept = NULL,
     .ldcstc_done   = NULL,
     .ldcstc_read   = NULL,
     .ldcstc_write  = NULL,
     .write2        = NULL,
     .read2         = NULL,
     .data_process  = NULL,
     .write         = NULL,
     .read          = NULL,
     },
    {
     .cpnum         = 6,
     .ldcstc_accept = NULL,
     .ldcstc_done   = NULL,
     .ldcstc_read   = NULL,
     .ldcstc_write  = NULL,
     .write2        = NULL,
     .read2         = NULL,
     .data_process  = NULL,
     .write         = NULL,
     .read          = NULL,
     },
    {
     .cpnum         = 7,
     .ldcstc_accept = NULL,
     .ldcstc_done   = NULL,
     .ldcstc_read   = NULL,
     .ldcstc_write  = NULL,
     .write2        = NULL,
     .read2         = NULL,
     .data_process  = NULL,
     .write         = NULL,
     .read          = NULL,
     },
    {
     .cpnum         = 8,
     .ldcstc_accept = NULL,
     .ldcstc_done   = NULL,
     .ldcstc_read   = NULL,
     .ldcstc_write  = NULL,
     .write2        = NULL,
     .read2         = NULL,
     .data_process  = NULL,
     .write         = NULL,
     .read          = NULL,
     },
    {
     .cpnum         = 9,
     .ldcstc_accept = NULL,
     .ldcstc_done   = NULL,
     .ldcstc_read   = NULL,
     .ldcstc_write  = NULL,
     .write2        = NULL,
     .read2         = NULL,
     .data_process  = NULL,
     .write         = NULL,
     .read          = NULL,
     },
    {.cpnum         = 10,
     .ldcstc_accept = NULL,
     .ldcstc_done   = NULL,
     .ldcstc_read   = NULL,
     .ldcstc_write  = NULL,
     .write2        = NULL,
     .read2         = NULL,
     .data_process  = NULL,
     .write         = NULL,
     .read          = NULL},
    {
     .cpnum         = 11,
     .ldcstc_accept = NULL,
     .ldcstc_done   = NULL,
     .ldcstc_read   = NULL,
     .ldcstc_write  = NULL,
     .write2        = NULL,
     .read2         = NULL,
     .data_process  = NULL,
     .write         = NULL,
     .read          = NULL,
     },
    {
     .cpnum         = 12,
     .ldcstc_accept = NULL,
     .ldcstc_done   = NULL,
     .ldcstc_read   = NULL,
     .ldcstc_write  = NULL,
     .write2        = NULL,
     .read2         = NULL,
     .data_process  = NULL,
     .write         = NULL,
     .read          = NULL,
     },
    {
     .cpnum         = 13,
     .ldcstc_accept = NULL,
     .ldcstc_done   = NULL,
     .ldcstc_read   = NULL,
     .ldcstc_write  = NULL,
     .write2        = NULL,
     .read2         = NULL,
     .data_process  = NULL,
     .write         = NULL,
     .read          = NULL,
     },
    {
     .cpnum         = 14,
     .ldcstc_accept = cpu_vcpu_cpx_ldcstc_accept_nop,
     .ldcstc_done   = cpu_vcpu_cpx_ldcstc_done_nop,
     .ldcstc_read   = cpu_vcpu_cpx_ldcstc_read_zero,
     .ldcstc_write  = cpu_vcpu_cpx_ldcstc_ignore_write,
     .write2        = cpu_vcpu_cpx_ignore_write2,
     .read2         = cpu_vcpu_cpx_read2_zero,
     .data_process  = cpu_vcpu_cpx_data_process_nop,
     .write         = &cpu_vcpu_cp14_write,
     .read          = &cpu_vcpu_cp14_read,
     },
    {
     .cpnum         = 15,
     .ldcstc_accept = cpu_vcpu_cpx_ldcstc_accept_nop,
     .ldcstc_done   = cpu_vcpu_cpx_ldcstc_done_nop,
     .ldcstc_read   = cpu_vcpu_cpx_ldcstc_read_zero,
     .ldcstc_write  = cpu_vcpu_cpx_ldcstc_ignore_write,
     .write2        = cpu_vcpu_cpx_ignore_write2,
     .read2         = cpu_vcpu_cpx_read2_zero,
     .data_process  = cpu_vcpu_cpx_data_process_nop,
     .write         = &cpu_vcpu_cp15_write,
     .read          = &cpu_vcpu_cp15_read,
     },
};

struct cpu_vcpu_coproc *cpu_vcpu_coproc_get(uint32_t cpnum)
{
    if (cpnum < CPU_COPROC_COUNT) {
        return &cp_array[cpnum];
    }

    return NULL;
}
