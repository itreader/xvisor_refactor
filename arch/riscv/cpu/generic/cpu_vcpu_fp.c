/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file cpu_vcpu_fp.c
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief source of VCPU FP functions
 */

#include <libs/stringlib.h>
#include <vmm_stdio.h>

#include <cpu_hwcap.h>
#include <cpu_vcpu_fp.h>
#include <cpu_vcpu_switch.h>

void cpu_vcpu_fp_init(vmm_vcpu_t *vcpu)
{
    riscv_regs(vcpu)->sstatus &= ~SSTATUS_FS;

    if (riscv_isa_extension_available(riscv_private(vcpu)->isa, f) || riscv_isa_extension_available(riscv_private(vcpu)->isa, d)) {
        riscv_regs(vcpu)->sstatus |= SSTATUS_FS_INITIAL;
    } else {
        riscv_regs(vcpu)->sstatus |= SSTATUS_FS_OFF;
    }

    memset(&riscv_private(vcpu)->fp, 0, sizeof(riscv_private(vcpu)->fp));
}

static inline void cpu_vcpu_fp_clean(arch_regs_t *regs)
{
    regs->sstatus &= ~SSTATUS_FS;
    regs->sstatus |= SSTATUS_FS_CLEAN;
}

static inline void cpu_vcpu_fp_force_save(vmm_vcpu_t *vcpu)
{
    uint64_t *isa = riscv_private(vcpu)->isa;

    if (riscv_isa_extension_available(isa, d)) {
        __cpu_vcpu_fp_d_save(&riscv_private(vcpu)->fp.d);
    } else if (riscv_isa_extension_available(isa, f)) {
        __cpu_vcpu_fp_f_save(&riscv_private(vcpu)->fp.f);
    }
}

static inline void cpu_vcpu_fp_force_restore(vmm_vcpu_t *vcpu)
{
    uint64_t *isa = riscv_private(vcpu)->isa;

    if (riscv_isa_extension_available(isa, d)) {
        __cpu_vcpu_fp_d_restore(&riscv_private(vcpu)->fp.d);
    } else if (riscv_isa_extension_available(isa, f)) {
        __cpu_vcpu_fp_f_restore(&riscv_private(vcpu)->fp.f);
    }
}

void cpu_vcpu_fp_save(vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    if (riscv_nested_virt(vcpu)) {
        /* Always save FP state when nested virtualization is ON */
        cpu_vcpu_fp_force_save(vcpu);
    } else {
        /* Lazy save FP state when nested virtualization is OFF */
        if ((regs->sstatus & SSTATUS_FS) == SSTATUS_FS_DIRTY) {
            cpu_vcpu_fp_force_save(vcpu);
            cpu_vcpu_fp_clean(regs);
        }
    }
}

void cpu_vcpu_fp_restore(vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    if (riscv_nested_virt(vcpu)) {
        /* Always restore FP state when nested virtualization is ON */
        cpu_vcpu_fp_force_restore(vcpu);
    } else {
        /* Lazy restore FP state when nested virtualization is OFF */
        if ((regs->sstatus & SSTATUS_FS) != SSTATUS_FS_OFF) {
            cpu_vcpu_fp_force_restore(vcpu);
            cpu_vcpu_fp_clean(regs);
        }
    }
}

void cpu_vcpu_fp_dump_regs(vmm_char_device_t *cdev, vmm_vcpu_t *vcpu)
{
    int i;
    struct riscv_priv *private = riscv_private(vcpu);

    if (!riscv_isa_extension_available(private->isa, f) && !riscv_isa_extension_available(private->isa, d)) {
        return;
    }

    vmm_cdev_printf(cdev, "\n");
    vmm_cdev_printf(cdev, "           fcsr=0x%08x\n", private->fp.d.fcsr);

    for (i = 0; i < array_size(private->fp.d.f) / 2; i++) {
        vmm_cdev_printf(
            cdev, "            f%02d=0x%016" PRIx64 "         f%02d=0x%016" PRIx64 "\n", (2 * i), private->fp.d.f[2 * i], (2 * i + 1),
            private->fp.d.f[2 * i + 1]);
    }
}
