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
 * @file bitops.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of generic bit operations.
 *
 * The source has been largely adapted from:
 * linux-xxx/lib/find_next_bit.c
 * linux-xxx/lib/find_last_bit.c
 *
 * The original code is licensed under the GPL.
 */

#include <libs/bitops.h>

#ifdef CONFIG_SMP
arch_spinlock_t __atomic_hash[ATOMIC_HASH_SIZE] = {
    ARCH_SPIN_LOCK_INITIALIZER,
    ARCH_SPIN_LOCK_INITIALIZER,
    ARCH_SPIN_LOCK_INITIALIZER,
    ARCH_SPIN_LOCK_INITIALIZER,
};
#endif

#define BITOP_WORD(nr) ((nr) / BITS_PER_LONG)

/*
 * Find the next set bit in a memory region.
 */
uint64_t find_next_bit(const uint64_t *addr, uint64_t size, uint64_t offset)
{
    const uint64_t *p      = addr + BITOP_WORD(offset);
    uint64_t        result = offset & ~(BITS_PER_LONG - 1);
    uint64_t        tmp;

    if (offset >= size) {
        return size;
    }

    size -= result;
    offset %= BITS_PER_LONG;

    if (offset) {
        tmp = *(p++);
        tmp &= (~0UL << offset);

        if (size < BITS_PER_LONG) {
            goto found_first;
        }

        if (tmp) {
            goto found_middle;
        }

        size -= BITS_PER_LONG;
        result += BITS_PER_LONG;
    }

    while (size & ~(BITS_PER_LONG - 1)) {
        if ((tmp = *(p++))) {
            goto found_middle;
        }

        result += BITS_PER_LONG;
        size -= BITS_PER_LONG;
    }

    if (!size) {
        return result;
    }

    tmp = *p;

found_first:
    tmp &= (~0UL >> (BITS_PER_LONG - size));

    if (tmp == 0UL) {         /* Are any bits set? */
        return result + size; /* Nope. */
    }

found_middle:
    return result + __ffs(tmp);
}

/*
 * This implementation of find_{first,next}_zero_bit was stolen from
 * Linus' asm-alpha/bitops.h.
 */
uint64_t find_next_zero_bit(const uint64_t *addr, uint64_t size, uint64_t offset)
{
    const uint64_t *p      = addr + BITOP_WORD(offset);
    uint64_t        result = offset & ~(BITS_PER_LONG - 1);
    uint64_t        tmp;

    if (offset >= size) {
        return size;
    }

    size -= result;
    offset %= BITS_PER_LONG;

    if (offset) {
        tmp = *(p++);
        tmp |= ~0UL >> (BITS_PER_LONG - offset);

        if (size < BITS_PER_LONG) {
            goto found_first;
        }

        if (~tmp) {
            goto found_middle;
        }

        size -= BITS_PER_LONG;
        result += BITS_PER_LONG;
    }

    while (size & ~(BITS_PER_LONG - 1)) {
        if (~(tmp = *(p++))) {
            goto found_middle;
        }

        result += BITS_PER_LONG;
        size -= BITS_PER_LONG;
    }

    if (!size) {
        return result;
    }

    tmp = *p;

found_first:
    tmp |= ~0UL << size;

    if (tmp == ~0UL) {        /* Are any bits zero? */
        return result + size; /* Nope. */
    }

found_middle:
    return result + ffz(tmp);
}

/*
 * Find the first set bit in a memory region.
 */
uint64_t find_first_bit(const uint64_t *addr, uint64_t size)
{
    const uint64_t *p      = addr;
    uint64_t        result = 0;
    uint64_t        tmp;

    while (size & ~(BITS_PER_LONG - 1)) {
        if ((tmp = *(p++))) {
            goto found;
        }

        result += BITS_PER_LONG;
        size -= BITS_PER_LONG;
    }

    if (!size) {
        return result;
    }

    tmp = (*p) & (~0UL >> (BITS_PER_LONG - size));

    if (tmp == 0UL) {         /* Are any bits set? */
        return result + size; /* Nope. */
    }

found:
    return result + __ffs(tmp);
}

/*
 * Find the first cleared bit in a memory region.
 */
uint64_t find_first_zero_bit(const uint64_t *addr, uint64_t size)
{
    const uint64_t *p      = addr;
    uint64_t        result = 0;
    uint64_t        tmp;

    while (size & ~(BITS_PER_LONG - 1)) {
        if (~(tmp = *(p++))) {
            goto found;
        }

        result += BITS_PER_LONG;
        size -= BITS_PER_LONG;
    }

    if (!size) {
        return result;
    }

    tmp = (*p) | (~0UL << size);

    if (tmp == ~0UL) {        /* Are any bits zero? */
        return result + size; /* Nope. */
    }

found:
    return result + ffz(tmp);
}

uint64_t find_last_bit(const uint64_t *addr, uint64_t size)
{
    uint64_t words;
    uint64_t tmp;

    /* Start at final word. */
    words = size / BITS_PER_LONG;

    /* Partial final word? */
    if (size & (BITS_PER_LONG - 1)) {
        tmp = (addr[words] & (~0UL >> (BITS_PER_LONG - (size & (BITS_PER_LONG - 1)))));

        if (tmp) {
            goto found;
        }
    }

    while (words) {
        tmp = addr[--words];

        if (tmp) {
        found:
            return words * BITS_PER_LONG + __fls(tmp);
        }
    }

    /* Not found */
    return size;
}
