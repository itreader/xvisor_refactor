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
 * @file vmm_modules.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 模块管理代码头文件
 */
#ifndef _VMM_MODULES_H__
#define _VMM_MODULES_H__

#include <libs/kallsyms.h>
#include <libs/list.h>
#include <vmm_compiler.h>
#include <vmm_types.h>

#define VMM_MODULE_SIGNATURE 0x564D4F44

typedef int (*vmm_module_init_t)(void);
typedef void (*vmm_module_exit_t)(void);

/*
 * The following license idents are currently accepted as indicating free
 * software modules
 *
 *  "GPL"               [GNU Public License v2 or later]
 *  "GPL v2"            [GNU Public License v2]
 *  "GPL and additional rights" [GNU Public License v2 rights and more]
 *  "Dual BSD/GPL"          [GNU Public License v2
 *                   or BSD license choice]
 *  "Dual MIT/GPL"          [GNU Public License v2
 *                   or MIT license choice]
 *  "Dual MPL/GPL"          [GNU Public License v2
 *                   or Mozilla license choice]
 *
 * The following other idents are available
 *
 *  "Proprietary"           [Non free products]
 *
 * There are dual licensed components, but when running with Linux it is the
 * GPL that is relevant so this is a non issue. Similarly LGPL linked with GPL
 * is a GPL combined work.
 *
 * This exists for several reasons
 * 1.   So modinfo can show license info for users wanting to vet their setup
 *  is free
 * 2.   So the community can ignore bug reports including proprietary modules
 * 3.   So vendors can do likewise based on their own policies
 */

#include <vmm_limits.h>

/**
 * @brief 可加载模块结构，封装模块的初始化/退出回调和元信息
 */
struct vmm_module {
    uint32_t          signature; /**< 签名标识 */
    char              name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    char              desc[VMM_FIELD_DESC_SIZE]; /**< 描述 */
    char              author[VMM_FIELD_AUTHOR_SIZE]; /**< 作者 */
    char              license[VMM_FIELD_LICENSE_SIZE]; /**< 许可证 */
    uint32_t          ipriority; /**< 初始化优先级 */
    vmm_module_init_t init; /**< 初始化回调 */
    vmm_module_exit_t exit; /**< 退出回调 */
    double_list_t     head; /**< 链表头 */
};

typedef struct vmm_module vmm_module_t;

/**
 * @brief 符号类型枚举，区分模块导出的函数和数据符号
 */
enum vmm_symbol_types {
    VMM_SYMBOL_ANY        = 0, /**< 0 */
    VMM_SYMBOL_GPL        = 1, /**< 1 */
    VMM_SYMBOL_GPL_FUTURE = 2, /**< 2 */
    VMM_SYMBOL_UNUSED     = 3, /**< 3 */
    VMM_SYMBOL_UNUSED_GPL = 4, /**< 4 */
};

/**
 * @brief 导出符号结构，保存符号名称和地址
 */
struct vmm_symbol {
    char           name[KSYM_NAME_LEN]; /**< 名称 */
    virtual_addr_t addr; /**< 地址 */
    uint32_t       type; /**< 类型 */
};

#define EXPAND(VAR)         #VAR
#define MACRO_CONCAT(X, Y)  X##Y
#define MACRO_CONCAT2(X, Y) MACRO_CONCAT(X, Y)

#ifdef __VMM_MODULES__

#define __VMM_DECLARE_MODULE(_var, _name, _desc, _author, _license, _ipriority, _init, _exit)                                                        \
    __modtbl vmm_module_t _var = {                                                                                                                   \
        .signature = VMM_MODULE_SIGNATURE,                                                                                                           \
        .name      = stringify(_name),                                                                                                               \
        .desc      = _desc,                                                                                                                          \
        .author    = _author,                                                                                                                        \
        .license   = _license,                                                                                                                       \
        .ipriority = _ipriority,                                                                                                                     \
        .init      = _init,                                                                                                                          \
        .exit      = _exit,                                                                                                                          \
        .head      = LIST_HEAD_INIT(_var.head),                                                                                                      \
    }

#define __VMM_ERR_XPORT_SYMBOL(sym, _type)                                                                                                              \
    __symtbl struct vmm_symbol __exported_##sym = {                                                                                                  \
        .name = #sym,                                                                                                                                \
        .addr = (virtual_addr_t) & sym,                                                                                                              \
        .type = (_type),                                                                                                                             \
    }

#else

#define __VMM_DECLARE_MODULE(_var, _name, _desc, _author, _license, _ipriority, _init, _exit)                                                        \
    __modtbl vmm_module_t _var = {                                                                                                                   \
        .signature = VMM_MODULE_SIGNATURE,                                                                                                           \
        .name      = stringify(_name),                                                                                                               \
        .desc      = _desc,                                                                                                                          \
        .author    = _author,                                                                                                                        \
        .license   = "GPL",                                                                                                                          \
        .ipriority = _ipriority,                                                                                                                     \
        .init      = _init,                                                                                                                          \
        .exit      = _exit,                                                                                                                          \
        .head      = LIST_HEAD_INIT(_var.head),                                                                                                      \
    }

#define __VMM_ERR_XPORT_SYMBOL(sym, _type)

#endif

#define MODTBL_VAR(NAME) MACRO_CONCAT2(MACRO_CONCAT(__modtable__, NAME), __LINE__)

#define VMM_DECLARE_MODULE(_desc, _author, _license, _ipriority, _init, _exit)                                                                       \
    __VMM_DECLARE_MODULE(MODTBL_VAR(VMM_MODNAME), VMM_MODNAME, _desc, _author, _license, _ipriority, _init, _exit)

#define VMM_DECLARE_MODULE2(_name, _desc, _author, _license, _ipriority, _init, _exit)                                                               \
    __VMM_DECLARE_MODULE(MODTBL_VAR(_name), _name, _desc, _author, _license, _ipriority, _init, _exit)

#define VMM_ERR_XPORT_SYMBOL(sym)            __VMM_ERR_XPORT_SYMBOL(sym, VMM_SYMBOL_ANY)

#define VMM_ERR_XPORT_SYMBOL_GPL(sym)        __VMM_ERR_XPORT_SYMBOL(sym, VMM_SYMBOL_GPL)

#define VMM_ERR_XPORT_SYMBOL_GPL_FUTURE(sym) __VMM_ERR_XPORT_SYMBOL(sym, VMM_SYMBOL_GPL_FUTURE)

#define VMM_ERR_XPORT_SYMBOL_UNUSED(sym)     __VMM_ERR_XPORT_SYMBOL(sym, VMM_SYMBOL_UNUSED)

#define VMM_ERR_XPORT_SYMBOL_UNUSED_GPL(sym) __VMM_ERR_XPORT_SYMBOL(sym, VMM_SYMBOL_UNUSED_GPL)

/**
 * @brief 在已加载模块中查找符号地址
 * @param symname 符号名称
 * @param sym 符号结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_modules_find_symbol(const char *symname, struct vmm_symbol *sym);

/**
 * @brief 检查模块是否为内建模块
 * @param mod 模块结构体指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_modules_isbuiltin(vmm_module_t *mod);

/**
 * @brief 模块 加载
 * @param load_addr 加载地址
 * @param load_size 加载大小（字节）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_modules_load(virtual_addr_t load_addr, virtual_size_t load_size);

/**
 * @brief 卸载模块
 * @param mod 模块结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_modules_unload(vmm_module_t *mod);

/**
 * @brief 获取模块实例
 * @param index 索引
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_module_t *vmm_modules_getmodule(uint32_t index);

/**
 * @brief 获取模块的数量
 * @return 数量值
 */
uint32_t vmm_modules_count(void);

/**
 * @brief 初始化模块
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_modules_init(void);

#endif
