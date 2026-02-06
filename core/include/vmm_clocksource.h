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
 * @file vmm_clocksource.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for state free clocksource
 */
#ifndef _VMM_CLOCKSOURCE_H__
#define _VMM_CLOCKSOURCE_H__

#include <libs/list.h>
#include <libs/mathlib.h>
#include <vmm_device_tree.h>
#include <vmm_types.h>

struct vmm_clocksource;
typedef struct vmm_clocksource vmm_clocksource_t;

/**
 * Hardware abstraction a timer subsystem clocksource
 * Provides mostly state-free accessors to the underlying hardware.
 * This is the structure used for tracking passsing time.
 *
 * @name:       ptr to clocksource name
 * @list:       list head for registration
 * @rating:     rating value for selection (higher is better)
 *          To avoid rating inflation the following
 *          list should give you a guide as to how
 *          to assign your clocksource a rating
 *          1-99: Unfit for real use
 *              Only available for bootup and testing purposes.
 *          100-199: Base level usability.
 *              Functional for real use, but not desired.
 *          200-299: Good.
 *              A correct and usable clocksource.
 *          300-399: Desired.
 *              A reasonably fast and accurate clocksource.
 *          400-499: Perfect
 *              The ideal clocksource. A must-use where
 *              available.
 * @read:       returns a cycle value, passes clocksource as argument
 * @enable:     optional function to enable the clocksource
 * @disable:        optional function to disable the clocksource
 * @mask:       bitmask for two's complement
 *          subtraction of non 64 bit counters
 * @freq:       frequency at which counter is running
 * @mult:       cycle to nanosecond multiplier
 * @shift:      cycle to nanosecond divisor (power of two)
 * @suspend:        suspend function for the clocksource, if necessary
 * @resume:     resume function for the clocksource, if necessary
 */
struct vmm_clocksource {
    double_list_t head;
    const char   *name;
    int           rating;
    uint64_t      mask;
    uint32_t      freq;
    uint32_t      mult;
    uint32_t      shift;
    uint64_t (*read)(vmm_clocksource_t *cs);
    int (*enable)(vmm_clocksource_t *cs);
    void (*disable)(vmm_clocksource_t *cs);
    void (*clocksource)(vmm_clocksource_t *cs);
    void (*resume)(vmm_clocksource_t *cs);
    void *private;
};

/* simplify initialization of mask field */
#define VMM_CLOCKSOURCE_MASK(bits) (uint64_t)((bits) < 64 ? ((1ULL << (bits)) - 1) : -1)

/* nodeid table based clocksource initialization callback */
typedef int (*vmm_clocksource_init_t)(vmm_device_tree_node_t *);

/* declare nodeid table based initialization for clocksource */
#define VMM_CLOCKSOURCE_INIT_DECLARE(name, compat, fn) VMM_DEVICE_TREE_NIDTBL_ENTRY(name, "clocksource", "", "", compat, fn)

/**
 * Layer above a %vmm_clocksource_t which counts nanoseconds
 * Contains the state needed by vmm_timecounter_read() to detect
 * clocksource wrap around. Initialize with vmm_timecounter_init().
 * Users of this code are responsible for initializing the underlying
 * clocksource hardware, locking issues and reading the time more often
 * than the clocksource wraps around. The nanosecond counter will only
 * wrap around after ~585 years.
 *
 * @cs:         the cycle counter used by this instance
 * @cycles_last:    most recent cycle counter value seen by
 *          vmm_timecounter_read()
 * @nsec:       continuously increasing count
 */
struct vmm_timecounter {
    vmm_clocksource_t *cs;
    uint64_t           cycles_last;
    uint64_t           nsec;
};

typedef struct vmm_timecounter vmm_timecounter_t;

/** Convert kHz clocksource to clocksource mult */
static inline uint32_t vmm_clocksource_khz2mult(uint32_t khz, uint32_t shift)
{
    uint64_t tmp = ((uint64_t)1000000) << shift;
    tmp += khz >> 1;
    tmp = udiv64(tmp, khz);
    return (uint32_t)tmp;
}

/** Convert Hz clocksource to clocksource mult */
static inline uint32_t vmm_clocksource_hz2mult(uint32_t hz, uint32_t shift)
{
    uint64_t tmp = ((uint64_t)1000000000) << shift;
    tmp += hz >> 1;
    tmp = udiv64(tmp, hz);
    return (uint32_t)tmp;
}

/** Convert delta cycles to nsecs */
#define vmm_clocksource_delta2nsecs(cycles, mult, shift) (((cycles) * (mult)) >> (shift))

/** Get clocksource frequency of nanosecond counter */
uint32_t vmm_timecounter_clocksource_frequency(vmm_timecounter_t *tc);

/** Get current value from nanosecond counter (nanoseconds elapsed) */
uint64_t vmm_timecounter_read(vmm_timecounter_t *tc);

#if defined(CONFIG_PROFILE)
/** Special version for profile */
uint64_t vmm_timecounter_read_for_profile(vmm_timecounter_t *tc);
#endif

/** Start nanosecond counter (nanoseconds elapsed) */
int vmm_timecounter_start(vmm_timecounter_t *tc);

/** Stop nanosecond counter (nanoseconds elapsed) */
int vmm_timecounter_stop(vmm_timecounter_t *tc);

/** Initialize nanosecond counter */
int vmm_timecounter_init(vmm_timecounter_t *tc, vmm_clocksource_t *cs, uint64_t start_nsec);

/** Register clocksource */
int vmm_clocksource_register(vmm_clocksource_t *cs);

/** Register clocksource */
int vmm_clocksource_unregister(vmm_clocksource_t *cs);

/** Get best rated clocksource */
vmm_clocksource_t *vmm_clocksource_best(void);

/** Find a clocksource */
vmm_clocksource_t *vmm_clocksource_find(const char *name);

/** Retrive clocksource with given index */
vmm_clocksource_t *vmm_clocksource_get(int index);

/** Count number of clocksources */
uint32_t vmm_clocksource_count(void);

/** Initialize clocksource management subsystem */
int vmm_clocksource_init(void);

#endif
