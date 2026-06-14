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
 * @file vmm_command_manager.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 命令管理器头文件
 */
#ifndef _VMM_command_manager_H__
#define _VMM_command_manager_H__

#include <libs/list.h>
#include <vmm_char_device.h>
#include <vmm_limits.h>
#include <vmm_types.h>

/**
 * @brief 管理命令结构，定义命令行名称、帮助信息和处理回调
 */
typedef struct vmm_command {
    double_list_t head; /**< 链表头 */
    char          name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    char          desc[VMM_FIELD_DESC_SIZE]; /**< 描述 */
    void (*usage)(vmm_char_device_t *); /**< usage成员 */
    int (*exec)(vmm_char_device_t *, int, char **); /**< 执行 */
} vmm_command_t;

/**
 * @brief 注册命令到命令管理器
 * @param cmd 命令标识或命令结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_command_manager_register_cmd(vmm_command_t *cmd);

/**
 * @brief 从命令管理器注销命令
 * @param cmd 命令标识或命令结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_command_manager_unregister_cmd(vmm_command_t *cmd);

/**
 * @brief 在命令管理器中查找指定名称的命令
 * @param cmd_name 命令名称字符串
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_command_t *vmm_command_manager_find_cmd(const char *cmd_name);

/**
 * @brief 获取命令管理器的命令
 * @param index 数组中的索引位置
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_command_t *vmm_command_manager_get_cmd(int index);

/**
 * @brief 获取命令管理器命令的数量
 * @return 数量值
 */
uint32_t vmm_command_manager_cmd_count(void);

/**
 * @brief 在命令管理器中查找并执行指定命令
 * @param cdev 字符设备指针
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_command_manager_execute_cmd(vmm_char_device_t *cdev, int argc, char **argv);

/**
 * @brief 解析命令字符串并在命令管理器中执行
 * @param cdev 字符设备指针
 * @param cmds 命令数组指针
 * @param (*filter 布尔值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_command_manager_execute_cmdstr(vmm_char_device_t *cdev, char *cmds, bool (*filter)(vmm_char_device_t *cdev, int argc, char **argv));

/**
 * @brief 初始化命令管理器
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_command_manager_init(void);

#endif
