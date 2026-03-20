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
 * @file arch_types.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief common header file for typedefs
 */
#ifndef __ARCH_TYPES_H__
#define __ARCH_TYPES_H__

typedef char           int8_t;
typedef short          int16_t;
typedef int            int32_t;
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef uint64_t       size_t;
typedef uint32_t bool;
typedef uint64_t ulong;

/** Boolean macros */
#define TRUE         1
#define FALSE        0
#define NULL         ((void *)0)

#define stringify(s) tostring(s)
#define tostring(s)  #s

#if __riscv_xlen == 32
typedef long long int64_t;
typedef uint64_t  uint64_t;
#define PRu64 "%llx"
#else
typedef long     int64_t;
typedef uint64_t uint64_t;
#define PRu64 "%lx"
#endif
typedef uint64_t irq_flags_t;
typedef uint64_t virtual_addr_t;
typedef uint64_t virtual_size_t;
typedef uint64_t physical_addr_t;
typedef uint64_t physical_size_t;

typedef struct {
    volatile long counter;
} atomic_t;

struct pt_regs {
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
} __attribute((packed));

#define _swab32(x)                                                                                                                                   \
    ((uint32_t)((((uint32_t)(x) & (uint32_t)0x000000ffU) << 24) | (((uint32_t)(x) & (uint32_t)0x0000ff00U) << 8) |                                   \
                (((uint32_t)(x) & (uint32_t)0x00ff0000U) >> 8) | (((uint32_t)(x) & (uint32_t)0xff000000U) >> 24)))

#define _swab64(x)                                                                                                                                   \
    ((uint64_t)((((uint64_t)(x) & (uint64_t)0x00000000000000ffULL) << 56) | (((uint64_t)(x) & (uint64_t)0x000000000000ff00ULL) << 40) |              \
                (((uint64_t)(x) & (uint64_t)0x0000000000ff0000ULL) << 24) | (((uint64_t)(x) & (uint64_t)0x00000000ff000000ULL) << 8) |               \
                (((uint64_t)(x) & (uint64_t)0x000000ff00000000ULL) >> 8) | (((uint64_t)(x) & (uint64_t)0x0000ff0000000000ULL) >> 24) |               \
                (((uint64_t)(x) & (uint64_t)0x00ff000000000000ULL) >> 40) | (((uint64_t)(x) & (uint64_t)0xff00000000000000ULL) >> 56)))

#define cpu_to_be32(x)  _swab32(x)
#define cpu_to_be64(x)  _swab64(x)
#define be32_to_cpu(x)  _swab32(x)
#define be64_to_cpu(x)  _swab64(x)
#define be32_to_cpup(x) _swab32(*(x))

#define max(a, b)       ((a) < (b) ? (b) : (a))

#endif /* __ARCH_TYPES_H__ */
