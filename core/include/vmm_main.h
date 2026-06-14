/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_main.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Hypervisor启动、停止和复位主头文件
 */
#ifndef _VMM_MAIN_H__
#define _VMM_MAIN_H__

#include <vmm_compiler.h>
#include <vmm_types.h>

/**
 * @brief 挂起虚拟机管理器，进入无限循环
 */
void __noreturn vmm_hang(void);

/**
 * @brief 检查虚拟机管理器是否已完成初始化
 * @return 已初始化返回TRUE，否则返回FALSE
 */
bool vmm_init_done(void);

/**
 * @brief 初始化虚拟机管理器
 */
void vmm_init(void);

/**
 * @brief 注册系统复位回调函数
 * @param callback 复位回调函数指针
 */
void vmm_register_system_reset(int (*callback)(void));

/**
 * @brief 执行系统复位
 */
void vmm_reset(void);

/**
 * @brief 注册系统关机回调函数
 * @param callback 关机回调函数指针
 */
void vmm_register_system_shutdown(int (*callback)(void));

/**
 * @brief 执行系统关机
 */
void vmm_shutdown(void);

#endif
