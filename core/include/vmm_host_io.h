/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_host_io.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief 通用IO函数头文件
 */

#ifndef __VMM_HOST_IO_H_
#define __VMM_HOST_IO_H_

#include <arch_io.h>
#include <vmm_types.h>

#ifndef ARCH_IO_SPACE_LIMIT
#define ARCH_IO_SPACE_LIMIT 0xffff
#endif

/** Endianness related helper macros */
#define vmm_cpu_to_le16(data) arch_cpu_to_le16(data)

#define vmm_le16_to_cpu(data) arch_le16_to_cpu(data)

#define vmm_cpu_to_be16(data) arch_cpu_to_be16(data)

#define vmm_be16_to_cpu(data) arch_be16_to_cpu(data)

#define vmm_cpu_to_le32(data) arch_cpu_to_le32(data)

#define vmm_le32_to_cpu(data) arch_le32_to_cpu(data)

#define vmm_cpu_to_be32(data) arch_cpu_to_be32(data)

#define vmm_be32_to_cpu(data) arch_be32_to_cpu(data)

#define vmm_cpu_to_le64(data) arch_cpu_to_le64(data)

#define vmm_le64_to_cpu(data) arch_le64_to_cpu(data)

#define vmm_cpu_to_be64(data) arch_cpu_to_be64(data)

#define vmm_be64_to_cpu(data) arch_be64_to_cpu(data)

#if ARCH_BITS_PER_LONG == 32
#define vmm_cpu_to_le_long(__val) vmm_cpu_to_le32(__val)
#define vmm_le_long_to_cpu(__val) vmm_le32_to_cpu(__val)
#else
#define vmm_cpu_to_le_long(__val) vmm_cpu_to_le64(__val)
#define vmm_le_long_to_cpu(__val) vmm_le64_to_cpu(__val)
#endif

/**
 * @brief IO空间访问函数（假定小端序）
 */
static inline uint8_t vmm_inb(uint64_t port)
{
/**
 * @brief 架构相关IO端口字节读取
 * @param port 端口编号或端口结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
    return arch_inb(port);
}

static inline uint16_t vmm_inw(uint64_t port)
{
/**
 * @brief 架构相关IO端口字读取
 * @param port 端口编号或端口结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
    return arch_inw(port);
}

static inline uint32_t vmm_inl(uint64_t port)
{
/**
 * @brief 架构相关IO端口双字读取
 * @param port 端口编号或端口结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
    return arch_inl(port);
}

static inline void vmm_outb(uint8_t value, uint64_t port)
{
    arch_outb(value, port);
}

static inline void vmm_outw(uint16_t value, uint64_t port)
{
    arch_outw(value, port);
}

static inline void vmm_outl(uint32_t value, uint64_t port)
{
    arch_outl(value, port);
}

static inline uint8_t vmm_inb_p(uint64_t port)
{
/**
 * @brief 从I/O端口读取8位数据（带端口延迟）
 * @param port 端口编号或端口结构体指针
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_inb_p(port);
}

static inline uint16_t vmm_inw_p(uint64_t port)
{
/**
 * @brief 从I/O端口读取16位数据（带端口延迟）
 * @param port 端口编号或端口结构体指针
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_inw_p(port);
}

static inline uint32_t vmm_inl_p(uint64_t port)
{
/**
 * @brief 从I/O端口读取32位数据（带端口延迟）
 * @param port 端口编号或端口结构体指针
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_inl_p(port);
}

static inline void vmm_outb_p(uint8_t value, uint64_t port)
{
    arch_outb_p(value, port);
}

static inline void vmm_outw_p(uint16_t value, uint64_t port)
{
    arch_outw_p(value, port);
}

static inline void vmm_outl_p(uint32_t value, uint64_t port)
{
    arch_outl_p(value, port);
}

static inline void vmm_insb(uint64_t port, void *buffer, int count)
{
    arch_insb(port, buffer, count);
}

static inline void vmm_insw(uint64_t port, void *buffer, int count)
{
    arch_insw(port, buffer, count);
}

static inline void vmm_insl(uint64_t port, void *buffer, int count)
{
    arch_insl(port, buffer, count);
}

static inline void vmm_outsb(uint64_t port, const void *buffer, int count)
{
    arch_outsb(port, buffer, count);
}

static inline void vmm_outsw(uint64_t port, const void *buffer, int count)
{
    arch_outsw(port, buffer, count);
}

static inline void vmm_outsl(uint64_t port, const void *buffer, int count)
{
    arch_outsl(port, buffer, count);
}

/**
 * @brief 内存读写传统函数（假定小端序）
 */
static inline uint8_t vmm_readb(volatile void *addr)
{
/**
 * @brief 从I/O端口读取8位数据（架构相关实现）
 * @param addr 地址值
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_in_8(addr);
}

static inline void vmm_writeb(uint8_t data, volatile void *addr)
{
    arch_out_8(addr, data);
}

static inline uint16_t vmm_readw(volatile void *addr)
{
/**
 * @brief 从I/O端口读取小端16位数据
 * @param addr 地址值
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_in_le16(addr);
}

static inline void vmm_writew(uint16_t data, volatile void *addr)
{
    arch_out_le16(addr, data);
}

static inline uint32_t vmm_readl(volatile void *addr)
{
/**
 * @brief 从I/O端口读取小端32位数据
 * @param addr 地址值
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_in_le32(addr);
}

static inline void vmm_writel(uint32_t data, volatile void *addr)
{
    arch_out_le32(addr, data);
}

static inline uint64_t vmm_readq(volatile void *addr)
{
/**
 * @brief 从I/O端口读取小端64位数据
 * @param addr 地址值
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_in_le64(addr);
}

static inline void vmm_writeq(uint64_t data, volatile void *addr)
{
    arch_out_le64(addr, data);
}

static inline void vmm_readsb(volatile void *addr, void *buffer, int len)
{
    if (len) {
        uint8_t *buf = buffer;

        do {
            uint8_t x = vmm_readb(addr);
            *buf++    = x;
        } while (--len);
    }
}

static inline void vmm_readsw(volatile void *addr, void *buffer, int len)
{
    if (len) {
        uint16_t *buf = buffer;

        do {
            uint16_t x = vmm_readw(addr);
            *buf++     = x;
        } while (--len);
    }
}

static inline void vmm_readsl(volatile void *addr, void *buffer, int len)
{
    if (len) {
        uint32_t *buf = buffer;

        do {
            uint32_t x = vmm_readl(addr);
            *buf++     = x;
        } while (--len);
    }
}

static inline void vmm_writesb(volatile void *addr, const void *buffer, int len)
{
    if (len) {
        const uint8_t *buf = buffer;

        do {
            vmm_writeb(*buf++, addr);
        } while (--len);
    }
}

static inline void vmm_writesw(volatile void *addr, const void *buffer, int len)
{
    if (len) {
        const uint16_t *buf = buffer;

        do {
            vmm_writew(*buf++, addr);
        } while (--len);
    }
}

static inline void vmm_writesl(volatile void *addr, const void *buffer, int len)
{
    if (len) {
        const uint32_t *buf = buffer;

        do {
            vmm_writel(*buf++, addr);
        } while (--len);
    }
}

/**
 * @brief 内存读写宽松传统函数（假定小端序）
 */
static inline uint8_t vmm_readb_relaxed(volatile void *addr)
{
/**
 * @brief 从I/O端口读取8位数据（宽松内存序）
 * @param addr 地址值
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_in_8_relax(addr);
}

static inline void vmm_writeb_relaxed(uint8_t data, volatile void *addr)
{
    arch_out_8_relax(addr, data);
}

static inline uint16_t vmm_readw_relaxed(volatile void *addr)
{
/**
 * @brief 从I/O端口读取小端16位数据（宽松内存序）
 * @param addr 地址值
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_in_le16_relax(addr);
}

static inline void vmm_writew_relaxed(uint16_t data, volatile void *addr)
{
    arch_out_le16_relax(addr, data);
}

static inline uint32_t vmm_readl_relaxed(volatile void *addr)
{
/**
 * @brief 从I/O端口读取小端32位数据（宽松内存序）
 * @param addr 地址值
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_in_le32_relax(addr);
}

static inline void vmm_writel_relaxed(uint32_t data, volatile void *addr)
{
    arch_out_le32_relax(addr, data);
}

static inline uint64_t vmm_readq_relaxed(volatile void *addr)
{
/**
 * @brief 从I/O端口读取小端64位数据（宽松内存序）
 * @param addr 地址值
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_in_le64_relax(addr);
}

static inline void vmm_writeq_relaxed(uint64_t data, volatile void *addr)
{
    arch_out_le64_relax(addr, data);
}

/**
 * @brief 内存读写函数
 */
static inline uint8_t vmm_in_8(volatile uint8_t *addr)
{
/**
 * @brief 从I/O端口读取8位数据（架构相关实现）
 * @param addr 地址值
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_in_8(addr);
}

static inline void vmm_out_8(volatile uint8_t *addr, uint8_t data)
{
    arch_out_8(addr, data);
}

static inline uint16_t vmm_in_le16(volatile uint16_t *addr)
{
/**
 * @brief 从I/O端口读取小端16位数据
 * @param addr 地址值
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_in_le16(addr);
}

static inline void vmm_out_le16(volatile uint16_t *addr, uint16_t data)
{
    arch_out_le16(addr, data);
}

static inline uint16_t vmm_in_be16(volatile uint16_t *addr)
{
/**
 * @brief 从I/O端口读取大端16位数据
 * @param addr 地址值
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_in_be16(addr);
}

static inline void vmm_out_be16(volatile uint16_t *addr, uint16_t data)
{
    arch_out_be16(addr, data);
}

static inline uint32_t vmm_in_le32(volatile uint32_t *addr)
{
/**
 * @brief 从I/O端口读取小端32位数据
 * @param addr 地址值
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_in_le32(addr);
}

static inline void vmm_out_le32(volatile uint32_t *addr, uint32_t data)
{
    arch_out_le32(addr, data);
}

static inline uint32_t vmm_in_be32(volatile uint32_t *addr)
{
/**
 * @brief 从I/O端口读取大端32位数据
 * @param addr 地址值
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_in_be32(addr);
}

static inline void vmm_out_be32(volatile uint32_t *addr, uint32_t data)
{
    arch_out_be32(addr, data);
}

static inline uint64_t vmm_in_le64(volatile uint64_t *addr)
{
/**
 * @brief 从I/O端口读取小端64位数据
 * @param addr 地址值
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_in_le64(addr);
}

static inline void vmm_out_le64(volatile uint64_t *addr, uint64_t data)
{
    arch_out_le64(addr, data);
}

static inline uint64_t vmm_in_be64(volatile uint64_t *addr)
{
/**
 * @brief 从I/O端口读取大端64位数据
 * @param addr 地址值
 * @return 成功读取的字节数，失败返回错误码
 */
    return arch_in_be64(addr);
}

static inline void vmm_out_be64(volatile uint64_t *addr, uint64_t data)
{
    arch_out_be64(addr, data);
}

/* Bitwise memory read/write functions */
#define vmm_clrbits(type, addr, clear)         vmm_out_##type((addr), vmm_in_##type(addr) & ~(clear))

#define vmm_setbits(type, addr, set)           vmm_out_##type((addr), vmm_in_##type(addr) | (set))

#define vmm_clrsetbits(type, addr, clear, set) vmm_out_##type((addr), (vmm_in_##type(addr) & ~(clear)) | (set))

#define vmm_clrbits_be32(addr, clear)          vmm_clrbits(be32, addr, clear)
#define vmm_setbits_be32(addr, set)            vmm_setbits(be32, addr, set)
#define vmm_clrsetbits_be32(addr, clear, set)  vmm_clrsetbits(be32, addr, clear, set)

#define vmm_clrbits_le32(addr, clear)          vmm_clrbits(le32, addr, clear)
#define vmm_setbits_le32(addr, set)            vmm_setbits(le32, addr, set)
#define vmm_clrsetbits_le32(addr, clear, set)  vmm_clrsetbits(le32, addr, clear, set)

#define vmm_clrbits_be16(addr, clear)          vmm_clrbits(be16, addr, clear)
#define vmm_setbits_be16(addr, set)            vmm_setbits(be16, addr, set)
#define vmm_clrsetbits_be16(addr, clear, set)  vmm_clrsetbits(be16, addr, clear, set)

#define vmm_clrbits_le16(addr, clear)          vmm_clrbits(le16, addr, clear)
#define vmm_setbits_le16(addr, set)            vmm_setbits(le16, addr, set)
#define vmm_clrsetbits_le16(addr, clear, set)  vmm_clrsetbits(le16, addr, clear, set)

#define vmm_clrbits_8(addr, clear)             vmm_clrbits(8, addr, clear)
#define vmm_setbits_8(addr, set)               vmm_setbits(8, addr, set)
#define vmm_clrsetbits_8(addr, clear, set)     vmm_clrsetbits(8, addr, clear, set)

#endif /* __VMM_HOST_IO_H_ */
