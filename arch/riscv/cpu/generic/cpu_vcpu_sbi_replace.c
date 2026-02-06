/**
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
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
 * @file cpu_vcpu_sbi_replace.c
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief source of SBI v0.2 replacement extensions
 */

#include <cpu_sbi.h>
#include <cpu_vcpu_nested.h>
#include <cpu_vcpu_sbi.h>
#include <cpu_vcpu_timer.h>
#include <generic_mmu.h>
#include <riscv_sbi.h>
#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_manager.h>
#include <vmm_stdio.h>
#include <vmm_vcpu_irq.h>

static int vcpu_sbi_time_ecall(vmm_vcpu_t *vcpu, uint64_t ext_id, uint64_t func_id, uint64_t *args, struct cpu_vcpu_sbi_return *out)
{
    if (func_id != SBI_EXT_TIME_SET_TIMER) {
        return SBI_ERR_NOT_SUPPORTED;
    }

    if (riscv_private(vcpu)->xlen == 32) {
        cpu_vcpu_timer_start(vcpu, ((uint64_t)args[1] << 32) | (uint64_t)args[0]);
    } else {
        cpu_vcpu_timer_start(vcpu, (uint64_t)args[0]);
    }

    return 0;
}

const struct cpu_vcpu_sbi_extension vcpu_sbi_time = {
    .extid_start = SBI_EXT_TIME,
    .extid_end   = SBI_EXT_TIME,
    .handle      = vcpu_sbi_time_ecall,
};

static int vcpu_sbi_rfence_ecall(vmm_vcpu_t *vcpu, uint64_t ext_id, uint64_t func_id, uint64_t *args, struct cpu_vcpu_sbi_return *out)
{
    uint32_t                  host_cpu;
    vmm_vcpu_t               *rvcpu;
    vmm_cpumask_t             cm, hm;
    struct vmm_guest         *guest = vcpu->guest;
    uint64_t                  hgatp, hmask = args[0], hbase = args[1];
    struct riscv_priv_nested *nprivate = riscv_nested_private(vcpu);

    vmm_cpumask_clear(&cm);
    vmm_manager_for_each_guest_vcpu(rvcpu, guest)
    {
        if (!(vmm_manager_vcpu_get_state(rvcpu) & VMM_VCPU_STATE_INTERRUPTIBLE)) {
            continue;
        }

        if (hbase != -1UL) {
            if (rvcpu->subid < hbase) {
                continue;
            }

            if (!(hmask & (1UL << (rvcpu->subid - hbase)))) {
                continue;
            }
        }

        if (vmm_manager_vcpu_get_hcpu(rvcpu, &host_cpu)) {
            continue;
        }

        vmm_cpumask_set_cpu(host_cpu, &cm);
    }
    sbi_cpumask_to_hartmask(&cm, &hm);

    switch (func_id) {
        case SBI_EXT_RFENCE_REMOTE_FENCE_I:
            sbi_remote_fence_i(vmm_cpumask_bits(&hm));
            break;

        case SBI_EXT_RFENCE_REMOTE_SFENCE_VMA:
            sbi_remote_hfence_vvma(vmm_cpumask_bits(&hm), args[2], args[3]);
            break;

        case SBI_EXT_RFENCE_REMOTE_SFENCE_VMA_ASID:
            sbi_remote_hfence_vvma_asid(vmm_cpumask_bits(&hm), args[2], args[3], args[4]);
            break;

        case SBI_EXT_RFENCE_REMOTE_HFENCE_GVMA:
        case SBI_EXT_RFENCE_REMOTE_HFENCE_GVMA_VMID:
            /* Flush the nested software TLB of calling VCPU */
            cpu_vcpu_nested_software_tlb_flush(vcpu, args[2], args[3]);

            if (mmu_page_table_has_hw_tag(nprivate->page_table)) {
                /*
                 * We use two VMIDs for nested virtualization:
                 * one for virtual-HS/U modes and another for
                 * virtual-VS/VU modes. This means we need to
                 * restrict guest remote HFENCE.GVMA to VMID
                 * used for virtual-VS/VU modes.
                 */
                sbi_remote_hfence_gvma_vmid(vmm_cpumask_bits(&hm), args[2], args[3], mmu_page_table_has_hw_tag(nprivate->page_table));
            } else {
                /*
                 * No VMID support so we do remote HFENCE.GVMA
                 * accross all VMIDs.
                 */
                sbi_remote_hfence_gvma(vmm_cpumask_bits(&hm), args[2], args[3]);
            }

            break;

        case SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA:
        case SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA_ASID:
            hgatp = 0;

            if (mmu_page_table_has_hw_tag(nprivate->page_table)) {
                /*
                 * We use two VMIDs for nested virtualization:
                 * one for virtual-HS/U modes and another for
                 * virtual-VS/VU modes. This means we need to
                 * switch hgatp.VMID before doing forwarding
                 * SBI call to host firmware.
                 */
                hgatp = mmu_page_table_hw_tag(nprivate->page_table);
                hgatp = csr_swap(CSR_HGATP, hgatp << HGATP_VMID_SHIFT);
            }

            if (func_id == SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA) {
                sbi_remote_hfence_vvma(vmm_cpumask_bits(&hm), args[2], args[3]);
            } else {
                sbi_remote_hfence_vvma_asid(vmm_cpumask_bits(&hm), args[2], args[3], args[4]);
            }

            if (mmu_page_table_has_hw_tag(nprivate->page_table)) {
                csr_write(CSR_HGATP, hgatp);
            }

            break;

        default:
            return SBI_ERR_NOT_SUPPORTED;
    };

    return 0;
}

const struct cpu_vcpu_sbi_extension vcpu_sbi_rfence = {
    .extid_start = SBI_EXT_RFENCE,
    .extid_end   = SBI_EXT_RFENCE,
    .handle      = vcpu_sbi_rfence_ecall,
};

static int vcpu_sbi_ipi_ecall(vmm_vcpu_t *vcpu, uint64_t ext_id, uint64_t func_id, uint64_t *args, struct cpu_vcpu_sbi_return *out)
{
    vmm_vcpu_t       *rvcpu;
    struct vmm_guest *guest = vcpu->guest;
    uint64_t          hmask = args[0], hbase = args[1];

    if (func_id != SBI_EXT_IPI_SEND_IPI) {
        return SBI_ERR_NOT_SUPPORTED;
    }

    vmm_manager_for_each_guest_vcpu(rvcpu, guest)
    {
        if (!(vmm_manager_vcpu_get_state(rvcpu) & VMM_VCPU_STATE_INTERRUPTIBLE)) {
            continue;
        }

        if (hbase != -1UL) {
            if (rvcpu->subid < hbase) {
                continue;
            }

            if (!(hmask & (1UL << (rvcpu->subid - hbase)))) {
                continue;
            }
        }

        vmm_vcpu_irq_assert(rvcpu, IRQ_VS_SOFT, 0x0);
    }

    return 0;
}

const struct cpu_vcpu_sbi_extension vcpu_sbi_ipi = {
    .extid_start = SBI_EXT_IPI,
    .extid_end   = SBI_EXT_IPI,
    .handle      = vcpu_sbi_ipi_ecall,
};

static int vcpu_sbi_srst_ecall(vmm_vcpu_t *vcpu, uint64_t ext_id, uint64_t func_id, uint64_t *args, struct cpu_vcpu_sbi_return *out)
{
    int               ret;
    struct vmm_guest *guest = vcpu->guest;

    if (func_id != SBI_EXT_SRST_RESET) {
        return SBI_ERR_NOT_SUPPORTED;
    }

    if ((((uint32_t)-1U) <= ((uint64_t)args[0])) || (((uint32_t)-1U) <= ((uint64_t)args[1]))) {
        return SBI_ERR_INVALID_PARAM;
    }

    switch (args[0]) {
        case SBI_SRST_RESET_TYPE_SHUTDOWN:
            ret = vmm_manager_guest_shutdown_request(guest);

            if (ret) {
                vmm_printf(
                    "%s: guest %s shutdown request failed "
                    "with error = %d\n",
                    __func__, guest->name, ret);
            }

            break;

        case SBI_SRST_RESET_TYPE_COLD_REBOOT:
        case SBI_SRST_RESET_TYPE_WARM_REBOOT:
            ret = vmm_manager_guest_reboot_request(guest);

            if (ret) {
                vmm_printf(
                    "%s: guest %s reset request failed "
                    "with error = %d\n",
                    __func__, guest->name, ret);
            }

            break;

        default:
            return SBI_ERR_NOT_SUPPORTED;
    };

    return 0;
}

const struct cpu_vcpu_sbi_extension vcpu_sbi_srst = {
    .extid_start = SBI_EXT_SRST,
    .extid_end   = SBI_EXT_SRST,
    .handle      = vcpu_sbi_srst_ecall,
};
