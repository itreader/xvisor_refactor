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
 * @file cpu_vcpu_sbi_legacy.c
 * @author Atish Patra (atish.patra@wdc.com)
 * @brief source of SBI legacy extensions
 */

#include <cpu_guest_serial.h>
#include <cpu_sbi.h>
#include <cpu_tlb.h>
#include <cpu_vcpu_sbi.h>
#include <cpu_vcpu_timer.h>
#include <cpu_vcpu_trap.h>
#include <cpu_vcpu_unprivate.h>
#include <riscv_sbi.h>
#include <vio/vmm_vserial.h>
#include <vmm_cpumask.h>
#include <vmm_error.h>
#include <vmm_manager.h>
#include <vmm_vcpu_irq.h>

static int vcpu_sbi_legacy_ecall(vmm_vcpu_t *vcpu, uint64_t ext_id, uint64_t func_id, uint64_t *args, struct cpu_vcpu_sbi_return *out)
{
    uint8_t                    send;
    uint32_t                   host_cpu;
    int                        i, ret = 0;
    uint64_t                   hmask;
    vmm_vcpu_t                *rvcpu;
    vmm_cpumask_t              cm, hm;
    struct vmm_guest          *guest = vcpu->guest;
    struct riscv_guest_serial *gs    = riscv_guest_serial(guest);

    switch (ext_id) {
        case SBI_EXT_0_1_SET_TIMER:
            if (riscv_private(vcpu)->xlen == 32) {
                cpu_vcpu_timer_start(vcpu, ((uint64_t)args[1] << 32) | (uint64_t)args[0]);
            } else {
                cpu_vcpu_timer_start(vcpu, (uint64_t)args[0]);
            }

            break;

        case SBI_EXT_0_1_CONSOLE_PUTCHAR:
            send = (uint8_t)args[0];
            vmm_vserial_receive(gs->vserial, &send, 1);
            break;

        case SBI_EXT_0_1_CONSOLE_GETCHAR:
            /* TODO: Implement get function if required */
            ret = SBI_ERR_NOT_SUPPORTED;
            break;

        case SBI_EXT_0_1_CLEAR_IPI:
            vmm_vcpu_irq_clear(vcpu, IRQ_VS_SOFT);
            break;

        case SBI_EXT_0_1_SEND_IPI:
            if (args[0]) {
                hmask = __cpu_vcpu_unpriv_read_ulong(args[0], out->trap);
            } else {
                hmask = (1UL << guest->vcpu_count) - 1;
            }

            if (out->trap->scause) {
                break;
            }

            for_each_set_bit(i, &hmask, BITS_PER_LONG)
            {
                rvcpu = vmm_manager_guest_vcpu(guest, i);

                if (!(vmm_manager_vcpu_get_state(rvcpu) & VMM_VCPU_STATE_INTERRUPTIBLE)) {
                    continue;
                }

                vmm_vcpu_irq_assert(rvcpu, IRQ_VS_SOFT, 0x0);
            }
            break;

        case SBI_EXT_0_1_SHUTDOWN:
            ret = vmm_manager_guest_shutdown_request(guest);

            if (ret) {
                vmm_printf(
                    "%s: guest %s shutdown request failed "
                    "with error = %d\n",
                    __func__, guest->name, ret);
                ret = SBI_ERR_FAILED;
            }

            break;

        case SBI_EXT_0_1_REMOTE_FENCE_I:
        case SBI_EXT_0_1_REMOTE_SFENCE_VMA:
        case SBI_EXT_0_1_REMOTE_SFENCE_VMA_ASID:
            if (args[0]) {
                hmask = __cpu_vcpu_unpriv_read_ulong(args[0], out->trap);
            } else {
                hmask = (1UL << guest->vcpu_count) - 1;
            }

            if (out->trap->scause) {
                break;
            }

            vmm_cpumask_clear(&cm);
            for_each_set_bit(i, &hmask, BITS_PER_LONG)
            {
                rvcpu = vmm_manager_guest_vcpu(guest, i);

                if (!(vmm_manager_vcpu_get_state(rvcpu) & VMM_VCPU_STATE_INTERRUPTIBLE)) {
                    continue;
                }

                if (vmm_manager_vcpu_get_hcpu(rvcpu, &host_cpu)) {
                    continue;
                }

                vmm_cpumask_set_cpu(host_cpu, &cm);
            }
            sbi_cpumask_to_hartmask(&cm, &hm);

            if (ext_id == SBI_EXT_0_1_REMOTE_FENCE_I) {
                sbi_remote_fence_i(vmm_cpumask_bits(&hm));
            } else if (ext_id == SBI_EXT_0_1_REMOTE_SFENCE_VMA) {
                sbi_remote_hfence_vvma(vmm_cpumask_bits(&hm), args[1], args[2]);
            } else if (ext_id == SBI_EXT_0_1_REMOTE_SFENCE_VMA_ASID) {
                sbi_remote_hfence_vvma_asid(vmm_cpumask_bits(&hm), args[1], args[2], args[3]);
            }

            break;

        default:
            ret = SBI_ERR_NOT_SUPPORTED;
            break;
    };

    return ret;
}

const struct cpu_vcpu_sbi_extension vcpu_sbi_legacy = {
    .extid_start = SBI_EXT_0_1_SET_TIMER,
    .extid_end   = SBI_EXT_0_1_SHUTDOWN,
    .handle      = vcpu_sbi_legacy_ecall,
};
