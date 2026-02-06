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
 * @file mathlib.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief library for common math operations
 */

#ifndef __MATHLIB_H__
#define __MATHLIB_H__

#include <arch_config.h>
#include <vmm_types.h>

#if !defined(ARCH_HAS_DIVISON_OPERATION)

#define do_abs(x) ((x) < 0 ? -(x) : (x))

extern uint64_t do_udiv64(uint64_t dividend, uint64_t divisor, uint64_t *remainder);

static inline uint64_t udiv64(uint64_t value, uint64_t divisor)
{
    uint64_t r;
    return do_udiv64(value, divisor, &r);
}

static inline uint64_t umod64(uint64_t value, uint64_t divisor)
{
    uint64_t r;
    do_udiv64(value, divisor, &r);
    return r;
}

static inline int64_t sdiv64(int64_t value, int64_t divisor)
{
    uint64_t r;

    if ((value * divisor) < 0) {
        return -do_udiv64(do_abs(value), do_abs(divisor), &r);
    } else { /* positive value */
        return do_udiv64(do_abs(value), do_abs(divisor), &r);
    }
}

static inline int64_t smod64(int64_t value, int64_t divisor)
{
    uint64_t r;
    do_udiv64(do_abs(value), do_abs(divisor), &r);

    if (value < 0) {
        return -r;
    } else { /* positive value */
        return r;
    }
}

extern uint32_t do_udiv32(uint32_t dividend, uint32_t divisor, uint32_t *remainder);

static inline uint32_t udiv32(uint32_t value, uint32_t divisor)
{
    uint32_t r;
    return do_udiv32(value, divisor, &r);
}

static inline uint32_t umod32(uint32_t value, uint32_t divisor)
{
    uint32_t r;
    do_udiv32(value, divisor, &r);
    return r;
}

static inline int32_t sdiv32(int32_t value, int32_t divisor)
{
    uint32_t r;

    if ((value * divisor) < 0) {
        return -do_udiv32(do_abs(value), do_abs(divisor), &r);
    } else { /* positive value */
        return do_udiv32(do_abs(value), do_abs(divisor), &r);
    }
}

static inline int32_t smod32(int32_t value, int32_t divisor)
{
    uint32_t r;
    do_udiv32(do_abs(value), do_abs(divisor), &r);

    if (value < 0) {
        return -r;
    } else { /* positive value */
        return r;
    }
}

#else

#include <arch_math.h>

/** Unsigned 64-bit divison.
 *  Prototype: uint64_t udiv64(uint64_t value, uint64_t divisor)
 */
#define udiv64(value, divisor) arch_udiv64(value, divisor)

/** Unsigned 64-bit modulus.
 *  Prototype: uint64_t umod64(uint64_t value, uint64_t divisor)
 */
#define umod64(value, divisor) arch_umod64(value, divisor)

/** Signed 64-bit divison.
 *  Prototype: int64_t sdiv64(int64_t value, int64_t divisor)
 */
#define sdiv64(value, divisor) arch_sdiv64(value, divisor)

/** Signed 64-bit modulus.
 *  Prototype: int64_t smod64(int64_t value, int64_t divisor)
 */
#define smod64(value, divisor) arch_smod64(value, divisor)

/** Unsigned 32-bit divison.
 *  Prototype: uint32_t udiv32(uint32_t value, uint32_t divisor)
 */
#define udiv32(value, divisor) arch_udiv32(value, divisor)

/** Unsigned 32-bit modulus.
 *  Prototype: uint32_t umod32(uint32_t value, uint32_t divisor)
 */
#define umod32(value, divisor) arch_umod32(value, divisor)

/** Signed 32-bit divison.
 *  Prototype: int32_t sdiv32(int32_t value, int32_t divisor)
 */
#define sdiv32(value, divisor) arch_sdiv32(value, divisor)

/** Signed 32-bit modulus.
 *  Prototype: int32_t smod32(int32_t value, int32_t divisor)
 */
#define smod32(value, divisor) arch_smod32(value, divisor)

#endif

/* Unsigned integer round-up macros */
#define DIV_ROUND_UP(n, d) udiv64(((n) + (d) - 1), (d))
#define DIV_ROUND_UP_ULL(ll, d)       \
    ({                                \
        uint64_t _t = (ll) + (d) - 1; \
        _t          = udiv64(_t, d);  \
        _t;                           \
    })
#define DIV_ROUND_CLOSEST(n, d) udiv64(((n) + (d) / 2), (d))
#define DIV_ROUND_CLOSEST_ULL(ll, d)  \
    ({                                \
        uint64_t _t = (ll) + (d) / 2; \
        _t          = udiv64(_t, d);  \
        _t;                           \
    })

/**
 * Rough approximation to sqrt
 * @x: integer of which to calculate the sqrt
 *
 * A very rough approximation to the sqrt() function.
 */
uint64_t int_sqrt(uint64_t x);

/**
 * Compute with 96 bit intermediate result: (a*b)/c
 */
static inline uint64_t muldiv64(uint64_t a, uint32_t b, uint32_t c)
{
    union {
        uint64_t ll;

        struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
            uint32_t low, high;
#else
            uint32_t high, low;
#endif
        } l;
    } u, res;

    uint64_t rl, rh;

    u.ll = a;
    rl   = (uint64_t)u.l.low * (uint64_t)b;
    rh   = (uint64_t)u.l.high * (uint64_t)b;
    rh += (rl >> 32);
    res.l.high = udiv64(rh, c);
    res.l.low  = udiv64(((umod64(rh, c) << 32) + (rl & 0xffffffff)), c);
    return res.ll;
}

/**
 * Compute the Greatest Common Divisor of two numbers.
 *
 */

uint64_t gcd(uint64_t a, uint64_t b);

#endif /* __MATHLIB_H__ */
