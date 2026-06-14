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
 * @file rtc-lib.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Real-Time Clock Library source
 *
 * This source has been adapted from Linux 3.xx.xx drivers/rtc/rtc-lib.c
 *
 * rtc and date/time utility functions
 *
 * Copyright (C) 2005-06 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * The original code is licensed under the GPL.
 */

#include <drv/rtc.h>
#include <libs/mathlib.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_wall_clock.h>

static const unsigned char rtc_days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static const unsigned short rtc_ydays[2][13]   = {
    /* Normal years */
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
    /* Leap years */
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}
};

#define LEAPS_THRU_END_OF(y) ((y) / 4 - (y) / 100 + (y) / 400)

bool rtc_is_leap_year(uint32_t year)
{
    return (!(year % 4) && (year % 100)) || !(year % 400);
}

VMM_ERR_XPORT_SYMBOL(rtc_is_leap_year);

uint32_t rtc_month_days(uint32_t month, uint32_t year)
{
    return rtc_days_in_month[month] + (rtc_is_leap_year(year) && month == 1);
}

VMM_ERR_XPORT_SYMBOL(rtc_month_days);

uint32_t rtc_year_days(uint32_t day, uint32_t month, uint32_t year)
{
    return rtc_ydays[rtc_is_leap_year(year)][month] + day - 1;
}

VMM_ERR_XPORT_SYMBOL(rtc_year_days);

bool rtc_valid_tm(struct rtc_time *tm)
{
    if (tm->tm_year < 70 || ((unsigned)tm->tm_month) >= 12 || tm->tm_month_of_day < 1 ||
        tm->tm_month_of_day > rtc_month_days(tm->tm_month, tm->tm_year + 1900) || ((unsigned)tm->tm_hour) >= 24 || ((unsigned)tm->tm_minute) >= 60 ||
        ((unsigned)tm->tm_second) >= 60) {
        return FALSE;
    }

    return TRUE;
}

VMM_ERR_XPORT_SYMBOL(rtc_valid_tm);

/* WARNING: this function will overflow on 2106-02-07 06:28:16 on
 * machines where long is 32-bit!
 */
int rtc_tm_to_time(struct rtc_time *tm, uint64_t *time)
{
    if (!tm || !time) {
        return VMM_ERR_FAIL;
    }

    *time = (uint64_t)vmm_wall_clock_mktime(tm->tm_year + 1900, tm->tm_month + 1, tm->tm_month_of_day, tm->tm_hour, tm->tm_minute, tm->tm_second);

    return VMM_OK;
}

VMM_ERR_XPORT_SYMBOL(rtc_tm_to_time);

time64_t rtc_tm_to_time64(struct rtc_time *tm)
{
    if (!tm) {
        return VMM_ERR_INVALID;
    }

    return (time64_t)vmm_wall_clock_mktime(tm->tm_year + 1900, tm->tm_month + 1, tm->tm_month_of_day, tm->tm_hour, tm->tm_minute, tm->tm_second);
}

VMM_ERR_XPORT_SYMBOL(rtc_tm_to_time64);

static void __rtc_time_to_tm(uint64_t time, struct rtc_time *tm)
{
    uint32_t month, year;
    int      days;

    days = udiv64(time, 86400);
    time -= (uint32_t)days * 86400;

    /* day of the week, 1970-01-01 was a Thursday */
    tm->tm_week_of_day = (days + 4) % 7;

    year               = 1970 + days / 365;
    days -= (year - 1970) * 365 + LEAPS_THRU_END_OF(year - 1) - LEAPS_THRU_END_OF(1970 - 1);

    if (days < 0) {
        year -= 1;
        days += 365 + rtc_is_leap_year(year);
    }

    tm->tm_year        = year - 1900;
    tm->tm_year_of_day = days + 1;

    for (month = 0; month < 11; month++) {
        int newdays;

        newdays = days - rtc_month_days(month, year);

        if (newdays < 0) {
            break;
        }

        days = newdays;
    }

    tm->tm_month        = month;
    tm->tm_month_of_day = days + 1;

    tm->tm_hour         = udiv64(time, 3600);
    time -= tm->tm_hour * 3600;
    tm->tm_minute = udiv64(time, 60);
    tm->tm_second = time - tm->tm_minute * 60;
}

void rtc_time_to_tm(uint64_t time, struct rtc_time *tm)
{
    __rtc_time_to_tm(time, tm);
}

VMM_ERR_XPORT_SYMBOL(rtc_time_to_tm);

void rtc_time64_to_tm(time64_t time, struct rtc_time *tm)
{
    __rtc_time_to_tm(time, tm);
}

VMM_ERR_XPORT_SYMBOL(rtc_time64_to_tm);
