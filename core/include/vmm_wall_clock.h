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
 * @brief 壁钟子系统头文件
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

/**
 * @brief 时间信息结构，保存秒数、纳秒数和频率等时间相关数据
 */
typedef struct vmm_time_info {
    /*
     * the number of seconds after the minute, normally in the range
     * 0 to 59, but can be up to 60 to allow for leap seconds
     */
    uint32_t tm_second; /**< tm_second成员 */
    /* the number of minutes after the hour, in the range 0 to 59*/
    uint32_t tm_minute; /**< tm_minute成员 */
    /* the number of hours past midnight, in the range 0 to 23 */
    uint32_t tm_hour; /**< tm_hour成员 */
    /* the day of the month, in the range 1 to 31 */
    uint32_t tm_month_of_day; /**< tm_month_of_day成员 */
    /* the number of months since January, in the range 0 to 11 */
    uint32_t tm_month; /**< tm_month成员 */
    /* the number of years since 1900 */
    uint32_t tm_year; /**< tm_year成员 */
    /* the number of days since Sunday, in the range 0 to 6 */
    uint32_t tm_week_of_day; /**< tm_week_of_day成员 */
    /* the number of days since January 1, in the range 0 to 365 */
    uint32_t tm_year_of_day; /**< tm_year_of_day成员 */
} vmm_time_info_t;

/** Parameters used to convert the timeval values: */
#define VMM_TIMEVAL_SEC_MAX  ((1ULL << ((sizeof(time64_t) << 3) - 1)) - 1)
#define VMM_TIMEVAL_NSEC_MAX NSEC_PER_SEC

/**
 * @brief 比较两个时间值实例
 */
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

/**
 * @brief 设置时间值的归一化值
 * @param tv 时间值结构体指针
 * @param sec 秒数
 * @param nsec 纳秒值
 */
void vmm_time_value_set_normalized(vmm_time_value_t *tv, time64_t sec, time64_t nsec);

/**
 * @brief 将两个时间值相加
 * @param lhs 左侧操作数
 * @param rhs 右侧操作数
 * @return 时间值（纳秒）
 */
vmm_time_value_t vmm_time_value_add(vmm_time_value_t lhs, vmm_time_value_t rhs);

/**
 * @brief 将两个时间值相减
 * @param lhs 左侧操作数
 * @param rhs 右侧操作数
 * @return 时间值（纳秒）
 */
vmm_time_value_t vmm_time_value_sub(vmm_time_value_t lhs, vmm_time_value_t rhs);

/**
 * @brief 将时间值转换为纳秒
 */
static inline time64_t vmm_time_value_to_ns(const vmm_time_value_t *tv)
{
    return ((time64_t)tv->tv_sec * NSEC_PER_SEC) + tv->tv_nsec;
}

/**
 * @brief 将纳秒转换为timeval结构体
 * @param nsec 纳秒值
 * @return 时间值（纳秒）
 */
vmm_time_value_t vmm_ns_to_timeval(const time64_t nsec);

/**
 * @brief 根据墙上时钟时间生成日历信息
 * @param totalsecs 总秒数
 * @param offset 偏移量（字节）
 * @param result 结果值指针
 */
void vmm_wall_clock_mkinfo(time64_t totalsecs, int offset, vmm_time_info_t *result);

/**
 * @brief 将公历日期转换为自1970年1月1日起的秒数
 */
time64_t vmm_wall_clock_mktime(
    const uint32_t year0, const uint32_t mon0, const uint32_t day, const uint32_t hour, const uint32_t min, const uint32_t sec);

/**
 * @brief 设置壁钟的本地时间
 * @param tv 时间值结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_wall_clock_set_local_time(vmm_time_value_t *tv);

/**
 * @brief 获取壁钟的本地时间
 * @param tv 时间值结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_wall_clock_get_local_time(vmm_time_value_t *tv);

/**
 * @brief 设置壁钟的时区
 * @param tz 时区信息指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_wall_clock_set_timezone(vmm_timezone_t *tz);

/**
 * @brief 获取壁钟的时区
 * @param tz 时区信息指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_wall_clock_get_timezone(vmm_timezone_t *tz);

/**
 * @brief 设置壁钟的日历时间
 * @param tv 时间值结构体指针
 * @param tz 时区信息指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_wall_clock_set_timeofday(vmm_time_value_t *tv, vmm_timezone_t *tz);

/**
 * @brief 获取壁钟的日历时间
 * @param tv 时间值结构体指针
 * @param tz 时区信息指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_wall_clock_get_timeofday(vmm_time_value_t *tv, vmm_timezone_t *tz);

/**
 * @brief 初始化壁钟
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_wall_clock_init(void);

#endif
