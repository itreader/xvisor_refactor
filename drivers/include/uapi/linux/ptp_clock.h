/*
 * PTP 1588 clock support - user space interface
 *
 * Copyright (C) 2010 OMICRON electronics GmbH
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _PTP_CLOCK_H_
#define _PTP_CLOCK_H_

#include <vmm_types.h>

/* PTP_xxx bits, for the flags field within the request structures. */
#define PTP_ENABLE_FEATURE (1 << 0)
#define PTP_RISING_EDGE    (1 << 1)
#define PTP_FALLING_EDGE   (1 << 2)

/*
 * struct ptp_clock_time - represents a time value
 *
 * The sign of the seconds field applies to the whole value. The
 * nanoseconds field is always unsigned. The reserved field is
 * included for sub-nanosecond resolution, should the demand for
 * this ever appear.
 *
 */
struct ptp_clock_time {
    __s64    sec;  /* seconds */
    uint32_t nsec; /* nanoseconds */
    uint32_t reserved;
};

struct ptp_extts_request {
    uint32_t index;  /* Which channel to configure. */
    uint32_t flags;  /* Bit field for PTP_xxx flags. */
    uint32_t rsv[2]; /* Reserved for future use. */
};

struct ptp_perout_request {
    struct ptp_clock_time start;  /* Absolute start time. */
    struct ptp_clock_time period; /* Desired period, zero means disable. */
    uint32_t              index;  /* Which channel to configure. */
    uint32_t              flags;  /* Reserved for future use. */
    uint32_t              rsv[4]; /* Reserved for future use. */
};

enum ptp_pin_function {
    PTP_PF_NONE,
    PTP_PF_EXTTS,
    PTP_PF_PEROUT,
    PTP_PF_PHYSYNC,
};

#endif /* !_PTP_CLOCK_H_ */
