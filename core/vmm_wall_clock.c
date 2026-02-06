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
 * @file vmm_wall_clock.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for wall-clock subsystem
 *
 *  This source has been adapted from linux/kernel/time.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  This file contains the interface functions for the various
 *  time related system calls: time, stime, gettimeofday, settimeofday,
 *                 adjtime
 *
 *  The original code is licensed under the GPL.
 */

#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_spinlocks.h>
#include <vmm_timer.h>
#include <vmm_wall_clock.h>

struct vmm_wall_clock_ctrl {
    vmm_spinlock_t   lock;
    vmm_time_value_t tv;
    vmm_timezone_t   tz;
    uint64_t         last_modify_tstamp;
};

static struct vmm_wall_clock_ctrl wclk;

void vmm_time_value_set_normalized(vmm_time_value_t *tv, time64_t sec, time64_t nsec)
{
    while (nsec >= NSEC_PER_SEC) {
        /*
         * The following asm() prevents the compiler from
         * optimising this loop into a modulo operation. See
         * also __iter_div_u64_rem() in include/linux/time.h
         */
        asm("" : "+rm"(nsec));
        nsec -= NSEC_PER_SEC;
        ++sec;
    }

    while (nsec < 0) {
        asm("" : "+rm"(nsec));
        nsec += NSEC_PER_SEC;
        --sec;
    }

    tv->tv_sec  = sec;
    tv->tv_nsec = nsec;
}

vmm_time_value_t vmm_time_value_add(vmm_time_value_t lhs, vmm_time_value_t rhs)
{
    vmm_time_value_t tv_delta;
    vmm_time_value_set_normalized(&tv_delta, lhs.tv_sec + rhs.tv_sec, lhs.tv_nsec + rhs.tv_nsec);

    if (tv_delta.tv_sec < lhs.tv_sec || tv_delta.tv_sec < rhs.tv_sec) {
        tv_delta.tv_sec = VMM_TIMEVAL_SEC_MAX;
    }

    return tv_delta;
}

vmm_time_value_t vmm_time_value_sub(vmm_time_value_t lhs, vmm_time_value_t rhs)
{
    vmm_time_value_t tv_delta;
    vmm_time_value_set_normalized(&tv_delta, lhs.tv_sec - rhs.tv_sec, lhs.tv_nsec - rhs.tv_nsec);

    if (tv_delta.tv_sec < lhs.tv_sec || tv_delta.tv_sec < rhs.tv_sec) {
        tv_delta.tv_sec = VMM_TIMEVAL_SEC_MAX;
    }

    return tv_delta;
}

vmm_time_value_t vmm_ns_to_timeval(const time64_t nsec)
{
    vmm_time_value_t tv;

    if (!nsec) {
        tv.tv_sec  = 0;
        tv.tv_nsec = 0;
        return tv;
    }

    tv.tv_sec  = sdiv64(nsec, NSEC_PER_SEC);
    tv.tv_nsec = nsec - tv.tv_sec * NSEC_PER_SEC;

    if (tv.tv_nsec < 0) {
        tv.tv_sec--;
        tv.tv_nsec += NSEC_PER_SEC;
    }

    return tv;
}

/*
 * Nonzero if YEAR is a leap year (every 4 years,
 * except every 100th isn't, and every 400th is).
 */
static int __isleap(long year)
{
    return (year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0);
}

/* do a mathdiv for long type */
static long math_div(long a, long b)
{
    return sdiv64(a, b) - (smod64(a, b) < 0);
}

/* How many leap years between y1 and y2, y1 must less or equal to y2 */
static long leaps_between(long y1, long y2)
{
    long leaps1 = math_div(y1 - 1, 4) - math_div(y1 - 1, 100) + math_div(y1 - 1, 400);
    long leaps2 = math_div(y2 - 1, 4) - math_div(y2 - 1, 100) + math_div(y2 - 1, 400);
    return leaps2 - leaps1;
}

/* How many days come before each month (0-12). */
static const unsigned short __mon_yday[2][13] = {
    /* Normal years. */
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
    /* Leap years. */
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}
};

#define SECS_PER_HOUR (60 * 60)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24)

void vmm_wall_clock_mkinfo(time64_t totalsecs, int offset, vmm_time_info_t *result)
{
    long                  days, rem, y;
    const unsigned short *ip;

    days = sdiv64(totalsecs, SECS_PER_DAY);
    rem  = totalsecs - days * SECS_PER_DAY;
    rem += offset;

    while (rem < 0) {
        rem += SECS_PER_DAY;
        --days;
    }

    while (rem >= SECS_PER_DAY) {
        rem -= SECS_PER_DAY;
        ++days;
    }

    result->tm_hour = rem / SECS_PER_HOUR;
    rem %= SECS_PER_HOUR;
    result->tm_minute      = rem / 60;
    result->tm_second      = rem % 60;

    /* January 1, 1970 was a Thursday. */
    result->tm_week_of_day = (4 + days) % 7;

    if (result->tm_week_of_day < 0) {
        result->tm_week_of_day += 7;
    }

    y = 1970;

    while (days < 0 || days >= (__isleap(y) ? 366 : 365)) {
        /* Guess a corrected year, assuming 365 days per year. */
        long yg = y + math_div(days, 365);

        /* Adjust DAYS and Y to match the guessed year. */
        days -= (yg - y) * 365 + leaps_between(y, yg);
        y = yg;
    }

    result->tm_year        = y - 1900;

    result->tm_year_of_day = days;

    ip                     = __mon_yday[__isleap(y)];

    for (y = 11; days < ip[y]; y--) {
        continue;
    }

    days -= ip[y];

    result->tm_month        = y;
    result->tm_month_of_day = days + 1;
}

/* [For the Julian calendar (which was used in Russia before 1917,
 * Britain & colonies before 1752, anywhere else before 1582,
 * and is still in use by some communities) leave out the
 * -year/100+year/400 terms, and add 10.]
 *
 * This algorithm was first published by Gauss (I think).
 *
 * NOTE: the original function mktime() in linux will overflow on
 * 2106-02-07 06:28:16 on machines where long is 32-bit! To take
 * care of this issue we return time64_t try to avoid overflow.
 */
time64_t vmm_wall_clock_mktime(
    const uint32_t year0, const uint32_t mon0, const uint32_t day, const uint32_t hour, const uint32_t min, const uint32_t sec)
{
    uint32_t year = year0, mon = mon0;
    uint64_t ret;

    /* 1..12 -> 11,12,1..10 */
    if (0 >= (int)(mon -= 2)) {
        mon += 12; /* Puts Feb last since it has leap day */
        year -= 1;
    }

    /* no. of days */
    ret = (uint64_t)(year / 4 - year / 100 + year / 400 + 367 * mon / 12 + day);
    ret += (uint64_t)(year) * 365 - 719499;

    /* no. of hours */
    ret *= (uint64_t)24;
    ret += hour;

    /* no. of mins */
    ret *= (uint64_t)60;
    ret += min;

    /* no. of secs */
    ret *= (uint64_t)60;
    ret += sec;

    return (time64_t)ret;
}

int vmm_wall_clock_set_local_time(vmm_time_value_t *tv)
{
    irq_flags_t flags;

    if (!tv) {
        return VMM_EFAIL;
    }

    vmm_spin_lock_irq_save(&wclk.lock, flags);

    wclk.tv.tv_sec          = tv->tv_sec;
    wclk.tv.tv_nsec         = tv->tv_nsec;
    wclk.last_modify_tstamp = vmm_timer_timestamp();

    vmm_spin_unlock_irq_restore(&wclk.lock, flags);

    return VMM_OK;
}

int vmm_wall_clock_get_local_time(vmm_time_value_t *tv)
{
    irq_flags_t flags;
    uint64_t    tdiff, tdiv, tmod;

    if (!tv) {
        return VMM_EFAIL;
    }

    vmm_spin_lock_irq_save(&wclk.lock, flags);

    tv->tv_sec  = wclk.tv.tv_sec;
    tv->tv_nsec = wclk.tv.tv_nsec;
    tdiff       = vmm_timer_timestamp() - wclk.last_modify_tstamp;

    vmm_spin_unlock_irq_restore(&wclk.lock, flags);

    tdiv = udiv64(tdiff, NSEC_PER_SEC);
    tmod = tdiff - tdiv * NSEC_PER_SEC;
    tv->tv_nsec += tmod;

    while (NSEC_PER_SEC <= tv->tv_nsec) {
        tv->tv_sec++;
        tv->tv_nsec -= NSEC_PER_SEC;
    }

    tv->tv_sec += tdiv;

    return VMM_OK;
}

int vmm_wall_clock_set_timezone(vmm_timezone_t *tz)
{
    int         minuteswest;
    irq_flags_t flags;

    if (!tz) {
        return VMM_EFAIL;
    }

    vmm_spin_lock_irq_save(&wclk.lock, flags);

    minuteswest = tz->tz_minutes_greenwich - wclk.tz.tz_minutes_greenwich;
    wclk.tv.tv_sec += minuteswest * 60;
    wclk.tz.tz_minutes_greenwich = tz->tz_minutes_greenwich;
    wclk.tz.tz_dsttime           = tz->tz_dsttime;

    vmm_spin_unlock_irq_restore(&wclk.lock, flags);

    return VMM_OK;
}

int vmm_wall_clock_get_timezone(vmm_timezone_t *tz)
{
    irq_flags_t flags;

    if (!tz) {
        return VMM_EFAIL;
    }

    vmm_spin_lock_irq_save(&wclk.lock, flags);

    tz->tz_minutes_greenwich = wclk.tz.tz_minutes_greenwich;
    tz->tz_dsttime           = wclk.tz.tz_dsttime;

    vmm_spin_unlock_irq_restore(&wclk.lock, flags);

    return VMM_OK;
}

int vmm_wall_clock_set_timeofday(vmm_time_value_t *tv, vmm_timezone_t *tz)
{
    int rc;

    if (tz) {
        if ((rc = vmm_wall_clock_set_timezone(tz))) {
            return rc;
        }
    }

    if (tv) {
        if ((rc = vmm_wall_clock_set_local_time(tv))) {
            return rc;
        }
    }

    return VMM_OK;
}

int vmm_wall_clock_get_timeofday(vmm_time_value_t *tv, vmm_timezone_t *tz)
{
    int rc;

    if (tz) {
        if ((rc = vmm_wall_clock_get_timezone(tz))) {
            return rc;
        }
    }

    if (tv) {
        if ((rc = vmm_wall_clock_get_local_time(tv))) {
            return rc;
        }
    }

    return VMM_OK;
}

int vmm_wall_clock_init(void)
{
    memset(&wclk, 0, sizeof(wclk));

    INIT_SPIN_LOCK(&wclk.lock);

    wclk.tv.tv_sec               = 0;
    wclk.tv.tv_nsec              = 0;

    wclk.tz.tz_minutes_greenwich = 0;
    wclk.tz.tz_dsttime           = 0;

    wclk.last_modify_tstamp      = vmm_timer_timestamp();

    return VMM_OK;
}
