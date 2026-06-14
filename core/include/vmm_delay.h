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
 * @file vmm_delay.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 软延迟子系统头文件
 */
#ifndef _VMM_DELAY_H__
#define _VMM_DELAY_H__

#include <vmm_types.h>

/**
 * @brief usleep
 * @param usecs 微秒值
 */
void vmm_usleep(uint64_t usecs);

/**
 * @brief msleep
 * @param msecs 毫秒值
 */
void vmm_msleep(uint64_t msecs);

/**
 * @brief ssleep
 * @param secs 秒数
 */
void vmm_ssleep(uint64_t secs);

/**
 * @brief ndelay
 * @param nsecs 时间值（纳秒）
 */
void vmm_ndelay(uint64_t nsecs);

/**
 * @brief udelay
 * @param usecs 微秒值
 */
void vmm_udelay(uint64_t usecs);

/**
 * @brief mdelay
 * @param msecs 毫秒值
 */
void vmm_mdelay(uint64_t msecs);

/**
 * @brief sdelay
 * @param secs 秒数
 */
void vmm_sdelay(uint64_t secs);

/**
 * @brief 估算CPU主频（MHz），用于校准延迟循环
 * @param cpu CPU编号
 * @return 返回64位无符号整数值
 */
uint64_t vmm_delay_estimate_cpu_mhz(uint32_t cpu);

/**
 * @brief 估算CPU主频（KHz），用于校准延迟循环
 * @param cpu CPU编号
 * @return 返回64位无符号整数值
 */
uint64_t vmm_delay_estimate_cpu_khz(uint32_t cpu);

/**
 * @brief 重新校准延迟循环参数
 */
void vmm_delay_recaliberate(void);

/**
 * @brief 初始化延迟
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_delay_init(void);

#endif
