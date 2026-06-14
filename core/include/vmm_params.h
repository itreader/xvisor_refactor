/**
 * Copyright (c) 2013 Himanshu Chauhan.
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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief 启动参数头文件
 */
#ifndef _VMM_PARAMS_H__
#define _VMM_PARAMS_H__

/**
 * @brief 系统启动参数，保存从引导程序传入的初始化配置
 */
typedef struct vmm_setup_param {
    const char *str; /**< 参数名称字符串 */
    int (*setup_func)(char *); /**< 参数设置回调函数 */
    int early; /**< 是否为早期参数标志 */
} vmm_setup_param_t;

/* 仅用于核心代码，参见vmm_module.h了解普通方式 */
#define __setup_param(str, unique_id, fn, early)                                                                                                     \
    static const char __setup_str_##unique_id[] __initconst __aligned(1) = str;                                                                      \
    static vmm_setup_param_t __setup_##unique_id __used __section(.setup.init)                                                                       \
        __attribute__((aligned((sizeof(long))))) = {__setup_str_##unique_id, fn, early}

#define __setup(str, fn)         __setup_param(str, fn, fn, 0)

/** @brief 声明早期参数设置，若fn返回非零值将发出警告 */
#define vmm_early_param(str, fn) __setup_param(str, fn, fn, 1)

/**
 * @brief 解析逗号分隔的早期参数整数值
 * @param str 待处理的字符串
 * @param pint 输出整数指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_get_option(char **str, int *pint);

/**
 * @brief 解析启动时或早期参数
 * @param cmdline 命令行字符串指针
 */
void vmm_parse_early_options(const char *cmdline);

#endif /* _VMM_PARAMS_H__ */
