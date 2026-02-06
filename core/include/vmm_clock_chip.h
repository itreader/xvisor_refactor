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
 * @file vmm_clock_chip.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface for clockchip management
 */
#ifndef _VMM_CLOCKCHIP_H__
#define _VMM_CLOCKCHIP_H__

#include <libs/list.h>
#include <libs/mathlib.h>
#include <vmm_cpumask.h>
#include <vmm_device_tree.h>
#include <vmm_types.h>

/* Clock chip mode commands */
/* 时钟芯片模式 */
enum vmm_clock_chip_mode {
    VMM_CLOCKCHIP_MODE_UNUSED = 0,
    VMM_CLOCKCHIP_MODE_SHUTDOWN,
    VMM_CLOCKCHIP_MODE_PERIODIC,
    VMM_CLOCKCHIP_MODE_ONESHOT,
    VMM_CLOCKCHIP_MODE_RESUME,
};

typedef enum vmm_clock_chip_mode vmm_clock_chip_mode_e;

/* Clockchip features */
#define VMM_CLOCKCHIP_FEAT_PERIODIC 0x000001
#define VMM_CLOCKCHIP_FEAT_ONESHOT  0x000002

struct vmm_clock_chip;
typedef struct vmm_clock_chip vmm_clock_chip_t;

/**
 * Hardware abstraction a clock chip device
 *
 * @head:       List head for registration
 * @name:       ptr to clockchip name
 * @hirq:       host irq number
 * @rating:     variable to rate clock event devices
 * @cpumask:        cpumask to indicate for which CPUs this device works
 * @features:       features
 * @freq:       frequency at which clock event device is running
 * @mult:       nanosecond to cycles multiplier
 * @shift:      nanoseconds to cycles divisor (power of two)
 * @max_delta_ns:   maximum delta value in ns
 * @min_delta_ns:   minimum delta value in ns
 * @event_handler:  Assigned by the framework to be called by the low
 *          level handler of the event source
 * @set_mode:       set mode function
 * @set_next_event: set next event function
 * @mode:       operating mode assigned by the management code
 * @bound_on:       Bound on host CPU
 * @next_event:     local storage for the next event in oneshot mode
 */
struct vmm_clock_chip {
    double_list_t        head;
    const char          *name;
    uint32_t             hirq;
    int                  rating;
    const vmm_cpumask_t *cpumask;
    uint32_t             features;
    uint32_t             freq;
    uint32_t             mult;
    uint32_t             shift;
    uint64_t             max_delta_ns;
    uint64_t             min_delta_ns;
    void (*event_handler)(vmm_clock_chip_t *cc);
    void (*set_mode)(vmm_clock_chip_mode_e mode, vmm_clock_chip_t *cc);
    int (*set_next_event)(uint64_t evt, vmm_clock_chip_t *cc);
    vmm_clock_chip_mode_e mode;
    uint32_t              bound_on;
    uint64_t              next_event;
    void *private;
};

#define VMM_NSEC_PER_SEC 1000000000UL

/* nodeid table based clockchip initialization callback */
typedef int (*vmm_clock_chip_init_t)(vmm_device_tree_node_t *);

/* declare nodeid table based initialization for clocksource */
#define VMM_CLOCKCHIP_INIT_DECLARE(name, compat, fn) VMM_DEVICE_TREE_NIDTBL_ENTRY(name, "clockchip", "", "", compat, fn)

/**
 * clocks_calc_mult_shift - calculate mult/shift factors for scaled math of clocks
 * @mult:   pointer to mult variable
 * @shift:  pointer to shift variable
 * @from:   frequency to convert from
 * @to:     frequency to convert to
 * @maxsec: guaranteed runtime conversion range in seconds
 *
 * The function evaluates the shift/mult pair for the scaled math
 * operations of clocksources and clockevents.
 *
 * @to and @from are frequency values in HZ. For clock sources @to is
 * VMM_NSEC_PER_SEC == 1GHz and @from is the counter frequency. For clock
 * event @to is the counter frequency and @from is VMM_NSEC_PER_SEC.
 *
 * The @maxsec conversion range argument controls the time frame in
 * seconds which must be covered by the runtime conversion with the
 * calculated mult and shift factors. This guarantees that no 64bit
 * overflow happens when the input value of the conversion is
 * multiplied with the calculated mult factor. Larger ranges may
 * reduce the conversion accuracy by chosing smaller mult and shift
 * factors.
 */
static inline void vmm_clocks_calc_mult_shift(uint32_t *mult, uint32_t *shift, uint32_t from, uint32_t to, uint32_t maxsec)
{
    uint64_t tmp;
    uint32_t sft, sftacc = 32;

    /*
     * Calculate the shift factor which is limiting the conversion
     * range:
     */
    tmp = ((uint64_t)maxsec * from) >> 32;

    while (tmp) {
        tmp >>= 1;
        sftacc--;
    }

    /*
     * Find the conversion shift/mult pair which has the best
     * accuracy and fits the maxsec conversion range:
     */
    for (sft = 32; sft > 0; sft--) {
        tmp = (uint64_t)to << sft;
        tmp += from / 2;
        tmp = udiv64(tmp, from);

        if ((tmp >> sftacc) == 0) {
            break;
        }
    }

    *mult  = tmp;
    *shift = sft;
}

/** Convert kHz clockchip to clockchip mult */
static inline uint32_t vmm_clock_chip_khz2mult(uint32_t khz, uint32_t shift)
{
    uint64_t tmp = ((uint64_t)khz) << shift;
    tmp          = udiv64(tmp, (uint64_t)1000000);
    return (uint32_t)tmp;
}

/** Convert Hz clockchip to clockchip mult */
static inline uint32_t vmm_clock_chip_hz2mult(uint32_t hz, uint32_t shift)
{
    uint64_t tmp = ((uint64_t)hz) << shift;
    tmp          = udiv64(tmp, (uint64_t)1000000000);
    return (uint32_t)tmp;
}

/** Get frequency of clockchip */
static inline uint32_t vmm_clock_chip_frequency(vmm_clock_chip_t *cc)
{
    return (cc) ? cc->freq : 0;
}

/** Convert tick delta to nanoseconds */
static inline uint64_t vmm_clock_chip_delta2ns(uint64_t delta, vmm_clock_chip_t *cc)
{
    uint64_t tmp = (uint64_t)delta << cc->shift;
    return udiv64(tmp, cc->mult);
}

/** Set event handler for clockchip */
void vmm_clock_chip_set_event_handler(vmm_clock_chip_t *cc, void (*event_handler)(vmm_clock_chip_t *));

/** Program clockchip for next event after delta nanoseconds */
int vmm_clock_chip_program_event(vmm_clock_chip_t *cc, uint64_t now_ns, uint64_t expires_ns);

/** Change mode of clockchip */
void vmm_clock_chip_set_mode(vmm_clock_chip_t *cc, vmm_clock_chip_mode_e mode);

/** Register clockchip */
int vmm_clock_chip_register(vmm_clock_chip_t *cc);

/** Register clockchip */
int vmm_clock_chip_unregister(vmm_clock_chip_t *cc);

/** Find best rated clockchip for given host CPU and bind it */
vmm_clock_chip_t *vmm_clock_chip_bind_best(uint32_t host_cpu);

/** Unbind clockchip from host CPU */
int vmm_clock_chip_unbind(vmm_clock_chip_t *cc);

/** Retrive clockchip with given index */
vmm_clock_chip_t *vmm_clock_chip_get(int index);

/** Count number of clockchips */
uint32_t vmm_clock_chip_count(void);

/** Initialize clockchip management subsystem */
int vmm_clock_chip_init(void);

#endif
