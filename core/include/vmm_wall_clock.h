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
 * @file vmm_wall_clock.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for wall-clock subsystem
 */
#ifndef _VMM_WALLCLOCK_H__
#define _VMM_WALLCLOCK_H__

#include <vmm_limits.h>
#include <vmm_types.h>

typedef struct vmm_time_value {
    time64_t tv_sec;  /* seconds */
    time64_t tv_nsec; /* nanoseconds */
} vmm_time_value_t;

typedef struct vmm_timezone {
    int tz_minutes_greenwich; /* minutes west of Greenwich */
    int tz_dsttime;           /* type of dst correction */
} vmm_timezone_t;

typedef struct vmm_time_info {
    /*
     * the number of seconds after the minute, normally in the range
     * 0 to 59, but can be up to 60 to allow for leap seconds
     */
    uint32_t tm_second;
    /* the number of minutes after the hour, in the range 0 to 59*/
    uint32_t tm_minute;
    /* the number of hours past midnight, in the range 0 to 23 */
    uint32_t tm_hour;
    /* the day of the month, in the range 1 to 31 */
    uint32_t tm_month_of_day;
    /* the number of months since January, in the range 0 to 11 */
    uint32_t tm_month;
    /* the number of years since 1900 */
    uint32_t tm_year;
    /* the number of days since Sunday, in the range 0 to 6 */
    uint32_t tm_week_of_day;
    /* the number of days since January 1, in the range 0 to 365 */
    uint32_t tm_year_of_day;
} vmm_time_info_t;

/** Parameters used to convert the timeval values: */
#define VMM_TIMEVAL_SEC_MAX  ((1ULL << ((sizeof(time64_t) << 3) - 1)) - 1)
#define VMM_TIMEVAL_NSEC_MAX NSEC_PER_SEC

/** Compare two vmm_time_value instances */
static inline int vmm_time_value_compare(const vmm_time_value_t *lhs, const vmm_time_value_t *rhs)
{
    if (lhs->tv_sec < rhs->tv_sec) {
        return -1;
    }

    if (lhs->tv_sec > rhs->tv_sec) {
        return 1;
    }

    return lhs->tv_nsec - rhs->tv_nsec;
}

/** Check if the vmm_time_value is normalized or denormalized */
#define vmm_time_value_valid(tv) (((tv)->tv_sec >= 0) && (((uint64_t)(tv)->tv_nsec) < NSEC_PER_SEC))

/** Set normalized values to vmm_time_value instance */
void vmm_time_value_set_normalized(vmm_time_value_t *tv, time64_t sec, time64_t nsec);

/** Add vmm_time_value intances. add = lhs - rhs, in normalized form */
vmm_time_value_t vmm_time_value_add(vmm_time_value_t lhs, vmm_time_value_t rhs);

/** Substract vmm_time_value intances. sub = lhs - rhs, in normalized form */
vmm_time_value_t vmm_time_value_sub(vmm_time_value_t lhs, vmm_time_value_t rhs);

/** Convert vmm_time_value to nanoseconds */
static inline time64_t vmm_time_value_to_ns(const vmm_time_value_t *tv)
{
    return ((time64_t)tv->tv_sec * NSEC_PER_SEC) + tv->tv_nsec;
}

/** Convert nanoseconds to vmm_time_value */
vmm_time_value_t vmm_ns_to_timeval(const time64_t nsec);

/** Convert seconds elapsed Gregorian date to vmm_time_info
 *  @totalsecs  number of seconds elapsed since 00:00:00 on January 1, 1970,
 *      Coordinated Universal Time (UTC).
 *  @offset offset seconds adding to totalsecs.
 *  @result pointer to vmm_time_info_t variable for broken-down time
 */
void vmm_wall_clock_mkinfo(time64_t totalsecs, int offset, vmm_time_info_t *result);

/** Converts Gregorian date to seconds since 1970-01-01 00:00:00.
 *  Assumes input in normal date format, i.e. 1980-12-31 23:59:59
 *  => year=1980, mon=12, day=31, hour=23, min=59, sec=59.
 */
time64_t vmm_wall_clock_mktime(
    const uint32_t year0, const uint32_t mon0, const uint32_t day, const uint32_t hour, const uint32_t min, const uint32_t sec);

/** Set local time */
int vmm_wall_clock_set_local_time(vmm_time_value_t *tv);

/** Get local time */
int vmm_wall_clock_get_local_time(vmm_time_value_t *tv);

/** Set current timezone */
int vmm_wall_clock_set_timezone(vmm_timezone_t *tz);

/** Get current timezone */
int vmm_wall_clock_get_timezone(vmm_timezone_t *tz);

/** Set current time and timezone */
int vmm_wall_clock_set_timeofday(vmm_time_value_t *tv, vmm_timezone_t *tz);

/** Get current time and timezone */
int vmm_wall_clock_get_timeofday(vmm_time_value_t *tv, vmm_timezone_t *tz);

/** Initialize wall-clock subsystem */
int vmm_wall_clock_init(void);

#endif
