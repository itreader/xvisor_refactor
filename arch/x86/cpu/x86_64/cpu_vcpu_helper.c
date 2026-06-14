/**
 * Copyright (c) 2011 Himanshu Chauhan
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
 * @file cpu_vcpu_helper.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source of VCPU helper functions
 */

#include <arch_cpu.h>
#include <arch_guest_helper.h>
#include <arch_regs.h>
#include <cpu_features.h>
#include <cpu_mmu.h>
#include <cpu_vm.h>
#include <libs/bitops.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_guest_address_space.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_manager.h>
#include <vmm_stdio.h>
#include <x86_debug_log.h>

DEFINE_X86_DEBUG_LOG_SUBSYS_LEVEL(x86_vcpu, X86_DEBUG_LOG_LVL_INFO);

void arch_vcpu_emergency_shutdown(struct vcpu_hw_context *context);

static void init_vcpu_capabilities(vmm_vcpu_t *vcpu)
{
    uint32_t funcs, b, c, d;
    struct x86_vcpu_priv *private = x86_vcpu_private(vcpu);
    struct cpuid_response    *func_response;
    extern struct cpuinfo_x86 cpu_info;

    for (funcs = CPUID_BASE_LFUNCSTD; funcs < CPUID_BASE_FUNC_LIMIT; funcs++) {
        func_response = (struct cpuid_response *)&private->standard_funcs[funcs];

        switch (funcs) {
            case CPUID_BASE_LFUNCSTD:
                cpuid(CPUID_BASE_LFUNCSTD, &func_response->resp_eax, &func_response->resp_ebx, &func_response->resp_ecx, &func_response->resp_edx);

                /* TODO: CPUID 7 as more features. Limiting to 4 right now. */
                func_response->resp_eax = CPUID_BASE_CACHE_CONF;
                X86_DEBUG_LOG(x86_vcpu, LVL_INFO, "Guest base CPUID Limited to 0x%" PRIx32 "\n", func_response->resp_eax);
                break;

            case CPUID_BASE_FEATURES:
                cpuid(CPUID_BASE_FEATURES, &func_response->resp_eax, &b, &c, &d);

                /* NR cpus and apic id */
                clear_bits(16, 31, (volatile uint64_t *)&b);
                b |= ((vcpu->subid << 24) | (vcpu->guest->vcpu_count << 16));
                func_response->resp_ebx = b;

                /* No VMX or x2APIC */
                if (cpu_info.vendor == x86_VENDOR_INTEL) {
                    clear_bit(CPUID_BASE_FEAT_BIT(FEATURES, ECX, X2APIC), (volatile uint64_t *)&c);
                    clear_bit(CPUID_BASE_FEAT_BIT(FEATURES, ECX, VMX), (volatile uint64_t *)&c);
                }

                clear_bit(CPUID_BASE_FEAT_BIT(FEATURES, ECX, MONITOR), (volatile uint64_t *)&c);
                func_response->resp_ecx = c;

                /* No PAE, MTRR, PGE, ACPI, PSE & MSR */
                clear_bit(CPUID_BASE_FEAT_BIT(FEATURES, EDX, PAE), (volatile uint64_t *)&d);
                clear_bit(CPUID_BASE_FEAT_BIT(FEATURES, EDX, MTRR), (volatile uint64_t *)&d);
                clear_bit(CPUID_BASE_FEAT_BIT(FEATURES, EDX, PGE), (volatile uint64_t *)&d);
                clear_bit(CPUID_BASE_FEAT_BIT(FEATURES, EDX, ACPI), (volatile uint64_t *)&d);
                clear_bit(CPUID_BASE_FEAT_BIT(FEATURES, EDX, HTT), (volatile uint64_t *)&d);
                clear_bit(CPUID_BASE_FEAT_BIT(FEATURES, EDX, PSE), (volatile uint64_t *)&d);
                clear_bit(CPUID_BASE_FEAT_BIT(FEATURES, EDX, MSR), (volatile uint64_t *)&d);

                func_response->resp_edx = d;
                break;

            default:
                X86_DEBUG_LOG(x86_vcpu, LVL_INFO, "CPUID: 0x%" PRIx32 " defaulting to CPU reported values\n", funcs);
                func_response->resp_eax = 0;
                func_response->resp_ebx = 0;
                func_response->resp_ecx = 0;
                func_response->resp_edx = 0;
                break;
        }
    }

    for (funcs = CPUID_EXTENDED_LFUNCEXTD; funcs < CPUID_EXTENDED_FUNC_LIMIT; funcs++) {
        func_response = (struct cpuid_response *)&private->extended_funcs[funcs - CPUID_EXTENDED_LFUNCEXTD];

        switch (funcs) {
            case CPUID_EXTENDED_LFUNCEXTD:
                cpuid(
                    CPUID_EXTENDED_FEATURES, &func_response->resp_eax, &func_response->resp_ebx, &func_response->resp_ecx, &func_response->resp_edx);

                func_response->resp_eax = CPUID_EXTENDED_ADDR_BITS;
                X86_DEBUG_LOG(x86_vcpu, LVL_INFO, "Guest extended CPUID Limited to 0x%" PRIx32 "\n", func_response->resp_eax);
                break;

            case CPUID_EXTENDED_FEATURES: /* replica of base features */
                cpuid(CPUID_EXTENDED_FEATURES, &func_response->resp_eax, &b, &c, &d);

                func_response->resp_ebx = b;
                func_response->resp_ecx = c;
                clear_bit(CPUID_BASE_FEAT_BIT(FEATURES, EDX, NX), (volatile uint64_t *)&d);
                func_response->resp_edx = d;
                break;

            case AMD_CPUID_EXTENDED_L1_CACHE_TLB_IDENTIFIER:
                if (cpu_info.vendor == x86_VENDOR_AMD) {
                    cpuid(
                        AMD_CPUID_EXTENDED_L1_CACHE_TLB_IDENTIFIER, &func_response->resp_eax, &func_response->resp_ebx, &func_response->resp_ecx,
                        &func_response->resp_edx);
                } else {
                    func_response->resp_eax = 0;
                    func_response->resp_ebx = 0;
                    func_response->resp_ecx = 0;
                    func_response->resp_edx = 0;
                }

                break;

            default:
                X86_DEBUG_LOG(x86_vcpu, LVL_INFO, "CPUID: 0x%" PRIx32 " defaulting to CPU reported values\n", funcs);
                cpuid(funcs, &func_response->resp_eax, &func_response->resp_ebx, &func_response->resp_ecx, &func_response->resp_edx);
                break;
        }
    }
}

static void arch_guest_vcpu_trampoline(vmm_vcpu_t *vcpu)
{
    X86_DEBUG_LOG(x86_vcpu, LVL_DEBUG, "Running VCPU %s\n", vcpu->name);
    cpu_boot_vcpu(x86_vcpu_private(vcpu)->hw_context);
    X86_DEBUG_LOG(x86_vcpu, LVL_ERR, "ERROR: Guest VCPU exited from run loop!\n");

    while (1)
        ; /* Should never come here! */
}

int arch_vcpu_init(vmm_vcpu_t *vcpu)
{
    uint64_t                  stack_start;
    extern struct cpuinfo_x86 cpu_info;

    if (!vcpu->is_normal) {
        /* For orphan vcpu */
        stack_start       = vcpu->stack_virtual_address + vcpu->stack_size - sizeof(uint64_t);
        vcpu->regs.rip    = vcpu->start_pc;
        vcpu->regs.rip    = vcpu->start_pc;
        vcpu->regs.rsp    = stack_start;
        vcpu->regs.cs     = VMM_CODE_SEG_SEL;
        vcpu->regs.ss     = VMM_DATA_SEG_SEL;
        vcpu->regs.rflags = (X86_EFLAGS_IF | X86_EFLAGS_PF | X86_EFLAGS_CF);
    } else {
        if (!vcpu->reset_count) {
            vcpu->arch_private = vmm_zalloc(sizeof(struct x86_vcpu_private));

            if (!vcpu->arch_private) {
                return VMM_ERR_FAIL;
            }

            INIT_SPIN_LOCK(&x86_vcpu_private(vcpu)->lock);

            init_vcpu_capabilities(vcpu);

            x86_vcpu_private(vcpu)->hw_context                          = vmm_zalloc(sizeof(struct vcpu_hw_context));
            x86_vcpu_private(vcpu)->hw_context->assoc_vcpu              = vcpu;
            x86_vcpu_private(vcpu)->hw_context->sign                    = 0xdeadbeef;

            x86_vcpu_private(vcpu)->hw_context->vcpu_emergency_shutdown = arch_vcpu_emergency_shutdown;
            cpu_init_vcpu_hw_context(&cpu_info, x86_vcpu_private(vcpu)->hw_context);

            /*
             * This vcpu has to run VMM code before and after guest mode
             * switch. Prepare for the same.
             */
            stack_start       = vcpu->stack_virtual_address + vcpu->stack_size - sizeof(uint64_t);
            vcpu->regs.rip    = (uint64_t)arch_guest_vcpu_trampoline;
            vcpu->regs.rsp    = stack_start;
            vcpu->regs.cs     = VMM_CODE_SEG_SEL;
            vcpu->regs.ss     = VMM_DATA_SEG_SEL;
            vcpu->regs.rdi    = (uint64_t)vcpu; /* this VCPU as parameter */
            vcpu->regs.rflags = (X86_EFLAGS_IF | X86_EFLAGS_PF | X86_EFLAGS_CF);
        }
    }

    return VMM_OK;
}

int arch_vcpu_deinit(vmm_vcpu_t *vcpu)
{
    return VMM_OK;
}

void arch_vcpu_switch(vmm_vcpu_t *vcpu_on_thread, vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    if (!vcpu_on_thread) {
        /* first time rescheduling */
        memcpy(regs, &vcpu->regs, sizeof(arch_regs_t));
    } else {
        memcpy(&vcpu_on_thread->regs, regs, sizeof(arch_regs_t));
        memcpy(regs, &vcpu->regs, sizeof(arch_regs_t));
    }
}

void arch_vcpu_post_switch(vmm_vcpu_t *vcpu, arch_regs_t *regs)
{
    /* Nothing to do here. */
}

void arch_vcpu_preempt_orphan(void)
{
    /* Trigger system call from hypervisor. This will
     * cause do_generic_int_handler() function to call
     * vmm_scheduler_preempt_orphan()
     */
    asm volatile("int $0x80\t\n");
}

static void __dump_vcpu_regs(vmm_char_device_t *cdev, arch_regs_t *regs)
{
    vmm_cdev_printf(cdev, "RAX: 0x%08lx RBX: 0x%08lx RCX: 0x%08lx RDX: 0x%08lx\n", regs->rax, regs->rbx, regs->rcx, regs->rdx);
    vmm_cdev_printf(cdev, "RDI: 0x%08lx RSI: 0x%08lx RBP: 0x%08lx R08: 0x%08lx\n", regs->rdi, regs->rsi, regs->rbp, regs->r8);
    vmm_cdev_printf(cdev, "R09: 0x%08lx R10: 0x%08lx R11: 0x%08lx R12: 0x%08lx\n", regs->r9, regs->r10, regs->r11, regs->r12);
    vmm_cdev_printf(cdev, "R13: 0x%08lx R14: 0x%08lx R15: 0x%08lx RIP: 0x%08lx\n", regs->r13, regs->r14, regs->r15, regs->rip);
    vmm_cdev_printf(cdev, "RSP: 0x%08lx RFLAGS: 0x%08lx HW-ERR: 0x%08lx\n", regs->rsp, regs->rflags, regs->hw_err_code);
    vmm_cdev_printf(cdev, "SS: 0x%08lx CS: 0x%08lx\n", regs->ss, regs->cs);
}

void dump_vcpu_regs(arch_regs_t *regs)
{
    __dump_vcpu_regs(NULL, regs);
}

void arch_vcpu_stat_dump(vmm_char_device_t *cdev, vmm_vcpu_t *vcpu)
{
    /* For now no arch specific stats */
}

static void dump_guest_vcpu_state(struct vcpu_hw_context *context)
{
    vmm_printf("\nGUEST %s dump state:\n\n", context->assoc_vcpu->name);

    if (context->cpuinfo->vendor == x86_VENDOR_AMD) {
        svm_dump_guest_state(context);
    } else {
        vmcs_dump(context);
    }

    vmm_printf("]\n");
}

void arch_vcpu_regs_dump(vmm_char_device_t *cdev, vmm_vcpu_t *vcpu)
{
    struct vcpu_hw_context *context = x86_vcpu_hw_context(vcpu);

    if (context) {
        dump_guest_vcpu_state(context);
    }
}

void arch_vcpu_emergency_shutdown(struct vcpu_hw_context *context)
{
    dump_guest_vcpu_state(context);
    vmm_manager_guest_halt(context->assoc_vcpu->guest);
}
