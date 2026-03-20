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
 * @file arch_regs.h
 * @author Anup Patel (anup@brainfault.org)
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
    /* CPSR */
    uint32_t cpsr;
    /* Program Counter */
    uint32_t pc;
    /* R0 - R12 */
    uint32_t gpr[CPU_GPR_COUNT];
    /* Stack Pointer */
    uint32_t sp;
    /* Link Register */
    uint32_t lr;
} __packed;

typedef struct arch_regs arch_regs_t;

/* Note: This structure is accessed from assembly code
 * hence any change in this structure should be reflected
 * in relevant defines in cpu_defines.h
 */
struct arm_priv_vfp {
    /* ID Registers */
    uint32_t fpsid; /* 0x0 */
    uint32_t mvfr0; /* 0x4 */
    uint32_t mvfr1; /* 0x8 */
    /* Control Registers */
    uint32_t fpexc;    /* 0xC */
    uint32_t fpscr;    /* 0x10 */
    uint32_t fpinst;   /* 0x14 */
    uint32_t fpinst2;  /* 0x18 */
    uint32_t reserved; /* 0x1C */
    /* General Purpose Registers */
    /* {d0-d15} 64bit floating point registers.*/
    uint64_t fpregs1[16]; /* 0x20 */
    /* {d16-d31} 64bit floating point registers.*/
    uint64_t fpregs2[16]; /* 0xA0 */
} __packed;

struct arm_priv_cp14 {
    /* ThumbEE Registers */
    uint32_t teecr;
    uint32_t teehbr;
};

/* Note: This structure is accessed from assembly code
 * hence any change in this structure should be reflected
 * in relevant defines in cpu_defines.h
 */
struct arm_priv_cp15 {
    /* ID Registers */
    uint32_t c0_midr;      /* 0x0 */
    uint32_t c0_mpidr;     /* 0x4 */
    uint32_t c0_cachetype; /* 0x8 */
    uint32_t c0_pfr0;      /* 0xC */
    uint32_t c0_pfr1;      /* 0x10 */
    uint32_t c0_dfr0;      /* 0x14 */
    uint32_t c0_afr0;      /* 0x18 */
    uint32_t c0_mmfr0;     /* 0x1C */
    uint32_t c0_mmfr1;     /* 0x20 */
    uint32_t c0_mmfr2;     /* 0x24 */
    uint32_t c0_mmfr3;     /* 0x28 */
    uint32_t c0_isar0;     /* 0x2C */
    uint32_t c0_isar1;     /* 0x30 */
    uint32_t c0_isar2;     /* 0x34 */
    uint32_t c0_isar3;     /* 0x38 */
    uint32_t c0_isar4;     /* 0x3C */
    uint32_t c0_isar5;     /* 0x40 */
    /* Cache id. */
    uint32_t c0_ccsid[16]; /* 0x44 */
    /* Cache level. */
    uint32_t c0_clid; /* 0x84 */
    /* Cache size selection. */
    uint32_t c0_cssel; /* 0x88 */
    /* System control register. */
    uint32_t c1_sctlr; /* 0x8C */
    /* Coprocessor access register.  */
    uint32_t c1_cpacr; /* 0x90 */
    /* MMU translation table base control. */
    uint32_t c2_ttbcr; /* 0x94 */
    /* MMU translation table base 0. */
    uint64_t c2_ttbr0; /* 0x98 */
    /* MMU translation table base 1. */
    uint64_t c2_ttbr1; /* 0xA0 */
    /* MMU domain access control register */
    uint32_t c3_dacr; /* 0xA8 */
    /* Fault status registers. */
    uint32_t c5_ifsr; /* 0xAC */
    /* Fault status registers. */
    uint32_t c5_dfsr; /* 0xB0 */
    /* Auxillary Fault status registers. */
    uint32_t c5_aifsr; /* 0xB4 */
    /* Auxillary Fault status registers. */
    uint32_t c5_adfsr; /* 0xB8 */
    /* Fault address registers. */
    uint32_t c6_ifar; /* 0xBC */
    /* Fault address registers. */
    uint32_t c6_dfar; /* 0xC0 */
    /* VA2PA Translation result. */
    uint32_t c7_par; /* 0xC4 */
    /* VA2PA Translation result. */
    uint64_t c7_par64; /* 0xC8 */
    /* Cache lockdown registers. */
    uint32_t c9_insn; /* 0xD0 */
    uint32_t c9_data; /* 0xD4 */
    /* Performance monitor control register */
    uint32_t c9_pmcr; /* 0xD8 */
    /* Perf monitor counter enables */
    uint32_t c9_pmcnten; /* 0xDC */
    /* Perf monitor overflow status */
    uint32_t c9_pmovsr; /* 0xE0 */
    /* Perf monitor event type */
    uint32_t c9_pmxevtyper; /* 0xE4 */
    /* Perf monitor user enable */
    uint32_t c9_pmuserenr; /* 0xE8 */
    /* Perf monitor interrupt enables */
    uint32_t c9_pminten; /* 0xEC */
    /* For Long-descriptor format this is MAIR0 */
    uint32_t c10_prrr; /* 0xF0 */
    /* For Long-descriptor format this is MAIR1 */
    uint32_t c10_nmrr; /* 0xF4 */
    /* Vector base address register */
    uint32_t c12_vbar; /* 0xF8 */
    /* FCSE PID. */
    uint32_t c13_fcseidr; /* 0xFC */
    /* Context ID. */
    uint32_t c13_contextidr; /* 0x100 */
    /* User RW Thread register. */
    uint32_t c13_tls1; /* 0x104 */
    /* User RO Thread register. */
    uint32_t c13_tls2; /* 0x108 */
    /* Privileged Thread register. */
    uint32_t c13_tls3; /* 0x10C */
    /* Maximum D-cache dirty line index. */
    uint32_t c15_i_max; /* 0x110 */
    /* Minimum D-cache dirty line index. */
    uint32_t c15_i_min; /* 0x114 */
} __packed;

/* Note: This structure is accessed from assembly code
 * hence any change in this structure should be reflected
 * in relevant defines in cpu_defines.h
 */
struct arm_priv_banked {
    uint32_t sp_usr; /* 0x0 */
    /* Supervisor Mode Registers */
    uint32_t sp_svc;   /* 0x4 */
    uint32_t lr_svc;   /* 0x8 */
    uint32_t spsr_svc; /* 0xC */
    /* Abort Mode Registers Registers */
    uint32_t sp_abt;   /* 0x10 */
    uint32_t lr_abt;   /* 0x14 */
    uint32_t spsr_abt; /* 0x18 */
    /* Undefined Mode Registers */
    uint32_t sp_und;   /* 0x1C */
    uint32_t lr_und;   /* 0x20 */
    uint32_t spsr_und; /* 0x24 */
    /* IRQ Mode Registers */
    uint32_t sp_irq;   /* 0x28 */
    uint32_t lr_irq;   /* 0x2C */
    uint32_t spsr_irq; /* 0x30 */
    /* FIQ Mode Registers */
    uint32_t gpr_fiq[CPU_FIQ_GPR_COUNT]; /* 0x34 */
    uint32_t sp_fiq;                     /* 0x48 */
    uint32_t lr_fiq;                     /* 0x4C */
    uint32_t spsr_fiq;                   /* 0x50 */
} __packed;

struct arm_private {
    /* Internal CPU feature flags. */
    uint32_t               cpuid;
    uint64_t               features;
    /* Hypervisor Configuration */
    vmm_spinlock_t         hcr_lock;
    uint32_t               hcr;
    uint32_t               hcptr;
    uint32_t               hstr;
    /* Banked Registers */
    struct arm_priv_banked bnk;
    /* VFP & SMID registers (cp10 & cp11 coprocessors) */
    struct arm_priv_vfp    vfp;
    /* Debug, Trace, and ThumbEE (cp14 coprocessor) */
    struct arm_priv_cp14   cp14;
    /* System control (cp15 coprocessor) */
    struct arm_priv_cp15   cp15;
    vmm_cpumask_t          dflush_needed;
    /* Last host CPU on which this VCPU ran */
    uint32_t               last_hcpu;
    /* Generic timer context */
    void                  *gentimer_private;
    /* VGIC context */
    bool                   vgic_avail;
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
#define arm_pc(regs)                  ((regs)->pc)
#define arm_cpsr(regs)                ((regs)->cpsr)

/**
 *  Generic timers support macros
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
