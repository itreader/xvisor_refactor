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
 * @file arch_regs.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief common header file for CPU registers
 */
#ifndef _ARCH_REGS_H__
#define _ARCH_REGS_H__

#include <vmm_const.h>

#define RISCV_PRIV_FP_F_F0   _AC(0x000, UL)
#define RISCV_PRIV_FP_F_F1   _AC(0x004, UL)
#define RISCV_PRIV_FP_F_F2   _AC(0x008, UL)
#define RISCV_PRIV_FP_F_F3   _AC(0x00c, UL)
#define RISCV_PRIV_FP_F_F4   _AC(0x010, UL)
#define RISCV_PRIV_FP_F_F5   _AC(0x014, UL)
#define RISCV_PRIV_FP_F_F6   _AC(0x018, UL)
#define RISCV_PRIV_FP_F_F7   _AC(0x01c, UL)
#define RISCV_PRIV_FP_F_F8   _AC(0x020, UL)
#define RISCV_PRIV_FP_F_F9   _AC(0x024, UL)
#define RISCV_PRIV_FP_F_F10  _AC(0x028, UL)
#define RISCV_PRIV_FP_F_F11  _AC(0x02c, UL)
#define RISCV_PRIV_FP_F_F12  _AC(0x030, UL)
#define RISCV_PRIV_FP_F_F13  _AC(0x034, UL)
#define RISCV_PRIV_FP_F_F14  _AC(0x038, UL)
#define RISCV_PRIV_FP_F_F15  _AC(0x03c, UL)
#define RISCV_PRIV_FP_F_F16  _AC(0x040, UL)
#define RISCV_PRIV_FP_F_F17  _AC(0x044, UL)
#define RISCV_PRIV_FP_F_F18  _AC(0x048, UL)
#define RISCV_PRIV_FP_F_F19  _AC(0x04c, UL)
#define RISCV_PRIV_FP_F_F20  _AC(0x050, UL)
#define RISCV_PRIV_FP_F_F21  _AC(0x054, UL)
#define RISCV_PRIV_FP_F_F22  _AC(0x058, UL)
#define RISCV_PRIV_FP_F_F23  _AC(0x05c, UL)
#define RISCV_PRIV_FP_F_F24  _AC(0x060, UL)
#define RISCV_PRIV_FP_F_F25  _AC(0x064, UL)
#define RISCV_PRIV_FP_F_F26  _AC(0x068, UL)
#define RISCV_PRIV_FP_F_F27  _AC(0x06c, UL)
#define RISCV_PRIV_FP_F_F28  _AC(0x070, UL)
#define RISCV_PRIV_FP_F_F29  _AC(0x074, UL)
#define RISCV_PRIV_FP_F_F30  _AC(0x078, UL)
#define RISCV_PRIV_FP_F_F31  _AC(0x07c, UL)
#define RISCV_PRIV_FP_F_FCSR _AC(0x080, UL)

#define RISCV_PRIV_FP_D_F0   _AC(0x000, UL)
#define RISCV_PRIV_FP_D_F1   _AC(0x008, UL)
#define RISCV_PRIV_FP_D_F2   _AC(0x010, UL)
#define RISCV_PRIV_FP_D_F3   _AC(0x018, UL)
#define RISCV_PRIV_FP_D_F4   _AC(0x020, UL)
#define RISCV_PRIV_FP_D_F5   _AC(0x028, UL)
#define RISCV_PRIV_FP_D_F6   _AC(0x030, UL)
#define RISCV_PRIV_FP_D_F7   _AC(0x038, UL)
#define RISCV_PRIV_FP_D_F8   _AC(0x040, UL)
#define RISCV_PRIV_FP_D_F9   _AC(0x048, UL)
#define RISCV_PRIV_FP_D_F10  _AC(0x050, UL)
#define RISCV_PRIV_FP_D_F11  _AC(0x058, UL)
#define RISCV_PRIV_FP_D_F12  _AC(0x060, UL)
#define RISCV_PRIV_FP_D_F13  _AC(0x068, UL)
#define RISCV_PRIV_FP_D_F14  _AC(0x070, UL)
#define RISCV_PRIV_FP_D_F15  _AC(0x078, UL)
#define RISCV_PRIV_FP_D_F16  _AC(0x080, UL)
#define RISCV_PRIV_FP_D_F17  _AC(0x088, UL)
#define RISCV_PRIV_FP_D_F18  _AC(0x090, UL)
#define RISCV_PRIV_FP_D_F19  _AC(0x098, UL)
#define RISCV_PRIV_FP_D_F20  _AC(0x0a0, UL)
#define RISCV_PRIV_FP_D_F21  _AC(0x0a8, UL)
#define RISCV_PRIV_FP_D_F22  _AC(0x0b0, UL)
#define RISCV_PRIV_FP_D_F23  _AC(0x0b8, UL)
#define RISCV_PRIV_FP_D_F24  _AC(0x0c0, UL)
#define RISCV_PRIV_FP_D_F25  _AC(0x0c8, UL)
#define RISCV_PRIV_FP_D_F26  _AC(0x0d0, UL)
#define RISCV_PRIV_FP_D_F27  _AC(0x0d8, UL)
#define RISCV_PRIV_FP_D_F28  _AC(0x0e0, UL)
#define RISCV_PRIV_FP_D_F29  _AC(0x0e8, UL)
#define RISCV_PRIV_FP_D_F30  _AC(0x0f0, UL)
#define RISCV_PRIV_FP_D_F31  _AC(0x0f8, UL)
#define RISCV_PRIV_FP_D_FCSR _AC(0x100, UL)

#ifndef __ASSEMBLY__

#include <vmm_compiler.h>
#include <vmm_types.h>

struct arch_regs {
    uint64_t zero;
    uint64_t ra;
    uint64_t sp;
    uint64_t gp;
    uint64_t tp;
    uint64_t t0;
    uint64_t t1;
    uint64_t t2;
    uint64_t s0;
    uint64_t s1;
    uint64_t a0;
    uint64_t a1;
    uint64_t a2;
    uint64_t a3;
    uint64_t a4;
    uint64_t a5;
    uint64_t a6;
    uint64_t a7;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t int8_t;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
    uint64_t t3;
    uint64_t t4;
    uint64_t t5;
    uint64_t t6;
    uint64_t sepc;
    uint64_t sstatus;
    uint64_t hstatus;
    uint64_t sp_exec;
};

typedef struct arch_regs arch_regs_t;

struct riscv_priv_fp_f {
    uint32_t f[32];
    uint32_t fcsr;
};

struct riscv_priv_fp_d {
    uint64_t f[32];
    uint32_t fcsr;
};

union riscv_priv_fp {
    struct riscv_priv_fp_f f;
    struct riscv_priv_fp_d d;
};

struct riscv_priv_nested {
    /* Nested virt state */
    bool                   virt;
    /* Nested interrupts timer event */
    void                  *timer_event;
    /* Nested software TLB */
    void                  *software_tlb;
    /* Nested shadow page table */
    struct mmu_page_table *page_table;
    /* Nested CSR state */
    uint64_t               hstatus;
    uint64_t               hedeleg;
    uint64_t               hideleg;
    uint64_t               hvip;
    uint64_t               hcounteren;
    uint64_t               htimedelta;
    uint64_t               htimedeltah;
    uint64_t               htval;
    uint64_t               htinst;
    uint64_t               henvcfg;
    uint64_t               henvcfgh;
    uint64_t               hgatp;
    uint64_t               vsstatus;
    uint64_t               vsie;
    uint64_t               vstvec;
    uint64_t               vsscratch;
    uint64_t               vsepc;
    uint64_t               vscause;
    uint64_t               vstval;
    uint64_t               vsatp;
    /* Nested AIA CSR state */
    uint64_t               hvictl;
};

#define RISCV_PRIV_MAX_TRAP_CAUSE 0x18

struct riscv_priv_stats {
    uint64_t trap[RISCV_PRIV_MAX_TRAP_CAUSE];
    uint64_t nested_enter;
    uint64_t nested_exit;
    uint64_t nested_vsirq;
    uint64_t nested_smode_csr_rmw;
    uint64_t nested_hext_csr_rmw;
    uint64_t nested_load_guest_page_fault;
    uint64_t nested_store_guest_page_fault;
    uint64_t nested_fetch_guest_page_fault;
    uint64_t nested_hfence_vvma;
    uint64_t nested_hfence_gvma;
    uint64_t nested_hlv;
    uint64_t nested_hsv;
    uint64_t nested_sbi;
};

struct riscv_priv {
    /* Register width */
    uint64_t                 xlen;
    /* ISA feature bitmap */
    uint64_t                *isa;
    /* Statistic data */
    struct riscv_priv_stats  stats;
    /* CSR state */
    uint64_t                 hie;
    uint64_t                 hip;
    uint64_t                 hvip;
    uint64_t                 henvcfg;
    uint64_t                 vsstatus;
    uint64_t                 vstvec;
    uint64_t                 vsscratch;
    uint64_t                 vsepc;
    uint64_t                 vscause;
    uint64_t                 vstval;
    uint64_t                 vsatp;
    uint64_t                 scounteren;
    /* Nested state */
    struct riscv_priv_nested nested;
    /* FP state */
    union riscv_priv_fp      fp;
    /* Opaque pointer to timer data */
    void                    *timer_private;
};

struct riscv_guest_priv {
    /* Time delta */
    uint64_t               time_delta;
    /* Stage2 pagetable */
    struct mmu_page_table *page_table;
    /* Opaque pointer to vserial data */
    void                  *guest_serial;
};

#define riscv_regs(vcpu)           (&((vcpu)->regs))
#define riscv_private(vcpu)        ((struct riscv_priv *)((vcpu)->arch_private))
#define riscv_stats_private(vcpu)  (&riscv_private(vcpu)->stats)
#define riscv_nested_private(vcpu) (&riscv_private(vcpu)->nested)
#define riscv_nested_virt(vcpu)    (riscv_nested_private(vcpu)->virt)
#define riscv_fp_private(vcpu)     (&riscv_private(vcpu)->fp)
#define riscv_timer_private(vcpu)  (riscv_private(vcpu)->timer_private)
#define riscv_guest_private(guest) ((struct riscv_guest_priv *)((guest)->arch_private))
#define riscv_guest_serial(guest)  (riscv_guest_private(guest)->guest_serial)

#endif

#endif
