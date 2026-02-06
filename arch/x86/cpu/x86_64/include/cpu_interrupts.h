/**
 * Copyright (c) 2010-20 Himanshu Chauhan.
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
 * @file cpu_interrupts.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief header file for cpu interrupts
 */
#ifndef _CPU_INTERRUPTS_H__
#define _CPU_INTERRUPTS_H__

#include <arch_regs.h>
#include <cpu_asm_macros.h>
#include <vmm_types.h>

/* 8259A interrupt controller ports. */
#define INT_CTL                     0x20 /* I/O port for interrupt controller */
#define INT_CTLMASK                 0x21 /* setting bits in this port disables ints */
#define INT2_CTL                    0xA0 /* I/O port for second interrupt controller */
#define INT2_CTLMASK                0xA1 /* setting bits in this port disables ints */

/* Magic numbers for interrupt controller. */
#define END_OF_INT                  0x20 /* code used to re-enable after an interrupt */

/* Hardware interrupt numbers. */
#define NR_GATES                    256
#define NR_IRQ_VECTORS              NR_GATES
#define USER_DEFINED_IRQ_BASE       32
#define LAPIC_TIMER_IRQ_VECTOR      USER_DEFINED_IRQ_BASE
#define IOAPIC_IRQ_BASE             LAPIC_TIMER_IRQ_VECTOR

#define IRQ_VECTOR_TO_IRQ(__vector) ((__vector < USER_DEFINED_IRQ_BASE) ? -1 : (__vector - USER_DEFINED_IRQ_BASE))
#define IRQ_TO_IRQ_VECTOR(__irq)    (__irq + USER_DEFINED_IRQ_BASE)

/* Interrupt Descriptor Table */
/* Segment Selector and Offset (SSO) */
union _sso {
    uint32_t val;

    struct {
        uint32_t offset   : 16;
        uint32_t selector : 16;
    } bits;
} __packed;

/* offset and type */
union _ot {
    uint32_t val;

    struct {
        uint32_t ist     : 3;
        uint32_t rz      : 5;
        uint32_t type    : 4;
        uint32_t z       : 1; /* should be zero */
        uint32_t dpl     : 2;
        uint32_t present : 1;
        uint32_t offset  : 16;
    } bits;
} __packed;

/* offset 32-63 bits */
union _off {
    uint32_t va;

    struct {
        uint32_t offset;
    } bits;
} __packed;

/* Trap and Interrupt Gate */
struct gate_descriptor {
    union _sso sso;
    union _ot  ot;
    union _off off;
    uint32_t   reserved;
} __packed;

struct idt64_ptr {
    uint16_t idt_limit;
    uint64_t idt_base;
} __packed;

#define DEBUG_STACK              1
#define STACKFAULT_STACK         2
#define DOUBLEFAULT_STACK        3
#define NMI_STACK                4
#define REGULAR_INT_STACK        5
#define MCE_STACK                6
#define EXCEPTION_STACK          7

#define N_EXCEPTION_STACKS       7

/*
 * Change any of the values below will entail a
 * respective change in the linker script.
 */
#define IRQ_STACK_SIZE           0x1000UL
#define EXEC_STACK_SIZE          0x2000UL

/*
 * These values are just flag bits not the actual value of
 * type to be written to register.
 */
#define IDT_GATE_TYPE_INTERRUPT  (0x01UL << 0)
#define IDT_GATE_TYPE_TRAP       (0x01UL << 1)
#define IDT_GATE_TYPE_CALL       (0x01UL << 2)

/* IA-32e mode types */
#define _GATE_TYPE_LDT           0x2
#define _GATE_TYPE_TSS_AVAILABLE 0x9
#define _GATE_TYPE_TSS_BUSY      0xB
#define _GATE_TYPE_CALL          0xC
#define _GATE_TYPE_INTERRUPT     0xE
#define _GATE_TYPE_TRAP          0xF

#define NR_IST_STACKS            7

/* Task state segment:
 * We need one because, x86 requires at least one TSS be present
 * and we want to use interrupt stack table. In IA32e mode task
 * switching isn't supported by the processor. Instead Intel chose
 * to reuse the TSS as IST in 64-bit mode. Another hack?
 */
struct tss_64 {
    uint32_t resvd_0;
    uint32_t rsp0_lo;
    uint32_t rsp0_hi;
    uint32_t rsp1_lo;
    uint32_t rsp1_hi;
    uint32_t rsp2_lo;
    uint32_t rsp2_hi;
    uint32_t resvd_1;
    uint32_t resvd_2;
    uint32_t ist1_lo;
    uint32_t ist1_hi;
    uint32_t ist2_lo;
    uint32_t ist2_hi;
    uint32_t ist3_lo;
    uint32_t ist3_hi;
    uint32_t ist4_lo;
    uint32_t ist4_hi;
    uint32_t ist5_lo;
    uint32_t ist5_hi;
    uint32_t ist6_lo;
    uint32_t ist6_hi;
    uint32_t ist7_lo;
    uint32_t ist7_hi;
    uint32_t resvd_3;
    uint32_t resvd_4;
    uint32_t map_base;
};

union tss_desc_base_limit {
    uint32_t val;

    struct {
        uint32_t tss_limit : 16;
        uint32_t tss_base1 : 16;
    } bits;
} __packed;

union tss_desc_base_type {
    uint32_t val;

    struct {
        uint32_t tss_base2   : 8;
        uint32_t type        : 4;
        uint32_t resvd1      : 1;
        uint32_t dpl         : 2;
        uint32_t present     : 1;
        uint32_t limit       : 4;
        uint32_t avl         : 1;
        uint32_t resvd0      : 2;
        uint32_t granularity : 1;
        uint32_t tss_base3   : 8;
    } bits;
} __packed;

union tss_desc_base {
    uint32_t val;

    struct {
        uint32_t tss_base4;
    } bits;
} __packed;

struct tss64_desc {
    union tss_desc_base_limit tbl;
    union tss_desc_base_type  tbt;
    union tss_desc_base       tb;
    uint32_t                  reserved;
} __packed;

struct segment {
    uint16_t selector;
    uint32_t access_rights;
    uint32_t limit;
    uint64_t base;
};

/* Interrupt handlers. */
extern void _exception_div_error(void);
extern void _exception_debug(void);
extern void _exception_bp(void);
extern void _exception_ovf(void);
extern void _exception_bounds(void);
extern void _exception_inval_opc(void);
extern void _exception_no_dev(void);
extern void _exception_double_fault(void);
extern void _exception_coproc_overrun(void);
extern void _exception_inval_tss(void);
extern void _exception_missing_seg(void);
extern void _exception_missing_stack(void);
extern void _exception_gpf(void);
extern void _exception_coproc_err(void);
extern void _exception_align_check(void);
extern void _exception_machine_check(void);
extern void _exception_simd_err(void);
extern void _exception_nmi(void);
extern void _exception_page_fault(void);
extern void __IRQ_32(void);
extern void _generic_handler(void);

extern void reload_host_tss(void);
#endif /* _CPU_INTERRYPTS_H__ */
