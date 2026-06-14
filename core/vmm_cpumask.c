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
 * @file vmm_cpumask.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief CPU集合管理接口的实现
 *
 * The source has been largely adapted from:
 * linux-xxx/kernel/cpu.c
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_cpumask.h>

/* Number of possible processor count
 * Note: SMP secondary core init will update this count
 */
int vmm_cpu_count = CONFIG_CPU_COUNT;

/*
 * cpu_bit_bitmap[] is a special, "compressed" data structure that
 * represents all CONFIG_CPU_COUNT bits binary values of 1<<nr.
 *
 * It is used by vmm_cpumask_of() to get a constant address to a CPU
 * mask value that has a single bit set only.
 */

/* cpu_bit_bitmap[0] is empty - so we can back into it */
#define MASK_DECLARE_1(x) [x + 1][0] = (1UL << (x))
#define MASK_DECLARE_2(x) MASK_DECLARE_1(x), MASK_DECLARE_1(x + 1)
#define MASK_DECLARE_4(x) MASK_DECLARE_2(x), MASK_DECLARE_2(x + 2)
#define MASK_DECLARE_8(x) MASK_DECLARE_4(x), MASK_DECLARE_4(x + 4)

const uint64_t cpu_bit_bitmap[BITS_PER_LONG + 1][BITS_TO_LONGS(CONFIG_CPU_COUNT)] = {

    MASK_DECLARE_8(0),  MASK_DECLARE_8(8),  MASK_DECLARE_8(16), MASK_DECLARE_8(24),
#if BITS_PER_LONG > 32
    MASK_DECLARE_8(32), MASK_DECLARE_8(40), MASK_DECLARE_8(48), MASK_DECLARE_8(56),
#endif
};

const DECLARE_BITMAP(cpu_all_bits, CONFIG_CPU_COUNT) = VMM_CPU_BITS_ALL;

static DECLARE_BITMAP(cpu_possible_bits, CONFIG_CPU_COUNT) __read_mostly;
const vmm_cpumask_t *const cpu_possible_mask = to_cpumask(cpu_possible_bits);

static DECLARE_BITMAP(cpu_online_bits, CONFIG_CPU_COUNT) __read_mostly;
const vmm_cpumask_t *const cpu_online_mask = to_cpumask(cpu_online_bits);

static DECLARE_BITMAP(cpu_present_bits, CONFIG_CPU_COUNT) __read_mostly;
const vmm_cpumask_t *const cpu_present_mask = to_cpumask(cpu_present_bits);

static DECLARE_BITMAP(cpu_active_bits, CONFIG_CPU_COUNT) __read_mostly;
const vmm_cpumask_t *const cpu_active_mask = to_cpumask(cpu_active_bits);

#if CONFIG_CPU_COUNT != 1

/**
 * @brief 获取两个CPU掩码交集中指定CPU之后的下一个CPU
 * @param n 起始位置编号
 * @param src1p CPU亲和性掩码
 * @param src2p CPU亲和性掩码
 * @return 下一个在两个掩码中都置位的CPU编号，无则返回nr_cpu_ids
 */
uint32_t vmm_cpumask_next_and(int n, const vmm_cpumask_t *src1p, const vmm_cpumask_t *src2p)
{
    while ((n = vmm_cpumask_next(n, src1p)) < vmm_cpu_count) {
        if (vmm_cpumask_test_cpu(n, src2p)) {
            break;
        }
    }

    return n;
}

/**
 * @brief 获取CPU掩码中除指定CPU外的任意一个CPU
 * @param mask CPU亲和性掩码
 * @param cpu CPU编号
 * @return 掩码中除指定CPU外的任一CPU编号，无则返回nr_cpu_ids
 */
uint32_t vmm_cpumask_any_but(const vmm_cpumask_t *mask, uint32_t cpu)
{
    uint32_t i;

    vmm_cpumask_check(cpu);

    for_each_cpu(i, mask) if (i != cpu) break;

    return i;
}

#endif

/**
 * @brief 设置指定CPU在possible掩码中的状态
 * @param cpu CPU编号
 * @param possible 是否可能出现
 */
void vmm_set_cpu_possible(uint32_t cpu, bool possible)
{
    if (possible) {
        vmm_cpumask_set_cpu(cpu, to_cpumask(cpu_possible_bits));
    } else {
        vmm_cpumask_clear_cpu(cpu, to_cpumask(cpu_possible_bits));
    }
}

/**
 * @brief 设置指定CPU在present掩码中的状态
 * @param cpu CPU编号
 * @param present 是否在线（已安装）
 */
void vmm_set_cpu_present(uint32_t cpu, bool present)
{
    if (present) {
        vmm_cpumask_set_cpu(cpu, to_cpumask(cpu_present_bits));
    } else {
        vmm_cpumask_clear_cpu(cpu, to_cpumask(cpu_present_bits));
    }
}

/**
 * @brief 设置指定CPU在online掩码中的状态
 * @param cpu CPU编号
 * @param online 是否在线
 */
void vmm_set_cpu_online(uint32_t cpu, bool online)
{
    if (online) {
        vmm_cpumask_set_cpu(cpu, to_cpumask(cpu_online_bits));
    } else {
        vmm_cpumask_clear_cpu(cpu, to_cpumask(cpu_online_bits));
    }
}

/**
 * @brief 设置指定CPU在active掩码中的状态
 * @param cpu CPU编号
 * @param active 是否处于活动状态
 */
void vmm_set_cpu_active(uint32_t cpu, bool active)
{
    if (active) {
        vmm_cpumask_set_cpu(cpu, to_cpumask(cpu_active_bits));
    } else {
        vmm_cpumask_clear_cpu(cpu, to_cpumask(cpu_active_bits));
    }
}

/**
 * @brief 用指定掩码初始化CPU present掩码
 * @param src CPU亲和性掩码
 */
void vmm_init_cpu_present(const vmm_cpumask_t *src)
{
    vmm_cpumask_copy(to_cpumask(cpu_present_bits), src);
}

/**
 * @brief 用指定掩码初始化CPU possible掩码
 * @param src CPU亲和性掩码
 */
void vmm_init_cpu_possible(const vmm_cpumask_t *src)
{
    vmm_cpumask_copy(to_cpumask(cpu_possible_bits), src);
}

/**
 * @brief 用指定掩码初始化CPU online掩码
 * @param src CPU亲和性掩码
 */
void vmm_init_cpu_online(const vmm_cpumask_t *src)
{
    vmm_cpumask_copy(to_cpumask(cpu_online_bits), src);
}
