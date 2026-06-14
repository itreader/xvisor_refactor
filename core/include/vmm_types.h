/**
 * Copyright (c) 2010 Himanshu Chauhan.
 *               2016 Open Wide
 *               2016 Institut de Recherche Technologique SystemX
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
 * @file vmm_types.h
 * @author Himanshu Chauhan (hchauhan@nulltrace.org)
 * @author Pavel Borzenkov (pavel.borzenkov@gmail.com)
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief Xvisor通用类型头文件
 */

#ifndef __VMM_TYPES_H__
#define __VMM_TYPES_H__

#include <arch_types.h>

/** @brief 布尔类型宏定义 */
#define TRUE  1
#define FALSE 0
#define true  TRUE
#define false FALSE
#define NULL  ((void *)0)

/** @brief DMA地址类型，基于物理地址 */
typedef physical_addr_t dma_addr_t;
/** @brief 资源地址类型，基于物理地址 */
typedef physical_addr_t resource_addr_t;
/** @brief 资源大小类型，基于物理大小 */
typedef physical_size_t resource_size_t;

/* 以下宏是POSIX.1对inttypes.h的要求，提供可移植的printf格式接口 */

/* 十进制格式：有符号整数 */
#define PRId8      "d"
#define PRId16     "d"
#define PRId32     "d"
#define PRId64     __ARCH_PRI64_PREFIX "d"
#define PRIi8      "i"
#define PRIi16     "i"
#define PRIi32     "i"
#define PRIi64     __ARCH_PRI64_PREFIX "i"

/* 十进制格式：无符号整数 */
#define PRIu8      "u"
#define PRIu16     "u"
#define PRIu32     "u"
#define PRIu64     __ARCH_PRI64_PREFIX "u"

/* 十六进制格式，小写 */
#define PRIx8      "x"
#define PRIx16     "x"
#define PRIx32     "x"
#define PRIx64     __ARCH_PRI64_PREFIX "x"

/* 十六进制格式，大写 */
#define PRIX8      "X"
#define PRIX16     "X"
#define PRIX32     "X"
#define PRIX64     __ARCH_PRI64_PREFIX "X"

/* 非标准，用于打印地址及其大小，避免针对不同架构编写冗长的测试代码 */
#define PRIADDR    "0" __ARCH_PRIADDR_DIGITS __ARCH_PRIADDR_PREFIX "X"
#define PRIADDR64  "016" __ARCH_PRI64_PREFIX "X"
#define PRISIZE    __ARCH_PRISIZE_PREFIX "u"

#define PRIPADDR   "0" __ARCH_PRIPADDR_DIGITS __ARCH_PRIPADDR_PREFIX "X"
#define PRIPADDR64 "016" __ARCH_PRI64_PREFIX "X"
#define PRIPSIZE   __ARCH_PRIPSIZE_PREFIX "u"

#endif /* __VMM_TYPES_H__ */
