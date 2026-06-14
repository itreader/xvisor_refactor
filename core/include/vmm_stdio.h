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
 * @file vmm_stdio.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 标准输入输出头文件
 */
#ifndef _VMM_STDIO_H__
#define _VMM_STDIO_H__

#include <libs/stack_trace.h>
#include <vmm_compiler.h>
#include <vmm_types.h>

#include <stdarg.h>

#define BUG_ON(x)                                                                                                                                    \
    do {                                                                                                                                             \
        if (unlikely(x)) {                                                                                                                           \
            vmm_lemergency(NULL, "Bug in %s() at %s:%d\n", __func__, __FILE__, __LINE__);                                                            \
            dump_stack_trace();                                                                                                                      \
            __vmm_panic("Please reset the system ...\n");                                                                                            \
        }                                                                                                                                            \
    } while (0)

#define BUG() BUG_ON(1)

#define WARN_ON(x)                                                                                                                                   \
    ({                                                                                                                                               \
        if (unlikely(x)) {                                                                                                                           \
            vmm_lwarning(NULL, "%s() at %s:%d\n", __func__, __FILE__, __LINE__);                                                                     \
            dump_stack_trace();                                                                                                                      \
        }                                                                                                                                            \
        (x);                                                                                                                                         \
    })

#define WARN(x, msg...)                                                                                                                              \
    ({                                                                                                                                               \
        if (unlikely(x)) {                                                                                                                           \
            vmm_lwarning(NULL, msg);                                                                                                                 \
            vmm_lwarning(NULL, "%s() at %s:%d\n", __func__, __FILE__, __LINE__);                                                                     \
            dump_stack_trace();                                                                                                                      \
        }                                                                                                                                            \
        (x);                                                                                                                                         \
    })

/** Representation of input history for use with (c)gets */
struct vmm_history {
    int    length; /* Number of entries in the history table */
    int    width;  /* Width of each entry */
    char **table;  /* Circular History Table */
    int    tail;   /* Last entry */
};

/** Initialize vmm_history pointer h having l length and w width */
#define INIT_HISTORY(h, l, w)                                                                                                                        \
    {                                                                                                                                                \
        int iter    = 0;                                                                                                                             \
        (h)->length = (l);                                                                                                                           \
        (h)->width  = (w);                                                                                                                           \
        (h)->table  = vmm_malloc((l) * sizeof(char *));                                                                                              \
        for (iter = 0; iter < (l); iter++) {                                                                                                         \
            (h)->table[iter]    = vmm_malloc((w) * sizeof(char));                                                                                    \
            (h)->table[iter][0] = '\0';                                                                                                              \
        }                                                                                                                                            \
        (h)->tail = 0;                                                                                                                               \
    }

/** Cleanup vmm_history pointer */
#define CLEANUP_HISTORY(h)                                                                                                                           \
    {                                                                                                                                                \
        int iter = 0;                                                                                                                                \
        for (iter = 0; iter < (h)->length; iter++) {                                                                                                 \
            vmm_free((h)->table[iter]);                                                                                                              \
        }                                                                                                                                            \
        vmm_free((h)->table);                                                                                                                        \
    }

struct vmm_char_device;
typedef struct vmm_char_device vmm_char_device_t;

/**
 * @brief 检查字符是否为控制字符
 * @param c 字符设备指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_is_control(char c);

/**
 * @brief 检查字符是否为可打印字符
 * @param c 字符设备指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_is_printable(char c);

/**
 * @brief printchars
 * @param cdev 字符设备指针
 * @param ch 字符值
 * @param num_ch 数量
 * @param block 块设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_printchars(vmm_char_device_t *cdev, char *ch, uint32_t num_ch, bool block);

/**
 * @brief 字符设备输出单个字符
 * @param cdev 字符设备指针
 * @param ch 字符值
 */
void vmm_cdev_putc(vmm_char_device_t *cdev, char ch);

/**
 * @brief putc
 * @param ch 字符值
 */
void vmm_putc(char ch);

/**
 * @brief 字符设备输出字符串
 * @param cdev 字符设备指针
 * @param str 待处理的字符串
 */
void vmm_cdev_puts(vmm_char_device_t *cdev, char *str);

/**
 * @brief puts
 * @param str 待处理的字符串
 */
void vmm_puts(char *str);

/**
 * @brief   snprintf
 * @param out 用于返回读取结果的输出指针
 * @param out_sz 用于返回输出数据大小
 * @param format 格式化字符串
 * @param args 参数数组指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __vmm_snprintf(char *out, uint32_t out_sz, const char *format, va_list args);

/**
 * @brief   printf
 * @param 2 参数2
 * @param 3 参数3
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __printf(2, 3) vmm_sprintf(char *out, const char *format, ...);

/**
 * @brief   printf
 * @param 3 参数3
 * @param 4 参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __printf(3, 4) vmm_snprintf(char *out, uint32_t out_sz, const char *format, ...);

/**
 * @brief   printf
 * @param 2 参数2
 * @param 3 参数3
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __printf(2, 3) vmm_cdev_printf(vmm_char_device_t *cdev, const char *format, ...);

/**
 * @brief   printf
 * @param 1 参数1
 * @param 2 参数2
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __printf(1, 2) vmm_printf(const char *format, ...);

/**
 * @brief   printf
 * @param 1 参数1
 * @param 2 参数2
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __printf(1, 2) vmm_init_printf(const char *format, ...);

/**
 * @brief 字符设备十六进制转储输出
 * @param cdev 字符设备指针
 * @param print_base_addr 是否打印基地址标志
 * @param data 用户自定义数据指针
 * @param len 数据长度
 */
void vmm_cdev_hexdump(vmm_char_device_t *cdev, uint64_t print_base_addr, void *data, uint64_t len);

/**
 * @brief 将版本信息输出到字符设备
 * @param cdev 字符设备指针
 */
void vmm_cdev_print_version(vmm_char_device_t *cdev);

/** Print version string to default device */
#define vmm_printver() vmm_cdev_print_version(NULL)

/** Predefined log levels */
/**
 * @brief 打印级别枚举，定义从紧急到调试的各日志输出等级
 */
enum vmm_print_level {
    VMM_LOGLEVEL_EMERGENCY = 0, /**< 0 */
    VMM_LOGLEVEL_ALERT     = 1, /**< 1 */
    VMM_LOGLEVEL_CRITICAL  = 2, /**< 2 */
    VMM_LOGLEVEL_ERROR     = 3, /**< 3 */
    VMM_LOGLEVEL_WARNING   = 4, /**< 4 */
    VMM_LOGLEVEL_NOTICE    = 5, /**< 5 */
    VMM_LOGLEVEL_INFO      = 6, /**< 6 */
};

/**
 * @brief   printf
 * @param 3 参数3
 * @param 4 参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __printf(3, 4) vmm_lprintf(enum vmm_print_level level, const char *prefix, const char *format, ...);

#define vmm_lprintf_once(level, prefix, msg...)                                                                                                      \
    ({                                                                                                                                               \
        static bool __print_once __read_mostly;                                                                                                      \
                                                                                                                                                     \
        if (!__print_once) {                                                                                                                         \
            __print_once = TRUE;                                                                                                                     \
            vmm_lprintf(level, prefix, msg);                                                                                                         \
        }                                                                                                                                            \
    })

/** Print formatted string to default device if current
 *  stdio log level is greater than or equal to specified level
 */

#define vmm_lemergency(prefix, msg...)  vmm_lprintf(VMM_LOGLEVEL_EMERGENCY, prefix, msg)
#define vmm_lalert(prefix, msg...)      vmm_lprintf(VMM_LOGLEVEL_ALERT, prefix, msg)
#define vmm_lcritical(prefix, msg...)   vmm_lprintf(VMM_LOGLEVEL_CRITICAL, prefix, msg)
#define vmm_lerror(prefix, msg...)      vmm_lprintf(VMM_LOGLEVEL_ERROR, prefix, msg)
#define vmm_lwarning(prefix, msg...)    vmm_lprintf(VMM_LOGLEVEL_WARNING, prefix, msg)
#define vmm_lnotice(prefix, msg...)     vmm_lprintf(VMM_LOGLEVEL_NOTICE, prefix, msg)
#define vmm_linfo(prefix, msg...)       vmm_lprintf(VMM_LOGLEVEL_INFO, prefix, msg)
#define vmm_lerror_once(prefix, msg...) vmm_lprintf_once(VMM_LOGLEVEL_ERROR, prefix, msg)

/**
 * @brief   恐慌
 * @param format 格式化字符串
 * @param ... 参数
 * @return 打印的字符数
 */
void __noreturn __vmm_panic(const char *format, ...);

#define vmm_panic(msg...)                                                                                                                            \
    do {                                                                                                                                             \
        vmm_lemergency(NULL, msg);                                                                                                                   \
        dump_stack_trace();                                                                                                                          \
        __vmm_panic("Please reset the system ...\n");                                                                                                \
    } while (0)

/**
 * @brief scanchars
 * @param cdev 字符设备指针
 * @param ch 字符值
 * @param num_ch 数量
 * @param block 块设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_scanchars(vmm_char_device_t *cdev, char *ch, uint32_t num_ch, bool block);

/**
 * @brief cgetc
 * @param cdev 字符设备指针
 * @param lecho 是否本地回显标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
char vmm_cgetc(vmm_char_device_t *cdev, bool lecho);

/**
 * @brief getc
 * @param lecho 是否本地回显标志
 * @return 获取到的值，失败返回错误码
 */
char vmm_getc(bool lecho);

/**
 * @brief cgets
 * @param cdev 字符设备指针
 * @param s 字符串或数据指针
 * @param maxwidth 最大宽度值
 * @param endchar 结束字符
 * @param history 历史记录缓冲区指针
 * @param lecho 是否本地回显标志
 * @return 目标对象指针，不存在返回NULL
 */
char *vmm_cgets(vmm_char_device_t *cdev, char *s, int maxwidth, char endchar, struct vmm_history *history, bool lecho);

/**
 * @brief gets
 * @param s 字符串或数据指针
 * @param maxwidth 最大宽度值
 * @param endchar 结束字符
 * @param history 历史记录缓冲区指针
 * @param lecho 是否本地回显标志
 * @return 目标对象指针，不存在返回NULL
 */
char *vmm_gets(char *s, int maxwidth, char endchar, struct vmm_history *history, bool lecho);

/**
 * @brief 标准IO 设备
 * @return 目标对象指针，不存在返回NULL
 */
vmm_char_device_t *vmm_stdio_device(void);

/**
 * @brief 切换标准输入输出设备
 * @param cdev 字符设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_stdio_change_device(vmm_char_device_t *cdev);

/**
 * @brief 获取或设置标准IO日志级别
 * @return 成功返回VMM_OK，失败返回错误码
 */
long vmm_stdio_loglevel(void);

/**
 * @brief 修改标准IO日志级别
 * @param loglevel 日志级别
 */
void vmm_stdio_change_loglevel(long loglevel);

/**
 * @brief 初始化标准IO
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_stdio_init(void);

#endif
