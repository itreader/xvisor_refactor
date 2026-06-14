/**
 * Copyright (c) 2016 Open Wide
 *               2016 Institut de Recherche Technologique SystemX
 *
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
 * @file vmm_device_emulate_debug.h
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief 设备模拟调试框架头文件
 */
#ifndef _VMM_DEVICE_EMULATE_DEBUG_H__
#define _VMM_DEVICE_EMULATE_DEBUG_H__

#include <vmm_device_emulate.h>

/**
 * Debugging flags that can be set in the Device Tree describing the
 * 接口 hypervisor-guest.
 * All flags but @c VMM_DEVICE_EMULATE_DEBUG_IRQ are automatically handled
 * by Xvisor. @c VMM_DEVICE_EMULATE_DEBUG_IRQ is 实现 defined: it
 * can be used within the 实现 of an emulator to provide
 * a better debugging 接口, but there is no guarantee all emulators
 * do implement it.
 *
 * Emulators can use bits in range [31;16] as 特定的 debug information.
 * Bits [15;0] are reserved for Xvisor (currently only 5 are used).
 *
 *
 * Example:
 * @code
 * node {
 *  debug = <0x7>; // PROBE | RESET | REMOVE
 * };
 * @endcode
 */
enum vmm_device_emulate_debug {
    VMM_DEVICE_EMULATE_DEBUG_NONE   = 0,        /**< 无调试 */
    VMM_DEVICE_EMULATE_DEBUG_PROBE  = (1 << 0), /**< 探测时调试 */
    VMM_DEVICE_EMULATE_DEBUG_RESET  = (1 << 1), /**< 复位时调试 */
    VMM_DEVICE_EMULATE_DEBUG_SYNC   = (1 << 2), /**< 同步时调试 */
    VMM_DEVICE_EMULATE_DEBUG_REMOVE = (1 << 3), /**< 移除时调试 */
    VMM_DEVICE_EMULATE_DEBUG_READ   = (1 << 4), /**< 读操作 */
    VMM_DEVICE_EMULATE_DEBUG_WRITE  = (1 << 5), /**< 写操作时调试 */
    VMM_DEVICE_EMULATE_DEBUG_IRQ    = (1 << 6), /**< IRQ模拟时调试 */
    /* (1 << 7)  is available */
    /* (1 << 8)  is available */
    /* (1 << 9)  is available */
    /* (1 << 10) is available */
    /* (1 << 11) is available */
    /* (1 << 12) is available */
    /* (1 << 13) is available */
    /* (1 << 14) is available */
    /* (1 << 15) is available */
    /* No more debug bits available for Xvisor core */
};

/*
 * When CONFIG_DEVICE_EMULATE_DEBUG is enabled, a field in the vmm_emulate_device structure
 * is added to hold debug flags.
 * The functions below are the only proper way to manipulate this field
 * properly, since it is absent when CONFIG_DEVICE_EMULATE_DEBUG is unset.
 *
 * Since those functions are inlined, branches that use the function
 * below can be optimized out, leading to zero overhead when CONFIG_DEVICE_EMULATE_DEBUG
 * is disabled.
 */

/**
 * @brief 获取设备模拟的调试信息
 * @param edev 模拟设备实例指针
 * @return 设备模拟调试标志位
 */
static inline uint32_t vmm_device_emulate_get_debug_info(const vmm_emulate_device_t *edev);

#ifdef CONFIG_DEVICE_EMULATE_DEBUG
static inline uint32_t vmm_device_emulate_get_debug_info(const vmm_emulate_device_t *edev)
{
    return edev->debug_info;
}
#else  /* ! CONFIG_DEVICE_EMULATE_DEBUG */
static inline uint32_t vmm_device_emulate_get_debug_info(const vmm_emulate_device_t *edev)
{
    return 0;
}
#endif /* CONFIG_DEVICE_EMULATE_DEBUG */

/**
 * @brief 函数接口
 */
static inline bool vmm_device_emulate_debug_probe(const vmm_emulate_device_t *edev)
{
    return (vmm_device_emulate_get_debug_info(edev) & VMM_DEVICE_EMULATE_DEBUG_PROBE);
}

/**
 * @brief 函数接口
 */
static inline bool vmm_device_emulate_debug_reset(const vmm_emulate_device_t *edev)
{
    return (vmm_device_emulate_get_debug_info(edev) & VMM_DEVICE_EMULATE_DEBUG_RESET);
}

/**
 * @brief 函数接口
 */
static inline bool vmm_device_emulate_debug_sync(const vmm_emulate_device_t *edev)
{
    return (vmm_device_emulate_get_debug_info(edev) & VMM_DEVICE_EMULATE_DEBUG_SYNC);
}

/**
 * @brief 函数接口
 */
static inline bool vmm_device_emulate_debug_remove(const vmm_emulate_device_t *edev)
{
    return (vmm_device_emulate_get_debug_info(edev) & VMM_DEVICE_EMULATE_DEBUG_REMOVE);
}

/**
 * @brief 函数接口
 */
static inline bool vmm_device_emulate_debug_read(const vmm_emulate_device_t *edev)
{
    return (vmm_device_emulate_get_debug_info(edev) & VMM_DEVICE_EMULATE_DEBUG_READ);
}

/**
 * @brief 函数接口
 */
static inline bool vmm_device_emulate_debug_write(const vmm_emulate_device_t *edev)
{
    return (vmm_device_emulate_get_debug_info(edev) & VMM_DEVICE_EMULATE_DEBUG_WRITE);
}

/**
 * @brief 提供设备模拟调试信息
 */
static inline bool vmm_device_emulate_debug_irq(const vmm_emulate_device_t *edev)
{
    return (vmm_device_emulate_get_debug_info(edev) & VMM_DEVICE_EMULATE_DEBUG_IRQ);
}

#endif /* ! _VMM_DEVICE_EMULATE_DEBUG_H__ */
