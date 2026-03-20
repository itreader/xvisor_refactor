/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file arch_regs.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief common header file for CPU registers
 */
#ifndef _ARCH_REGS_H__
#define _ARCH_REGS_H__

#include <cpu_defines.h>
#include <vmm_compiler.h>
#include <vmm_cpumask.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

struct arch_regs {
    /* X0 - X29 */
    uint64_t gpr[CPU_GPR_COUNT];
    /* Link Register (or X30) */
    uint64_t lr;
    /* Stack Pointer */
    uint64_t sp;
    /* Program Counter */
    uint64_t pc;
    /* PState/CPSR */
    uint64_t pstate;
} __packed;

typedef struct arch_regs arch_regs_t;

/* Note: This structure is accessed from assembly code
 * hence any change in this structure should be reflected
 * in relevant defines in cpu_defines.h
 */
struct arm_priv_ptrauth {
    uint64_t apiakeylo_el1; /* 0x0 */
    uint64_t apiakeyhi_el1; /* 0x8 */
    uint64_t apibkeylo_el1; /* 0x10 */
    uint64_t apibkeyhi_el1; /* 0x18 */
    uint64_t apdakeylo_el1; /* 0x20 */
    uint64_t apdakeyhi_el1; /* 0x28 */
    uint64_t apdbkeylo_el1; /* 0x30 */
    uint64_t apdbkeyhi_el1; /* 0x38 */
    uint64_t apgakeylo_el1; /* 0x40 */
    uint64_t apgakeyhi_el1; /* 0x48 */
};

/* Note: This structure is accessed from assembly code
 * hence any change in this structure should be reflected
 * in relevant defines in cpu_defines.h
 */
struct arm_priv_vfp {
    /* 64bit EL1/EL0 registers */
    uint32_t mvfr0; /* 0x0 */
    uint32_t mvfr1; /* 0x4 */
    uint32_t mvfr2; /* 0x8 */
    uint32_t fpcr;  /* 0xC */
    uint32_t fpsr;  /* 0x10 */
    /* 32bit only registers */
    uint32_t fpexc32; /* 0x14 */
    /* 32x 128bit floating point registers. */
    uint64_t fpregs[64]; /* 0x18 */
} __packed;

/* Note: This structure is accessed from assembly code
 * hence any change in this structure should be reflected
 * in relevant defines in cpu_defines.h
 */
struct arm_priv_sysregs {
    /* 64bit EL1/EL0 registers */
    uint64_t sp_el0;    /* 0x0 */
    uint64_t sp_el1;    /* 0x8 */
    uint64_t elr_el1;   /* 0x10 */
    uint64_t spsr_el1;  /* 0x18 */
    uint64_t midr_el1;  /* 0x20 */
    uint64_t mpidr_el1; /* 0x28 */
    /* System control register. */
    uint64_t sctlr_el1; /* 0x30 */
    /* Auxillary control register. */
    uint64_t actlr_el1; /* 0x38 */
    /* Coprocessor access register.  */
    uint64_t cpacr_el1; /* 0x40 */
    /* MMU translation table base 0. */
    uint64_t ttbr0_el1; /* 0x48 */
    /* MMU translation table base 1. */
    uint64_t ttbr1_el1; /* 0x50 */
    /* MMU translation control register. */
    uint64_t tcr_el1; /* 0x58 */
    /* Exception status register. */
    uint64_t esr_el1; /* 0x60 */
    /* Fault address register. */
    uint64_t far_el1; /* 0x68 */
    /* Translation result. */
    uint64_t par_el1; /* 0x70 */
    /* Memory attribute index Register */
    uint64_t mair_el1; /* 0x78 */
    /* Vector base address register */
    uint64_t vbar_el1; /* 0x80 */
    /* Context ID. */
    uint64_t contextidr_el1; /* 0x88 */
    /* User RW Thread register. */
    uint64_t tpidr_el0; /* 0x90 */
    /* Privileged Thread register. */
    uint64_t tpidr_el1; /* 0x98 */
    /* User RO Thread register. */
    uint64_t tpidrro_el0; /* 0xA0 */
    /* 32bit only registers */
    uint32_t spsr_abt; /* 0xA8 */
    uint32_t spsr_und; /* 0xAC */
    uint32_t spsr_irq; /* 0xB0 */
    uint32_t spsr_fiq; /* 0xB4 */
    /* MMU domain access control register */
    uint32_t dacr32_el2; /* 0xB8 */
    /* Fault status registers. */
    uint32_t ifsr32_el2; /* 0xBC */
    /* 32bit only ThumbEE registers */
    uint32_t teecr32_el1;  /* 0xC0 */
    uint32_t teehbr32_el1; /* 0xC4 */
} __packed;

struct arm_private {
    /* Internal CPU feature flags. */
    uint32_t                cpuid;
    uint64_t                features;
    /* Hypervisor context */
    vmm_spinlock_t          hcr_lock;
    uint64_t                hcr;  /* Hypervisor Configuration */
    uint64_t                cptr; /* Coprocessor Trap Register */
    uint64_t                hstr; /* Hypervisor System Trap Register */
    /* EL1/EL0 sysregs */
    struct arm_priv_sysregs sysregs;
    vmm_cpumask_t           dflush_needed;
    /* VFP & SMID context */
    struct arm_priv_vfp     vfp;
    /* Pointer Authentication context */
    struct arm_priv_ptrauth ptrauth;
    /* Last host CPU on which this VCPU ran */
    uint32_t                last_hcpu;
    /* Generic timer context */
    void                   *gentimer_private;
    /* VGIC context */
    bool                    vgic_avail;
    void (*vgic_save)(void *vcpu_ptr);
    void (*vgic_restore)(void *vcpu_ptr);
    bool (*vgic_irq_pending)(void *vcpu_ptr);
    void *vgic_private;
};

struct arm_guest_private {
    /* Stage2 table */
    struct mmu_page_table *ttbl;
    /* PSCI version
     * Bits[31:16] = Major number
     * Bits[15:0] = Minor number
     */
    uint32_t               psci_version;
};

#define arm_regs(vcpu)                (&((vcpu)->regs))
#define arm_private(vcpu)             ((struct arm_private *)((vcpu)->arch_private))
#define arm_guest_private(guest)      ((struct arm_guest_private *)((guest)->arch_private))

#define arm_cpuid(vcpu)               (arm_private(vcpu)->cpuid)
#define arm_set_feature(vcpu, feat)   (arm_private(vcpu)->features |= (0x1ULL << (feat)))
#define arm_clear_feature(vcpu, feat) (arm_private(vcpu)->features &= ~(0x1ULL << (feat)))
#define arm_feature(vcpu, feat)       (arm_private(vcpu)->features & (0x1ULL << (feat)))

/**
 *  Instruction emulation support macros
 */
#define arm_cpsr(regs)                ((uint32_t)((regs)->pstate & 0xffffffff))
#define arm_pc(regs)                  ((regs)->pc)

/**
 *  Generic timers support macro
 */
#define arm_gentimer_context(vcpu)    (arm_private(vcpu)->gentimer_private)

/**
 *  VGIC support macros
 */
#define arm_vgic_setup(vcpu, __save_func, __restore_func, __irq_pending_func, __private)                                                             \
    do {                                                                                                                                             \
        arm_private(vcpu)->vgic_avail       = TRUE;                                                                                                  \
        arm_private(vcpu)->vgic_save        = __save_func;                                                                                           \
        arm_private(vcpu)->vgic_restore     = __restore_func;                                                                                        \
        arm_private(vcpu)->vgic_irq_pending = __irq_pending_func;                                                                                    \
        arm_private(vcpu)->vgic_private     = __private;                                                                                             \
    } while (0)
#define arm_vgic_cleanup(vcpu)                                                                                                                       \
    do {                                                                                                                                             \
        arm_private(vcpu)->vgic_avail       = FALSE;                                                                                                 \
        arm_private(vcpu)->vgic_save        = NULL;                                                                                                  \
        arm_private(vcpu)->vgic_restore     = NULL;                                                                                                  \
        arm_private(vcpu)->vgic_irq_pending = NULL;                                                                                                  \
        arm_private(vcpu)->vgic_private     = NULL;                                                                                                  \
    } while (0)
#define arm_vgic_avail(vcpu) (arm_private(vcpu)->vgic_avail)
#define arm_vgic_save(vcpu)                                                                                                                          \
    if (arm_vgic_avail(vcpu)) {                                                                                                                      \
        arm_private(vcpu)->vgic_save(vcpu);                                                                                                          \
    }
#define arm_vgic_restore(vcpu)                                                                                                                       \
    if (arm_vgic_avail(vcpu)) {                                                                                                                      \
        arm_private(vcpu)->vgic_restore(vcpu);                                                                                                       \
    }
#define arm_vgic_irq_pending(vcpu)                                                                                                                   \
    ({                                                                                                                                               \
        bool __r = FALSE;                                                                                                                            \
        if (arm_vgic_avail(vcpu)) {                                                                                                                  \
            __r = arm_private(vcpu)->vgic_irq_pending(vcpu);                                                                                         \
        }                                                                                                                                            \
        __r;                                                                                                                                         \
    })
#define arm_vgic_private(vcpu) (arm_private(vcpu)->vgic_private)

#endif
