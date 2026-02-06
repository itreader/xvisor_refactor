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
 * @file cpu_vcpu_mem.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief This source file is for VCPU memory read/write emulation
 */

#include <cpu_inline_asm.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_mem.h>
#include <vmm_device_emulate.h>
#include <vmm_error.h>

int cpu_vcpu_mem_read(vmm_vcpu_t *vcpu, arch_regs_t *regs, virtual_addr_t addr, void *dst, uint32_t dst_len, bool force_unprivate)
{
    int                                rc;
    uint8_t                            data8;
    uint16_t                           data16;
    uint32_t                           data32;
    physical_addr_t                    guest_pa;
    enum vmm_device_emulate_endianness data_endian;

    /* Determine data endianness */
    if (regs->cpsr & CPSR_BE_ENABLED) {
        data_endian = VMM_DEVICE_EMULATE_BIG_ENDIAN;
    } else {
        data_endian = VMM_DEVICE_EMULATE_LITTLE_ENDIAN;
    }

    /* Determine guest physical address */
    va2pa_c_pr(addr);
    guest_pa = read_par64();
    guest_pa &= PAR64_PA_MASK;
    guest_pa |= (addr & 0x00000FFF);

    /* Do guest memory read */
    switch (dst_len) {
        case 1:
            rc                = vmm_device_emulate_emulate_read(vcpu, guest_pa, &data8, sizeof(data8), data_endian);
            *((uint8_t *)dst) = (!rc) ? data8 : 0;
            break;

        case 2:
            rc                 = vmm_device_emulate_emulate_read(vcpu, guest_pa, &data16, sizeof(data16), data_endian);
            *((uint16_t *)dst) = (!rc) ? data16 : 0;
            break;

        case 4:
            rc                 = vmm_device_emulate_emulate_read(vcpu, guest_pa, &data32, sizeof(data32), data_endian);
            *((uint32_t *)dst) = (!rc) ? data32 : 0;
            break;

        default:
            rc = VMM_EFAIL;
            break;
    };

    return rc;
}

int cpu_vcpu_mem_write(vmm_vcpu_t *vcpu, arch_regs_t *regs, virtual_addr_t addr, void *src, uint32_t src_len, bool force_unprivate)
{
    int                                rc;
    uint8_t                            data8;
    uint16_t                           data16;
    uint32_t                           data32;
    physical_addr_t                    guest_pa;
    enum vmm_device_emulate_endianness data_endian;

    /* Determine data endianness */
    if (regs->cpsr & CPSR_BE_ENABLED) {
        data_endian = VMM_DEVICE_EMULATE_BIG_ENDIAN;
    } else {
        data_endian = VMM_DEVICE_EMULATE_LITTLE_ENDIAN;
    }

    /* Determine guest physical address */
    va2pa_c_pr(addr);
    guest_pa = read_par64();
    guest_pa &= PAR64_PA_MASK;
    guest_pa |= (addr & 0x00000FFF);

    /* Do guest memory read */
    switch (src_len) {
        case 1:
            data8 = *((uint8_t *)src);
            rc    = vmm_device_emulate_emulate_write(vcpu, guest_pa, &data8, sizeof(data8), data_endian);
            break;

        case 2:
            data16 = *((uint16_t *)src);
            rc     = vmm_device_emulate_emulate_write(vcpu, guest_pa, &data16, sizeof(data16), data_endian);
            break;

        case 4:
            data32 = *((uint32_t *)src);
            rc     = vmm_device_emulate_emulate_write(vcpu, guest_pa, &data32, sizeof(data32), data_endian);
            break;

        default:
            rc = VMM_EFAIL;
            break;
    };

    return rc;
}

int cpu_vcpu_mem_readex(vmm_vcpu_t *vcpu, arch_regs_t *regs, virtual_addr_t addr, void *dst, uint32_t dst_len, bool force_unprivate)
{
    /* Not supported */
    return VMM_EFAIL;
}

int cpu_vcpu_mem_writeex(vmm_vcpu_t *vcpu, arch_regs_t *regs, virtual_addr_t addr, void *src, uint32_t src_len, bool force_unprivate)
{
    /* Not supported */
    return VMM_EFAIL;
}
