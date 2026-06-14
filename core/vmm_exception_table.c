/**
 * Copyright (c) 2015 Himanshu Chauhan
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
 * @file vmm_exception_table.c
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief 异常表实现
 */

#include <arch_config.h>
#include <arch_sections.h>
#include <libs/libsort.h>
#include <vmm_cpumask.h>
#include <vmm_error.h>
#include <vmm_exception_table.h>
#include <vmm_host_address_space.h>
#include <vmm_per_cpu.h>
#include <vmm_stdio.h>

#ifdef ARCH_HAS_EXCEPTION_TABLE

/*
 * The exception table needs to be sorted so that the binary
 * search that we use to find entries in it works properly.
 * This is used both for the kernel exception table and for
 * the exception tables of modules that get loaded.
 */

/**
 * \brief 比较两个异常表条目
 * \param a 第一个条目指针
 * \param b 第二个条目指针
 * \return 如果a > b返回1, a < b返回-1, 相等返回0
 */
static int cmp_ex(const void *a, const void *b)
{
    const struct vmm_exception_table_entry *x = a;
    const struct vmm_exception_table_entry *y = b;

    /* avoid overflow */
    if (x->insn > y->insn) {
        return 1;
    }

    if (x->insn < y->insn) {
        return -1;
    }

    return 0;
}

/**
 * \brief 对异常表进行排序
 * \param start 异常表起始地址
 * \param finish 异常表结束地址
 */
static void sort_exception_table(struct vmm_exception_table_entry *start, struct vmm_exception_table_entry *finish)
{
    simple_sort(start, finish - start, sizeof(struct vmm_exception_table_entry), cmp_ex, NULL);
}

/*
 * Search one exception table for an entry corresponding to the
 * given instruction address, and return the address of the entry,
 * or NULL if none is found.
 * We use a binary search, and thus we assume that the table is
 * already sorted.
 */

/**
 * \brief 在异常表中搜索对应指令地址的条目
 * \param first 异常表起始条目
 * \param last 异常表结束条目
 * \param value 要搜索的指令地址
 * \return 找到的条目指针或NULL
 */
static const struct vmm_exception_table_entry *search_exception_table(
    const struct vmm_exception_table_entry *first, const struct vmm_exception_table_entry *last, uint64_t value)
{
    while (first <= last) {
        const struct vmm_exception_table_entry *mid;

        mid = ((last - first) >> 1) + first;

        /*
         * careful, the distance between value and insn
         * can be larger than MAX_LONG:
         */
        if (mid->insn < value) {
            first = mid + 1;
        } else if (mid->insn > value) {
            last = mid - 1;
        } else {
            return mid;
        }
    }

    return NULL;
}

/**
 * \brief 搜索异常表中的条目
 * \param addr 指令地址
 * \return 找到的异常表条目或NULL
 */
const struct vmm_exception_table_entry *vmm_exception_table_search(uint64_t addr)
{
    struct vmm_exception_table_entry       *start = (struct vmm_exception_table_entry *)arch_exception_table_start();
    struct vmm_exception_table_entry       *end   = (struct vmm_exception_table_entry *)arch_exception_table_end();
    const struct vmm_exception_table_entry *e;

    e = search_exception_table(start, end - 1, addr);

    return e;
}

/**
 * \brief 初始化异常表
 * \return 初始化结果，成功返回VMM_OK
 */
int __init vmm_exception_table_init(void)
{
    struct vmm_exception_table_entry *start = (struct vmm_exception_table_entry *)arch_exception_table_start();
    struct vmm_exception_table_entry *end   = (struct vmm_exception_table_entry *)arch_exception_table_end();

    /* Sort the built-in exception table */
    sort_exception_table(start, end);

    return VMM_OK;
}

#else

/**
 * \brief 搜索异常表中的条目
 * \param addr 指令地址
 * \return 找到的异常表条目或NULL
 */
const struct vmm_exception_table_entry *vmm_exception_table_search(uint64_t addr)
{
    return NULL;
}

/**
 * \brief 初始化异常表
 * \return 初始化结果，成功返回VMM_OK
 */
int __init vmm_exception_table_init(void)
{
    /* Do nothing */
    return VMM_OK;
}

#endif
