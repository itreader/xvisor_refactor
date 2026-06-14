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
 * @file vmm_stdio.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 标准输入输出源文件
 */

#include <arch_atomic.h>
#include <arch_default_terminal.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_char_device.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_scheduler.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>
#include <vmm_version.h>

#define PAD_RIGHT     1
#define PAD_ZERO      2
#define PAD_ALTERNATE 4
/* the following should be enough for 32 bit int */
#define PRINT_BUF_LEN 64

#ifdef CONFIG_LOG_ANSI_COLORS
#define VMM_LOG_COLOR_RESET     "\033[0m"
#define VMM_LOG_COLOR_LIGHTRED  "\033[31;1m"
#define VMM_LOG_COLOR_RED       "\033[31m"
#define VMM_LOG_COLOR_LIGHTBLUE "\033[34;1m"
#define VMM_LOG_COLOR_BLUE      "\033[34m"
#define VMM_LOG_COLOR_GREEN     "\033[32;1m"
#define VMM_LOG_COLOR_YELLOW    "\033[33;1m"
#define VMM_LOG_COLOR_ORANGE    "\033[0;33m"
#define VMM_LOG_COLOR_WHITE     "\033[37;1m"
#define VMM_LOG_COLOR_LIGHTCYAN "\033[36;1m"
#define VMM_LOG_COLOR_CYAN      "\033[36m"
#define VMM_LOG_COLOR_REVERSE   "\033[7m"
#else /* ! CONFIG_LOG_ANSI_COLORS */
#define VMM_LOG_COLOR_RESET     ""
#define VMM_LOG_COLOR_LIGHTRED  ""
#define VMM_LOG_COLOR_RED       ""
#define VMM_LOG_COLOR_LIGHTBLUE ""
#define VMM_LOG_COLOR_BLUE      ""
#define VMM_LOG_COLOR_GREEN     ""
#define VMM_LOG_COLOR_YELLOW    ""
#define VMM_LOG_COLOR_ORANGE    ""
#define VMM_LOG_COLOR_WHITE     ""
#define VMM_LOG_COLOR_LIGHTCYAN ""
#define VMM_LOG_COLOR_CYAN      ""
#define VMM_LOG_COLOR_REVERSE   ""
#endif /* CONFIG_LOG_ANSI_COLORS */

/* Strings used when prefixing a log level */
#define VMM_LOG_INFO      VMM_LOG_COLOR_CYAN VMM_LOG_COLOR_REVERSE "INFO:"
#define VMM_LOG_NOTICE    VMM_LOG_COLOR_GREEN VMM_LOG_COLOR_REVERSE "NOTE:"
#define VMM_LOG_WARNING   VMM_LOG_COLOR_YELLOW VMM_LOG_COLOR_REVERSE "WARNING:"
#define VMM_LOG_ERROR     VMM_LOG_COLOR_ORANGE VMM_LOG_COLOR_REVERSE "ERROR:"
#define VMM_LOG_CRITICAL  VMM_LOG_COLOR_RED VMM_LOG_COLOR_REVERSE "CRITICAL:"
#define VMM_LOG_ALERT     VMM_LOG_COLOR_LIGHTRED VMM_LOG_COLOR_REVERSE "ALERT:"
#define VMM_LOG_EMERGENCY VMM_LOG_COLOR_LIGHTRED VMM_LOG_COLOR_REVERSE "FATAL:"

static char const *const _log_prefixes[] = {
    [VMM_LOGLEVEL_EMERGENCY] = VMM_LOG_EMERGENCY, [VMM_LOGLEVEL_ALERT] = VMM_LOG_ALERT,     [VMM_LOGLEVEL_CRITICAL] = VMM_LOG_CRITICAL,
    [VMM_LOGLEVEL_ERROR] = VMM_LOG_ERROR,         [VMM_LOGLEVEL_WARNING] = VMM_LOG_WARNING, [VMM_LOGLEVEL_NOTICE] = VMM_LOG_NOTICE,
    [VMM_LOGLEVEL_INFO] = VMM_LOG_INFO,
};

/**
 * @brief 标准IO控制结构，管理终端输入输出和打印级别
 */
struct vmm_stdio_ctrl {
    atomic_t           loglevel; /**< loglevel成员 */
    vmm_spinlock_t     lock; /**< 自旋锁 */
    vmm_char_device_t *dev; /**< 设备 */
};

static struct vmm_stdio_ctrl m_stdio_ctrl;
static bool                  m_stdio_init_done = FALSE;

/**
 * @brief  格式化 错误
 * @param fmt 格式化字符串
 * @param err 错误码
 */
static inline void _vmm_format_error(const char *fmt, char err)
{
    vmm_printf(
        "\n*** vmm_stdio: invalid specifier within format "
        "\"%%%s\": '%c' (0x%x)\n",
        fmt, err, err);
}

/**
 * @brief 检查字符是否为控制字符
 * @param c 字符设备指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_is_control(char c)
{
    return ((0 <= c) && (c < 32)) ? TRUE : FALSE;
}

/**
 * @brief 检查字符是否为可打印字符
 * @param c 字符设备指针
 * @return 条件满足返回TRUE，否则返回FALSE
 */
bool vmm_is_printable(char c)
{
    if (((31 < c) && (c < 127)) || (c == '\f') || (c == '\r') || (c == '\n') || (c == '\t')) {
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief printchars
 * @param cdev 字符设备指针
 * @param ch 字符值
 * @param num_ch 数量
 * @param block 块设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_printchars(vmm_char_device_t *cdev, char *ch, uint32_t num_ch, bool block)
{
    int i;
    int rc;

    if (!ch || !num_ch) {
        return VMM_ERR_FAIL;
    }

    if (m_stdio_init_done) {
        if (cdev) {
            rc = vmm_char_device_dowrite(cdev, (uint8_t *)ch, num_ch, NULL, block) ? VMM_OK : VMM_ERR_FAIL;
        } else {
            for (i = 0; i < num_ch; i++) {
                while ((rc = arch_default_terminal_putc((uint8_t)ch[i])) && block)
                    ;
            }
        }
    } else {
        for (i = 0; i < num_ch; i++) {
            arch_default_terminal_early_putc(ch[i]);
        }

        rc = VMM_OK;
    }

    return rc;
}

/**
 * @brief 字符设备输出单个字符
 * @param cdev 字符设备指针
 * @param ch 字符值
 */
void vmm_cdev_putc(vmm_char_device_t *cdev, char ch)
{
    if (ch == '\n') {
        vmm_printchars(cdev, "\r", 1, TRUE);
    }

    vmm_printchars(cdev, &ch, 1, TRUE);
}

/**
 * @brief putc
 * @param ch 字符值
 */
void vmm_putc(char ch)
{
    vmm_cdev_putc(m_stdio_ctrl.dev, ch);
}

/**
 * @brief 字符设备输出字符串
 * @param cdev 字符设备指针
 * @param str 待处理的字符串
 */
void vmm_cdev_puts(vmm_char_device_t *cdev, char *str)
{
    if (!str) {
        return;
    }

    while (*str) {
        vmm_cdev_putc(cdev, *str);
        str++;
    }
}

/**
 * @brief puts
 * @param str 待处理的字符串
 */
void vmm_puts(char *str)
{
    vmm_cdev_puts(m_stdio_ctrl.dev, str);
}

/**
 * @brief printc
 * @param out 用于返回读取结果的输出指针
 * @param out_len 大小
 * @param cdev 字符设备指针
 * @param ch 字符值
 */
static void printc(char **out, uint32_t *out_len, vmm_char_device_t *cdev, char ch)
{
    if (out) {
        if (*out) {
            if (out_len && (0 < *out_len)) {
                **out = ch;
                ++(*out);
                (*out_len)--;
            } else {
                **out = ch;
                ++(*out);
            }
        }
    } else {
        vmm_cdev_putc(cdev, ch);
    }
}

/**
 * @brief prints
 * @param out 用于返回读取结果的输出指针
 * @param out_len 大小
 * @param cdev 字符设备指针
 * @param string 待匹配的字符串
 * @param width 宽度值
 * @param flags 标志位
 * @return 打印的字符数
 */
static int prints(char **out, uint32_t *out_len, vmm_char_device_t *cdev, const char *string, int width, int flags)
{
    int  pc      = 0;
    char padchar = ' ';

    if (width > 0) {
        int         len = 0;
        const char *ptr;

        for (ptr = string; *ptr; ++ptr) {
            ++len;
        }

        if (len >= width) {
            width = 0;
        } else {
            width -= len;
        }

        if (flags & PAD_ZERO) {
            padchar = '0';
        }
    }

    if (!(flags & PAD_RIGHT)) {
        for (; width > 0; --width) {
            printc(out, out_len, cdev, padchar);
            ++pc;
        }
    }

    for (; *string; ++string) {
        printc(out, out_len, cdev, *string);
        ++pc;
    }

    for (; width > 0; --width) {
        printc(out, out_len, cdev, padchar);
        ++pc;
    }

    return pc;
}

/**
 * @brief printi
 * @param out 用于返回读取结果的输出指针
 * @param out_len 大小
 * @param cdev 字符设备指针
 * @param i 循环索引
 * @param b 字节值或缓冲区
 * @param sg 散列聚含列表指针
 * @param width 宽度值
 * @param flags 标志位
 * @param letbase 进制基数值
 * @return 打印的字符数
 */
static int printi(char **out, uint32_t *out_len, vmm_char_device_t *cdev, long long i, int b, int sg, int width, int flags, int letbase)
{
    char     print_buf[PRINT_BUF_LEN];
    char    *s;
    int t;
    int neg = 0;
    int pc = 0;
    uint64_t u = i;

    if (sg && b == 10 && i < 0) {
        neg = 1;
        u   = -i;
    }

    s  = print_buf + PRINT_BUF_LEN - 1;
    *s = '\0';

    if (!u) {
        *--s = '0';
    } else {
        while (u) {
            t = umod64(u, b);

            if (t >= 10) {
                t += letbase - '0' - 10;
            }

            *--s = t + '0';
            u    = udiv64(u, b);
        }
    }

    if (flags & PAD_ALTERNATE) {
        if ((b == 16) && (letbase == 'A')) {
            *--s = 'X';
        } else if ((b == 16) && (letbase == 'a')) {
            *--s = 'x';
        }

        *--s = '0';
    }

    if (neg) {
        if (width && (flags & PAD_ZERO)) {
            printc(out, out_len, cdev, '-');
            ++pc;
            --width;
        } else {
            *--s = '-';
        }
    }

    return pc + prints(out, out_len, cdev, s, width, flags);
}

/**
 * @brief 打印
 * @param out 用于返回读取结果的输出指针
 * @param out_len 大小
 * @param cdev 字符设备指针
 * @param format 格式化字符串
 * @param args 参数数组指针
 * @return 打印的字符数
 */
static int print(char **out, uint32_t *out_len, vmm_char_device_t *cdev, const char *format, va_list args)
{
    int width;
    int flags;
    int acnt = 0;
    int      pc = 0;
    char     scr[2];
    uint64_t tmp;

    for (; *format != 0; ++format) {
        if (*format == '%') {
            ++format;
            width = flags = 0;

            if (*format == '\0') {
                break;
            } else if (*format == '%') {
                goto out;
            }
            /* Get flags */
            else if (*format == '-') {
                ++format;
                flags = PAD_RIGHT;
            } else if (*format == '#') {
                ++format;
                flags |= PAD_ALTERNATE;
            }

            while (*format == '0') {
                ++format;
                flags |= PAD_ZERO;
            }

            /* Get width */
            for (; *format >= '0' && *format <= '9'; ++format) {
                width *= 10;
                width += *format - '0';
            }

            if (*format == 's') {
                char *s = va_arg(args, char *);
                acnt += sizeof(char *);
                pc += prints(out, out_len, cdev, s ? s : "(null)", width, flags);
            } else if ((*format == 'd') || (*format == 'i')) {
                pc += printi(out, out_len, cdev, va_arg(args, int), 10, 1, width, flags, '0');
                acnt += sizeof(int);
            } else if (*format == 'x') {
                pc += printi(out, out_len, cdev, va_arg(args, uint32_t), 16, 0, width, flags, 'a');
                acnt += sizeof(uint32_t);
            } else if (*format == 'X') {
                pc += printi(out, out_len, cdev, va_arg(args, uint32_t), 16, 0, width, flags, 'A');
                acnt += sizeof(uint32_t);
            } else if (*format == 'u') {
                pc += printi(out, out_len, cdev, va_arg(args, uint32_t), 10, 0, width, flags, 'a');
                acnt += sizeof(uint32_t);
            } else if (*format == 'p') {
                pc += printi(out, out_len, cdev, va_arg(args, uint64_t), 16, 0, width, flags, 'a');
                acnt += sizeof(uint64_t);
            } else if (*format == 'z') {
                if (*(format + 1) == 'x') {
                    format += 1;
                    pc += printi(out, out_len, cdev, va_arg(args, size_t), 16, 0, width, flags, 'a');
                    acnt += sizeof(size_t);
                } else if (*(format + 1) == 'X') {
                    format += 1;
                    pc += printi(out, out_len, cdev, va_arg(args, size_t), 16, 0, width, flags, 'A');
                    acnt += sizeof(size_t);
                } else if (*(format + 1) == 'u') {
                    format += 1;
                    pc += printi(out, out_len, cdev, va_arg(args, size_t), 10, 0, width, flags, 'a');
                    acnt += sizeof(size_t);
                } else if ((*(format + 1) == 'd') || (*(format + 1) == 'i')) {
                    format += 1;
                    pc += printi(out, out_len, cdev, va_arg(args, ssize_t), 10, 1, width, flags, '0');
                    acnt += sizeof(ssize_t);
                } else { /* Unhandled cases */
                    _vmm_format_error("z", *(format + 1));
                }
            } else if (*format == 'l' && *(format + 1) == 'l') {
                tmp = va_arg(args, uint64_t);

                if (*(format + 2) == 'u') {
                    format += 2;
                    pc += printi(out, out_len, cdev, tmp, 10, 0, width, flags, 'a');
                } else if (*(format + 2) == 'x') {
                    format += 2;
                    pc += printi(out, out_len, cdev, tmp, 16, 0, width, flags, 'a');
                } else if (*(format + 2) == 'X') {
                    format += 2;
                    pc += printi(out, out_len, cdev, tmp, 16, 0, width, flags, 'A');
                } else if ((*(format + 2) == 'd') || (*(format + 2) == 'i')) {
                    format += 2;
                    pc += printi(out, out_len, cdev, tmp, 10, 1, width, flags, '0');
                } else { /* Unhandled cases */
                    _vmm_format_error("ll", *(format + 2));
                }
            } else if (*format == 'l') {
                if (*(format + 1) == 'x') {
                    format += 1;
                    pc += printi(out, out_len, cdev, va_arg(args, uint64_t), 16, 0, width, flags, 'a');
                    acnt += sizeof(uint64_t);
                } else if (*(format + 1) == 'X') {
                    format += 1;
                    pc += printi(out, out_len, cdev, va_arg(args, uint64_t), 16, 0, width, flags, 'A');
                    acnt += sizeof(uint64_t);
                } else if (*(format + 1) == 'u') {
                    format += 1;
                    pc += printi(out, out_len, cdev, va_arg(args, uint64_t), 10, 0, width, flags, 'a');
                    acnt += sizeof(uint64_t);
                } else if ((*(format + 1) == 'd') || (*(format + 1) == 'i')) {
                    format += 1;
                    pc += printi(out, out_len, cdev, va_arg(args, long), 10, 1, width, flags, '0');
                    acnt += sizeof(long);
                } else { /* Unhandled cases */
                    _vmm_format_error("l", *(format + 1));
                }
            } else if (*format == 'c') {
                /* char are converted to int then pushed on the stack */
                scr[0] = va_arg(args, int);
                scr[1] = '\0';
                pc += prints(out, out_len, cdev, scr, width, flags);
                acnt += sizeof(int);
            } else {
                _vmm_format_error("", *format);
            }
        } else {
        out:
            printc(out, out_len, cdev, *format);
            ++pc;
        }
    }

    if (out) {
        **out = '\0';
    }

    return pc;
}

/**
 * @brief sprintf
 * @param out 用于返回读取结果的输出指针
 * @param format 格式化字符串
 * @param ... 参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_sprintf(char *out, const char *format, ...)
{
    va_list args;
    int     retval;
    va_start(args, format);
    retval = print(&out, NULL, m_stdio_ctrl.dev, format, args);
    va_end(args);
    return retval;
}

/**
 * @brief   snprintf
 * @param out 用于返回读取结果的输出指针
 * @param out_sz 用于返回输出数据大小
 * @param format 格式化字符串
 * @param args 参数数组指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __vmm_snprintf(char *out, uint32_t out_sz, const char *format, va_list args)
{
    return print(&out, &out_sz, m_stdio_ctrl.dev, format, args);
}

/**
 * @brief snprintf
 * @param out 用于返回读取结果的输出指针
 * @param out_sz 用于返回输出数据大小
 * @param format 格式化字符串
 * @param ... 参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_snprintf(char *out, uint32_t out_sz, const char *format, ...)
{
    va_list args;
    int     retval;
    va_start(args, format);
    retval = print(&out, &out_sz, m_stdio_ctrl.dev, format, args);
    va_end(args);
    return retval;
}

/**
 * @brief cvprintf
 * @param cdev 字符设备指针
 * @param format 格式化字符串
 * @param args 参数数组指针
 * @return 打印的字符数
 */
static int vmm_cvprintf(vmm_char_device_t *cdev, const char *format, va_list args)
{
    return print(NULL, NULL, (cdev) ? cdev : m_stdio_ctrl.dev, format, args);
}

/**
 * @brief 字符设备格式化输出
 * @param cdev 字符设备指针
 * @param format 格式化字符串
 * @param ... 参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_cdev_printf(vmm_char_device_t *cdev, const char *format, ...)
{
    va_list args;
    int     retval;
    va_start(args, format);
    retval = vmm_cvprintf(cdev, format, args);
    va_end(args);
    return retval;
}

/**
 * @brief printf
 * @param format 格式化字符串
 * @param ... 参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_printf(const char *format, ...)
{
    va_list args;
    int     retval;
    va_start(args, format);
    retval = vmm_cvprintf(NULL, format, args);
    va_end(args);
    return retval;
}

/**
 * @brief 初始化阶段格式化输出
 * @param format 格式化字符串
 * @param ... 参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_init_printf(const char *format, ...)
{
    va_list args;
    int     retval;
    va_start(args, format);
    vmm_cvprintf(NULL, "INIT: ", args);
    retval = vmm_cvprintf(NULL, format, args);
    va_end(args);
    return retval;
}

/**
 * @brief 字符设备十六进制转储输出
 * @param cdev 字符设备指针
 * @param print_base_addr 是否打印基地址标志
 * @param data 用户自定义数据指针
 * @param len 数据长度
 */
void vmm_cdev_hexdump(vmm_char_device_t *cdev, uint64_t print_base_addr, void *data, uint64_t len)
{
    uint64_t i;

    if (!data || !len) {
        return;
    }

    for (i = 0; i < len; i++) {
        if ((i & 0xF) == 0) {
            vmm_cdev_printf(cdev, "%016" PRIx64 "  %02x", (print_base_addr + i), ((uint8_t *)data)[i]);
        } else if ((i & 0xF) == 0x8) {
            vmm_cdev_printf(cdev, "  %02x", ((uint8_t *)data)[i]);
        } else if ((i & 0xF) == 0xF) {
            vmm_cdev_printf(cdev, " %02x\n", ((uint8_t *)data)[i]);
        } else {
            vmm_cdev_printf(cdev, " %02x", ((uint8_t *)data)[i]);
        }
    }
}

/**
 * @brief 将版本信息输出到字符设备
 * @param cdev 字符设备指针
 */
void vmm_cdev_print_version(vmm_char_device_t *cdev)
{
#ifdef VMM_VERSION_GITDESC
    vmm_cdev_printf(cdev, "%s %s (%s %s)\n", VMM_NAME, VMM_VERSION_GITDESC, __DATE__, __TIME__);
#else
    vmm_cdev_printf(cdev, "%s v%d.%d.%d (%s %s)\n", VMM_NAME, VMM_VERSION_MAJOR, VMM_VERSION_MINOR, VMM_VERSION_RELEASE, __DATE__, __TIME__);
#endif
}

/**
 * @brief 带日志级别的格式化输出
 * @param level 中断触发级别
 * @param prefix 前缀长度
 * @param format 格式化字符串
 * @param args 参数数组指针
 * @return 打印的字符数
 */
static int vmm_level_printf(enum vmm_print_level level, const char *prefix, const char *format, va_list args)
{
    int                      retval = 0;
    vmm_char_device_t *const cdev   = m_stdio_ctrl.dev;

    if (vmm_stdio_loglevel() >= level) {
        retval = vmm_cdev_printf(cdev, "%s%s %s%s", _log_prefixes[level], VMM_LOG_COLOR_RESET, (prefix) ? prefix : "", (prefix) ? ": " : "");
        retval += vmm_cvprintf(cdev, format, args);
    }

    return retval;
}

/**
 * @brief lprintf
 * @param level 中断触发级别
 * @param prefix 前缀长度
 * @param format 格式化字符串
 * @param ... 参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_lprintf(enum vmm_print_level level, const char *prefix, const char *format, ...)
{
    int     retval;
    va_list args;
    va_start(args, format);
    retval = vmm_level_printf(level, prefix, format, args);
    va_end(args);
    return retval;
}

/**
 * @brief   恐慌
 * @param format 格式化字符串
 * @param ... 参数
 * @return 打印的字符数
 */
void __noreturn __vmm_panic(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vmm_level_printf(VMM_LOGLEVEL_EMERGENCY, NULL, format, args);
    va_end(args);
    vmm_hang();
}

/**
 * @brief scanchars
 * @param cdev 字符设备指针
 * @param ch 字符值
 * @param num_ch 数量
 * @param block 块设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_scanchars(vmm_char_device_t *cdev, char *ch, uint32_t num_ch, bool block)
{
    int i;
    int rc;

    if (!ch || !num_ch) {
        return VMM_ERR_FAIL;
    }

    if (m_stdio_init_done) {
        if (cdev) {
            rc = (vmm_char_device_doread(cdev, (uint8_t *)ch, num_ch, NULL, block) ? VMM_OK : VMM_ERR_FAIL);
        } else {
            for (i = 0; i < num_ch; i++) {
                if (!block) {
                    rc = arch_default_terminal_getc((uint8_t *)&ch[i]);

                    if (rc) {
                        break;
                    }

                    continue;
                }

                while (arch_default_terminal_getc((uint8_t *)&ch[i])) {
                    vmm_scheduler_yield();
                }
            }
        }
    } else {
        for (i = 0; i < num_ch; i++) {
            ch[i] = '\0';
        }

        rc = VMM_OK;
    }

    return rc;
}

/**
 * @brief cgetc
 * @param cdev 字符设备指针
 * @param lecho 是否本地回显标志
 * @return 成功返回VMM_OK，失败返回错误码
 */
char vmm_cgetc(vmm_char_device_t *cdev, bool lecho)
{
    char ch = 0;
    vmm_scanchars(cdev, &ch, 1, TRUE);

    if (ch == '\r') {
        ch = '\n';
    }

    if (lecho && vmm_is_printable(ch)) {
        vmm_cdev_putc(cdev, ch);
    }

    return ch;
}

/**
 * @brief getc
 * @param lecho 是否本地回显标志
 * @return 获取到的值，失败返回错误码
 */
char vmm_getc(bool lecho)
{
    return vmm_cgetc(m_stdio_ctrl.dev, lecho);
}

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
char *vmm_cgets(vmm_char_device_t *cdev, char *s, int maxwidth, char endchar, struct vmm_history *history, bool lecho)
{
    char ch;
    char ch1;
    bool add_ch;
    bool del_ch;
    bool to_left;
    bool to_right;
    bool to_start;
    bool to_end;
    uint32_t ite;
    uint32_t pos = 0;
    uint32_t count = 0;
    int prev;
    int hist_cur = 0;

    if (!s) {
        return NULL;
    }

    if (history) {
        hist_cur = history->tail;
        maxwidth = (maxwidth < history->width) ? maxwidth : history->width;
    }

    while (1) {
        to_left  = FALSE;
        to_right = FALSE;
        to_start = FALSE;
        to_end   = FALSE;
        add_ch   = FALSE;
        del_ch   = FALSE;

        if ((ch = vmm_cgetc(cdev, lecho)) == endchar) {
            break;
        }

        /* Note: we have to process all the required
         * ANSI escape seqences for special keyboard keys */
        if (vmm_is_printable(ch)) {
            add_ch = TRUE;
        } else if (ch == '\e') { /* Escape character */
            vmm_scanchars(cdev, &ch, 1, TRUE);
            vmm_scanchars(cdev, &ch1, 1, TRUE);

            if (ch == '[') {
                if (history && (ch1 == 'A')) { /* Up Key */
                    prev = (hist_cur == 0) ? (history->length - 1) : (hist_cur - 1);

                    if (history->table[prev][0]) {
                        s[count] = '\0';
                        strlcpy(history->table[hist_cur], s, maxwidth);

                        /* First erase the current line */
                        if (pos > 0) {
                            vmm_cdev_printf(cdev, "\e[%dD", pos);

                            for (ite = 0; ite <= count; ite++) {
                                vmm_cdev_putc(cdev, ' ');
                            }

                            vmm_cdev_printf(cdev, "\e[%dD", count + 1);
                        }

                        /* Write the prev line */
                        pos   = 0;
                        count = 0;

                        for (; history->table[prev][pos] != '\0'; pos++, count++) {
                            vmm_cdev_putc(cdev, history->table[prev][pos]);
                            s[pos] = history->table[prev][pos];
                        }

                        hist_cur = prev;
                    }
                } else if (history && (ch1 == 'B')) { /* Down Key */
                    if (hist_cur != history->tail) {
                        s[count] = '\0';
                        strlcpy(history->table[hist_cur], s, maxwidth);
                        hist_cur = (hist_cur == (history->length - 1)) ? 0 : (hist_cur + 1);

                        /* First erase the current line */
                        if (pos > 0) {
                            vmm_cdev_printf(cdev, "\e[%dD", pos);

                            for (ite = 0; ite <= count; ite++) {
                                vmm_cdev_putc(cdev, ' ');
                            }

                            vmm_cdev_printf(cdev, "\e[%dD", count + 1);
                        }

                        /* Write the next line */
                        pos   = 0;
                        count = 0;

                        for (; history->table[hist_cur][pos] != '\0'; pos++, count++) {
                            vmm_cdev_putc(cdev, history->table[hist_cur][pos]);
                            s[pos] = history->table[hist_cur][pos];
                        }
                    }
                } else if (ch1 == 'C') { /* Right Key */
                    to_right = TRUE;
                } else if (ch1 == 'D') { /* Left Key */
                    to_left = TRUE;
                } else if (ch1 == 'H') { /* Home Key */
                    to_start = TRUE;
                } else if (ch1 == 'F') { /* End Key */
                    to_end = TRUE;
                } else if (ch1 == '3') {
                    vmm_scanchars(cdev, &ch, 1, TRUE);

                    if (ch == '~') { /* Delete Key */
                        if (pos < count) {
                            to_right = TRUE;
                            del_ch   = TRUE;
                        }
                    }
                }
            } else if (ch == 'O') {
                if (ch1 == 'H') {        /* Home Key */
                    to_start = TRUE;
                } else if (ch1 == 'F') { /* End Key */
                    to_end = TRUE;
                }
            }
        } else if ((ch == 127) || (ch == '\b')) { /* Delete character */
            if (pos > 0) {
                del_ch = TRUE;
            }
        }

        if (to_left) {
            if (pos > 0) {
                vmm_cdev_puts(cdev, "\e[D");
                pos--;
            }
        }

        if (to_right) {
            if (pos < count) {
                vmm_cdev_puts(cdev, "\e[C");
                pos++;
            }
        }

        if (to_start) {
            if (pos > 0) {
                vmm_cdev_printf(cdev, "\e[%dD", pos);
            }

            pos = 0;
        }

        if (to_end) {
            if (pos < count) {
                vmm_cdev_printf(cdev, "\e[%dC", count - pos);
            }

            pos = count;
        }

        if (add_ch) {
            /* Add the character till maxwidth is not reached */
            if (count < maxwidth) {
                for (ite = 0; ite < (count - pos); ite++) {
                    s[count - ite] = s[(count - 1) - ite];
                }

                for (ite = pos; ite < count; ite++) {
                    vmm_cdev_putc(cdev, s[ite + 1]);
                }

                for (ite = pos; ite < count; ite++) {
                    vmm_cdev_puts(cdev, "\e[D");
                }

                s[pos] = ch;
                count++;
                pos++;
            } else {
                /* Erase the printed character otherwise */
                vmm_cdev_printf(cdev, "\e[D \e[D");
            }
        }

        if (del_ch) {
            if (pos > 0) {
                for (ite = pos; ite < count; ite++) {
                    s[ite - 1] = s[ite];
                }

                s[count] = '\0';
                pos--;
                count--;
            }

            vmm_cdev_puts(cdev, "\e[D");

            for (ite = pos; ite < count; ite++) {
                vmm_cdev_putc(cdev, s[ite]);
            }

            vmm_cdev_putc(cdev, ' ');

            for (ite = pos; ite <= count; ite++) {
                vmm_cdev_puts(cdev, "\e[D");
            }
        }
    }

    s[count] = '\0';

    if (history) {
        bool duplicate = FALSE;
        prev           = (history->tail == 0) ? (history->length - 1) : (history->tail - 1);

        if (history->table[prev][0]) {
            duplicate = !(strcmp(s, history->table[prev]));
        }

        if (!duplicate) {
            strlcpy(history->table[history->tail], s, maxwidth);
        }

        if (!duplicate && (count > 0)) {
            history->tail = (history->tail == (history->length - 1)) ? 0 : (history->tail + 1);
        } else {
            history->table[history->tail][0] = '\0';
        }
    }

    return s;
}

/**
 * @brief gets
 * @param s 字符串或数据指针
 * @param maxwidth 最大宽度值
 * @param endchar 结束字符
 * @param history 历史记录缓冲区指针
 * @param lecho 是否本地回显标志
 * @return 目标对象指针，不存在返回NULL
 */
char *vmm_gets(char *s, int maxwidth, char endchar, struct vmm_history *history, bool lecho)
{
    return vmm_cgets(m_stdio_ctrl.dev, s, maxwidth, endchar, history, lecho);
}

/**
 * @brief 标准IO 设备
 * @return 目标对象指针，不存在返回NULL
 */
vmm_char_device_t *vmm_stdio_device(void)
{
    return m_stdio_ctrl.dev;
}

/**
 * @brief 切换标准输入输出设备
 * @param cdev 字符设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_stdio_change_device(vmm_char_device_t *cdev)
{
    if (!cdev) {
        return VMM_ERR_FAIL;
    }

    vmm_spin_lock(&m_stdio_ctrl.lock);
    m_stdio_ctrl.dev = cdev;
    vmm_spin_unlock(&m_stdio_ctrl.lock);

    return VMM_OK;
}

/**
 * @brief 获取或设置标准IO日志级别
 * @return 成功返回VMM_OK，失败返回错误码
 */
long vmm_stdio_loglevel(void)
{
    return arch_atomic_read(&m_stdio_ctrl.loglevel);
}

/**
 * @brief 修改标准IO日志级别
 * @param loglevel 日志级别
 */
void vmm_stdio_change_loglevel(long loglevel)
{
    arch_atomic_write(&m_stdio_ctrl.loglevel, loglevel);
}

/* size of early buffer.
 * This should be enough to hold 80x25 characters
 */
#define EARLY_BUF_SZ 2048
static uint32_t __initdata stdio_early_count = 0;
static char __initdata     stdio_early_buffer[EARLY_BUF_SZ];

/**
 * @brief 架构默认终端早期字符输出
 * @param ch 字符值
 * @return 成功返回VMM_OK，失败返回错误码
 */
void __weak __init arch_default_terminal_early_putc(uint8_t ch)
{
    if (stdio_early_count < EARLY_BUF_SZ) {
        stdio_early_buffer[stdio_early_count] = ch;
        stdio_early_count++;
    }
}

/**
 * @brief 刷新早期参数的缓冲区
 * @return 成功返回VMM_OK，失败返回错误码
 */
static void __init flush_early_buffer(void)
{
    int i;

    if (!m_stdio_init_done) {
        return;
    }

    for (i = 0; i < stdio_early_count; i++) {
        vmm_putc(stdio_early_buffer[i]);
    }
}

/**
 * @brief 初始化标准IO
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __init vmm_stdio_init(void)
{
    int rc;

    /* Reset memory of control structure */
    memset(&m_stdio_ctrl, 0, sizeof(m_stdio_ctrl));

    /* Initialize loglevel */
    ARCH_ATOMIC_INIT(&m_stdio_ctrl.loglevel, CONFIG_LOG_LEVEL);

    /* Initialize lock */
    INIT_SPIN_LOCK(&m_stdio_ctrl.lock);

    /* Set current device to NULL */
    m_stdio_ctrl.dev = NULL;

    /* Initialize default serial terminal */
    if ((rc = arch_default_terminal_init())) {
        return rc;
    }

    /* Update init done flag */
    m_stdio_init_done = TRUE;

    /* Flush early buffer */
    flush_early_buffer();

    return VMM_OK;
}
