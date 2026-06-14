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
 * @file vmm_error.h
 * @author Anup Patel (anup@brainfault.org)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief VMM错误码头文件
 */
#ifndef _VMM_ERR_RROR_H__
#define _VMM_ERR_RROR_H__

#include "vmm_types.h"

#define VMM_OK              0
#define VMM_ERR_FAIL           -1
#define VMM_ERR_UNKNOWN        -2
#define VMM_ERR_NOTAVAIL       -3
#define VMM_ERR_ALREADY        -4
#define VMM_ERR_INVALID        -5
#define VMM_ERR_OVERFLOW       -6
#define VMM_ERR_NOMEM          -7
#define VMM_ERR_NODEV          -8
#define VMM_ERR_BUSY           -9
#define VMM_ERR_EXIST          -10
#define VMM_ERR_TIMEDOUT       -11
#define VMM_ERR_ACCESS         -12
#define VMM_ERR_NOEXEC         -13
#define VMM_ERR_NOENT          -14
#define VMM_ERR_NOSYS          -15
#define VMM_ERR_IO             -16
#define VMM_ERR_TIME           -17
#define VMM_ERR_RANGE          -18
#define VMM_ERR_ILSEQ          -19
#define VMM_ERR_OPNOTSUPP      -20
#define VMM_ERR_NOSPC          -21
#define VMM_ERR_NODATA         -22
#define VMM_ERR_FAULT          -23
#define VMM_ERR_NXIO           -24
#define VMM_ERR_PROTONOSUPPORT -25
#define VMM_ERR_PROBE_DEFER    -26
#define VMM_ERR_SHUTDOWN       -27
#define VMM_ERR_REMOTEIO       -28
#define VMM_ERR_INPROGRESS     -29
#define VMM_ERR_ROFS           -30 /* Read-only file system */
#define VMM_ERR_BADMSG         -31 /* Not a data message */
#define VMM_ERR_UCLEAN         -32 /* Structure needs cleaning */
#define VMM_ERR_NOTSUPP        -33
#define VMM_ERR_AGAIN          -34
#define VMM_ERR_PROTO          -35 /* Protocol error */

#define VMM_MAX_ERRNO       4095

#define VMM_IS_ERR_VALUE(x) ((x) && ((uint64_t)(x) <= (uint64_t)VMM_MAX_ERRNO))

static inline void *VMM_ERR_RR_PTR(long error)
{
    if (0 < (-error) && (-error) < VMM_MAX_ERRNO) {
        return (void *)(-error);
    }

    return (void *)0;
}

static inline long VMM_PTR_ERR(const void *ptr)
{
    return (ptr) ? -((long)ptr) : VMM_ERR_FAIL;
}

static inline long VMM_IS_ERR(const void *ptr)
{
/**
 * @brief 判断指针值是否为错误码
 * @param (uint64_t 64位无符号整数
 * @return 成功返回VMM_OK，失败返回错误码
 */
    return VMM_IS_ERR_VALUE((uint64_t)ptr);
}

static inline long VMM_IS_ERR_OR_NULL(const void *ptr)
{
    return !ptr || VMM_IS_ERR_VALUE((uint64_t)ptr);
}

static inline void *VMM_ERR_RR_CAST(const void *ptr)
{
    /* cast away the const */
    return (void *)ptr;
}

static inline int VMM_PTR_RET(const void *ptr)
{
    if (VMM_IS_ERR(ptr)) {
/**
 * @brief 将错误码编码为指针值
 * @param ptr 通用指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
        return VMM_PTR_ERR(ptr);
    } else {
        return 0;
    }
}

#endif
