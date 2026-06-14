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
 * @file vmm_exception_table.h
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief 异常表头文件
 */
#ifndef __VMM_ERR_XCEPTION_TABLE_H
#define __VMM_ERR_XCEPTION_TABLE_H

/*
 * The exception table consists of pairs of addresses: the first is the
 * address of an instruction that is allowed to fault, and the second is
 * the address at which the program should continue.  No registers are
 * modified, so it is entirely up to the continuation code to figure out
 * what to do.
 *
 * All the routines below use bits of fixup code that are out of line
 * with the main instruction path.  This means when everything is well,
 * we don't even have to jump over them.  Further, they do not intrude
 * on our cache or tlb entries.
 */

/**
 * @brief 异常表条目，记录异常触发地址和修复处理地址
 */
struct vmm_exception_table_entry {
    uint64_t insn; /**< 允许发生异常的指令地址 */
    uint64_t fixup; /**< 异常发生后的修复入口地址 */
};

/**
 * @brief 在异常表中搜索指定地址对应的表项
 * @param addr 要搜索的地址
 * @return 成功返回异常表项指针，未找到返回NULL
 */
const struct vmm_exception_table_entry *vmm_exception_table_search(uint64_t addr);

/**
 * @brief 初始化异常表
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_exception_table_init(void);

#endif /* __VMM_ERR_XCEPTION_TABLE_H */
