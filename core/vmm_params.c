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
 * @brief 启动时或早期参数源文件
 */

#include <libs/stringlib.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_params.h>
#include <vmm_stdio.h>
#include <vmm_types.h>

extern const vmm_setup_param_t __setup_start[], __setup_end[];

/**
 * @brief 将参数名中的短横线转换为下划线
 * @param c 字符设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static char dash2underscore(char c)
{
    if (c == '-') {
        return '_';
    }

    return c;
}

/**
 * @brief 检查参数等式是否匹配
 * @param a 参数值
 * @param b 字节值或缓冲区
 * @param n 起始位置编号
 * @return 成功返回VMM_OK，失败返回错误码
 */
static bool parameqn(const char *a, const char *b, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (dash2underscore(a[i]) != dash2underscore(b[i])) {
            return false;
        }
    }

    return true;
}

/* Check for early params. */
/**
 * @brief 解析并执行早期启动参数
 * @param param 参数结构体指针
 * @param val 待写入的值
 * @param unused 未使用的字符串参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int __init do_early_param(char *param, char *val, const char *unused)
{
    const vmm_setup_param_t *p;

    for (p = __setup_start; p < __setup_end; p++) {
        if ((p->early && parameqn(param, p->str, strlen(param))) || (strcmp(param, "console") == 0 && strcmp(p->str, "earlycon") == 0)) {
            p->setup_func(val);
        }
    }

    /* We accept everything at this stage. */
    return 0;
}

/* You can use " around spaces, but can't escape ". */
/* Hyphens and underscores equivalent in parameter names. */
/**
 * @brief 下一个 参数
 * @param args 参数数组指针
 * @param param 参数结构体指针
 * @param val 待写入的值
 * @return 成功返回目标指针，失败返回NULL
 */
static char *next_arg(char *args, char **param, char **val)
{
    uint32_t i;
    uint32_t equals = 0;
    int in_quote = 0;
    int quoted = 0;
    char    *next;

    if (*args == '"') {
        args++;
        in_quote = 1;
        quoted   = 1;
    }

    for (i = 0; args[i]; i++) {
        if (isspace(args[i]) && !in_quote) {
            break;
        }

        if (equals == 0) {
            if (args[i] == '=') {
                equals = i;
            }
        }

        if (args[i] == '"') {
            in_quote = !in_quote;
        }
    }

    *param = args;

    if (!equals) {
        *val = NULL;
    } else {
        args[equals] = '\0';
        *val         = args + equals + 1;

        /* Don't include quotes in value. */
        if (**val == '"') {
            (*val)++;

            if (args[i - 1] == '"') {
                args[i - 1] = '\0';
            }
        }

        if (quoted && args[i - 1] == '"') {
            args[i - 1] = '\0';
        }
    }

    if (args[i]) {
        args[i] = '\0';
        next    = args + i + 1;
    } else {
        next = args + i;
    }

    /* Chew up trailing spaces. */
    return skip_spaces(next);
}

/* Args looks like "foo=bar,bar2 baz=fuz wiz". */
/**
 * @brief 解析 参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int parse_args(
    const char *doing, char *args, unsigned num, int16_t min_level, int16_t max_level, int (*unknown)(char *param, char *val, const char *doing))
{
    char *param = NULL;
    char *val = NULL;

    /* Chew leading spaces */
    args = skip_spaces(args);

    while (*args) {
        int ret;

        args = next_arg(args, &param, &val);

        if (unknown) {
            ret = unknown(param, val, doing);

            if (ret) {
                return ret;
            }
        }
    }

    /* All parsed OK. */
    return 0;
}

/**
 * @brief 获取启动参数选项
 * @param str 待处理的字符串
 * @param pint 输出整数指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_get_option(char **str, int *pint)
{
    char *cur = *str;

    if (!cur || !(*cur)) {
        return 0;
    }

    *pint = strtol(cur, str, 0);

    if (cur == *str) {
        return 0;
    }

    if (**str == ',') {
        (*str)++;
        return 2;
    }

    if (**str == '-') {
        return 3;
    }

    return 1;
}

/**
 * @brief 解析早期启动选项
 * @param cmdline 命令行字符串
 * @return 获取到的值，失败返回错误码
 */
void __init vmm_parse_early_options(const char *cmdline)
{
    vmm_init_printf("early_params: %s\n", cmdline);
    parse_args("early options", (char *)cmdline, 0, 0, 0, do_early_param);
}
