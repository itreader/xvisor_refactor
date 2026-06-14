/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file vmm_resource.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief 任意资源（含I/O内存和I/O端口）管理
 * host IO space and host memory space.
 *
 * This header has been largely adapted from Linux sources:
 * commit 97bf6af1f928216fd6c5a66e8a57bfa95a659672
 * Linux 3.19-rc1
 *
 *  linux/include/linux/ioport.h
 *
 * ioport.h Definitions of routines for detecting, reserving and
 *      allocating system resources.
 *
 * Authors: Linus Torvalds
 */

#ifndef __VMM_RESOURCE_H__
#define __VMM_RESOURCE_H__

#include <vmm_types.h>

struct vmm_device;
struct vmm_resource;

typedef struct vmm_device   vmm_device_t;
typedef struct vmm_resource vmm_resource_t;

/*
 * Resources are tree-like, allowing
 * nesting etc..
 */
/**
 * @brief 资源结构，描述I/O内存或I/O端口资源的地址范围和标志
 */
struct vmm_resource {
    resource_size_t      start; /**< 起始 */
    resource_size_t      end; /**< 结束 */
    const char          *name; /**< 名称 */
    uint64_t             flags; /**< 标志位 */
    struct vmm_resource *parent; /**< 父节点 */
    struct vmm_resource *sibling; /**< 兄弟节点 */
    struct vmm_resource *child; /**< 子节点 */
};

/*
 * IO resources have these defined flags.
 */
#define VMM_IORESOURCE_BITS             0x000000ff /* Bus-specific bits */

#define VMM_IORESOURCE_TYPE_BITS        0x00001f00 /* Resource type */
#define VMM_IORESOURCE_IO               0x00000100 /* PCI/ISA I/O ports */
#define VMM_IORESOURCE_MEM              0x00000200
#define VMM_IORESOURCE_REG              0x00000300 /* Register offsets */
#define VMM_IORESOURCE_IRQ              0x00000400
#define VMM_IORESOURCE_DMA              0x00000800
#define VMM_IORESOURCE_BUS              0x00001000

#define VMM_IORESOURCE_PREFETCH         0x00002000 /* No side effects */
#define VMM_IORESOURCE_READONLY         0x00004000
#define VMM_IORESOURCE_CACHEABLE        0x00008000
#define VMM_IORESOURCE_RANGELENGTH      0x00010000
#define VMM_IORESOURCE_SHADOWABLE       0x00020000

#define VMM_IORESOURCE_SIZEALIGN        0x00040000 /* size indicates alignment */
#define VMM_IORESOURCE_STARTALIGN       0x00080000 /* start field is alignment */

#define VMM_IORESOURCE_MEM_64           0x00100000
#define VMM_IORESOURCE_WINDOW           0x00200000 /* forwarded by bridge */
#define VMM_IORESOURCE_MUXED            0x00400000 /* Resource is software muxed */

#define VMM_IORESOURCE_EXCLUSIVE        0x08000000 /* Userland may not map this resource */
#define VMM_IORESOURCE_DISABLED         0x10000000
#define VMM_IORESOURCE_UNSET            0x20000000 /* No address assigned yet */
#define VMM_IORESOURCE_AUTO             0x40000000
#define VMM_IORESOURCE_BUSY             0x80000000 /* Driver has marked this resource busy */

/* PnP IRQ specific bits (VMM_IORESOURCE_BITS) */
#define VMM_IORESOURCE_IRQ_HIGHEDGE     (1 << 0)
#define VMM_IORESOURCE_IRQ_LOWEDGE      (1 << 1)
#define VMM_IORESOURCE_IRQ_HIGHLEVEL    (1 << 2)
#define VMM_IORESOURCE_IRQ_LOWLEVEL     (1 << 3)
#define VMM_IORESOURCE_IRQ_SHAREABLE    (1 << 4)
#define VMM_IORESOURCE_IRQ_OPTIONAL     (1 << 5)

/* PnP DMA specific bits (VMM_IORESOURCE_BITS) */
#define VMM_IORESOURCE_DMA_TYPE_MASK    (3 << 0)
#define VMM_IORESOURCE_DMA_8BIT         (0 << 0)
#define VMM_IORESOURCE_DMA_8AND16BIT    (1 << 0)
#define VMM_IORESOURCE_DMA_16BIT        (2 << 0)

#define VMM_IORESOURCE_DMA_MASTER       (1 << 2)
#define VMM_IORESOURCE_DMA_BYTE         (1 << 3)
#define VMM_IORESOURCE_DMA_WORD         (1 << 4)

#define VMM_IORESOURCE_DMA_SPEED_MASK   (3 << 6)
#define VMM_IORESOURCE_DMA_COMPATIBLE   (0 << 6)
#define VMM_IORESOURCE_DMA_TYPEA        (1 << 6)
#define VMM_IORESOURCE_DMA_TYPEB        (2 << 6)
#define VMM_IORESOURCE_DMA_TYPEF        (3 << 6)

/* PnP memory I/O specific bits (VMM_IORESOURCE_BITS) */
#define VMM_IORESOURCE_MEM_WRITEABLE    (1 << 0) /* dup: VMM_IORESOURCE_READONLY */
#define VMM_IORESOURCE_MEM_CACHEABLE    (1 << 1) /* dup: VMM_IORESOURCE_CACHEABLE */
#define VMM_IORESOURCE_MEM_RANGELENGTH  (1 << 2) /* dup: VMM_IORESOURCE_RANGELENGTH */
#define VMM_IORESOURCE_MEM_TYPE_MASK    (3 << 3)
#define VMM_IORESOURCE_MEM_8BIT         (0 << 3)
#define VMM_IORESOURCE_MEM_16BIT        (1 << 3)
#define VMM_IORESOURCE_MEM_8AND16BIT    (2 << 3)
#define VMM_IORESOURCE_MEM_32BIT        (3 << 3)
#define VMM_IORESOURCE_MEM_SHADOWABLE   (1 << 5) /* dup: VMM_IORESOURCE_SHADOWABLE */
#define VMM_IORESOURCE_MEM_EXPANSIONROM (1 << 6)

/* PnP I/O specific bits (VMM_IORESOURCE_BITS) */
#define VMM_IORESOURCE_IO_16BIT_ADDR    (1 << 0)
#define VMM_IORESOURCE_IO_FIXED         (1 << 1)

/* PCI ROM control bits (VMM_IORESOURCE_BITS) */
#define VMM_IORESOURCE_ROM_ENABLE       (1 << 0) /* ROM is enabled, same as PCI_ROM_ADDRESS_ENABLE */
#define VMM_IORESOURCE_ROM_SHADOW       (1 << 1) /* ROM is copy at C000:0 */
#define VMM_IORESOURCE_ROM_COPY         (1 << 2) /* ROM is alloc'd copy, resource field overlaid */
#define VMM_IORESOURCE_ROM_BIOS_COPY    (1 << 3) /* ROM is BIOS copy, resource field overlaid */

/* PCI control bits.  Shares VMM_IORESOURCE_BITS with above PCI ROM.  */
#define VMM_IORESOURCE_PCI_FIXED        (1 << 4) /* Do not move resource */

/* helpers to define resources */
#define DEFINE_RES_NAMED(_start, _size, _name, _flags) \
    {                                                  \
        .start = (_start),                             \
        .end   = (_start) + (_size) - 1,               \
        .name  = (_name),                              \
        .flags = (_flags),                             \
    }

#define DEFINE_RES_IO_NAMED(_start, _size, _name)  DEFINE_RES_NAMED((_start), (_size), (_name), VMM_IORESOURCE_IO)
#define DEFINE_RES_IO(_start, _size)               DEFINE_RES_IO_NAMED((_start), (_size), NULL)

#define DEFINE_RES_MEM_NAMED(_start, _size, _name) DEFINE_RES_NAMED((_start), (_size), (_name), VMM_IORESOURCE_MEM)
#define DEFINE_RES_MEM(_start, _size)              DEFINE_RES_MEM_NAMED((_start), (_size), NULL)

#define DEFINE_RES_IRQ_NAMED(_irq, _name)          DEFINE_RES_NAMED((_irq), 1, (_name), VMM_IORESOURCE_IRQ)
#define DEFINE_RES_IRQ(_irq)                       DEFINE_RES_IRQ_NAMED((_irq), NULL)

#define DEFINE_RES_DMA_NAMED(_dma, _name)          DEFINE_RES_NAMED((_dma), 1, (_name), VMM_IORESOURCE_DMA)
#define DEFINE_RES_DMA(_dma)                       DEFINE_RES_DMA_NAMED((_dma), NULL)

/* PC/ISA/whatever - the normal PC address spaces: IO and memory */
extern vmm_resource_t vmm_hostio_resource;
extern vmm_resource_t vmm_hostmem_resource;

/**
 * @brief 检查请求的资源是否与已有资源冲突
 * @param root 根节点指针
 * @param new 新值
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_resource_t *vmm_request_resource_conflict(vmm_resource_t *root, vmm_resource_t *new);

/**
 * @brief 请求 资源
 * @param root 根节点指针
 * @param new 新值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_request_resource(vmm_resource_t *root, vmm_resource_t *new);

/**
 * @brief 释放资源
 * @param new 新值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_release_resource(vmm_resource_t *new);

/**
 * @brief 释放子资源
 * @param new 新值
 */
void vmm_release_child_resources(vmm_resource_t *new);

/**
 * @brief 遍历系统内存区域
 * @param start_pfn 起始页帧号
 * @param nr_pages 页数量
 * @param arg 参数值
 * @param (*func 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_walk_system_ram_range(uint64_t start_pfn, uint64_t nr_pages, void *arg, int (*func)(uint64_t, uint64_t, void *));

/**
 * @brief 遍历系统内存资源
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param end 结束位置或结束地址
 * @param arg 参数值
 * @param (*func 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_walk_system_ram_res(uint64_t start, uint64_t end, void *arg, int (*func)(uint64_t, uint64_t, void *));

/**
 * @brief 遍历主机内存资源树
 * @param name 目标对象的名称
 * @param flags 标志位
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param end 结束位置或结束地址
 * @param arg 参数值
 * @param (*func 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_walk_hostmem_res(char *name, uint64_t flags, uint64_t start, uint64_t end, void *arg, int (*func)(uint64_t, uint64_t, void *));

/**
 * @brief 遍历资源树下的每个资源节点并调用回调函数
 */
int vmm_walk_tree_res(
    vmm_resource_t *root, void *arg, int (*func)(const char *name, uint64_t start, uint64_t end, uint64_t flags, int level, void *arg));

/**
 * @brief 检查资源插入是否与已有资源冲突
 * @param parent 父设备树节点
 * @param new 新值
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_resource_t *vmm_insert_resource_conflict(vmm_resource_t *parent, vmm_resource_t *new);

/**
 * @brief 插入 资源
 * @param parent 父设备树节点
 * @param new 新值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_insert_resource(vmm_resource_t *parent, vmm_resource_t *new);

/**
 * @brief 扩展资源以容纳新插入的子资源
 * @param root 根节点指针
 * @param new 新值
 */
void vmm_insert_resource_expand_to_fit(vmm_resource_t *root, vmm_resource_t *new);

/**
 * @brief 在资源树中分配指定范围和对齐的空闲槽位，若已分配则重新分配新大小
 */
int vmm_allocate_resource(
    vmm_resource_t *root, vmm_resource_t *new, resource_size_t size, resource_size_t min, resource_size_t max, resource_size_t align,
    resource_size_t (*alignf)(void *, const vmm_resource_t *, resource_size_t, resource_size_t), void *alignf_data);

/**
 * @brief 查找 资源
 * @param root 根节点指针
 * @param start 遍历起始节点（NULL表示从头开始）
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_resource_t *vmm_lookup_resource(vmm_resource_t *root, resource_size_t start);

/**
 * @brief 调整资源范围
 * @param res 资源结构体指针
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_adjust_resource(vmm_resource_t *res, resource_size_t start, resource_size_t size);

/**
 * @brief 获取资源对齐值
 * @param res 资源结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
resource_size_t vmm_resource_alignment(vmm_resource_t *res);

static inline resource_size_t vmm_resource_size(const vmm_resource_t *res)
{
    return res->end - res->start + 1;
}

static inline uint64_t vmm_resource_type(const vmm_resource_t *res)
{
    return res->flags & VMM_IORESOURCE_TYPE_BITS;
}

/* True iff r1 completely contains r2 */
static inline bool vmm_resource_contains(vmm_resource_t *r1, vmm_resource_t *r2)
{
    if (vmm_resource_type(r1) != vmm_resource_type(r2)) {
        return FALSE;
    }

    if ((r1->flags & VMM_IORESOURCE_UNSET) || (r2->flags & VMM_IORESOURCE_UNSET)) {
        return FALSE;
    }

    return r1->start <= r2->start && r1->end >= r2->end;
}

/**
 * @brief 预留并拆分资源区域
 * @param root 根节点指针
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param end 结束位置或结束地址
 * @param name 目标对象的名称
 */
void vmm_reserve_region_with_split(vmm_resource_t *root, resource_size_t start, resource_size_t end, const char *name);

/**
 * @brief   请求 区域
 * @param parent 父设备树节点
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param n 起始位置编号
 * @param name 目标对象的名称
 * @param flags 标志位
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_resource_t *__vmm_request_region(vmm_resource_t *parent, resource_size_t start, resource_size_t n, const char *name, int flags);

/* Convenience shorthand with allocation */
#define vmm_request_region(start, n, name)               __vmm_request_region(&vmm_hostio_resource, (start), (n), (name), 0)
#define vmm_request_muxed_region(start, n, name)         __vmm_request_region(&vmm_hostio_resource, (start), (n), (name), VMM_IORESOURCE_MUXED)
#define __vmm_request_mem_region(start, n, name, excl)   __vmm_request_region(&vmm_hostmem_resource, (start), (n), (name), excl)
#define vmm_request_mem_region(start, n, name)           __vmm_request_region(&vmm_hostmem_resource, (start), (n), (name), 0)
#define vmm_request_mem_region_exclusive(start, n, name) __vmm_request_region(&vmm_hostmem_resource, (start), (n), (name), VMM_IORESOURCE_EXCLUSIVE)
#define vmm_rename_region(region, newname)                                                                                                           \
    do {                                                                                                                                             \
        (region)->name = (newname);                                                                                                                  \
    } while (0)

/**
 * @brief   检查 区域
 * @param resource_size_t 资源大小值
 * @param resource_size_t 资源大小值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int __vmm_check_region(vmm_resource_t *, resource_size_t, resource_size_t);

/**
 * @brief   释放IO内存区域
 * @param resource_size_t 资源大小值
 * @param resource_size_t 资源大小值
 */
void __vmm_release_region(vmm_resource_t *, resource_size_t, resource_size_t);

/* Compatibility cruft */
#define vmm_release_region(start, n)     __vmm_release_region(&vmm_hostio_resource, (start), (n))
#define vmm_check_mem_region(start, n)   __vmm_check_region(&vmm_hostmem_resource, (start), (n))
#define vmm_release_mem_region(start, n) __vmm_release_region(&vmm_hostmem_resource, (start), (n))

#ifdef CONFIG_MEMORY_HOTREMOVE
/**
 * @brief 释放可调整内存区域
 * @param resource_size_t 资源大小值
 * @param resource_size_t 资源大小值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_release_mem_region_adjustable(vmm_resource_t *, resource_size_t, resource_size_t);
#endif

/*
 * Managed region resource
 */

/**
 * @brief 使用托管方式请求设备资源
 * @param dev 设备结构体指针
 * @param root 根节点指针
 * @param new 新值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_devm_request_resource(vmm_device_t *dev, vmm_resource_t *root, vmm_resource_t *new);

/**
 * @brief 释放托管设备资源
 * @param dev 设备结构体指针
 * @param new 新值
 */
void vmm_devm_release_resource(vmm_device_t *dev, vmm_resource_t *new);

#define vmm_devm_request_region(dev, start, n, name)     __vmm_devm_request_region(dev, &vmm_hostio_resource, (start), (n), (name))
#define vmm_devm_request_mem_region(dev, start, n, name) __vmm_devm_request_region(dev, &vmm_hostmem_resource, (start), (n), (name))

/**
 * @brief 使用托管方式请求I/O或内存区域
 * @param dev 设备结构体指针
 * @param parent 父设备树节点
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param n 起始位置编号
 * @param name 目标对象的名称
 */
vmm_resource_t *__vmm_devm_request_region(vmm_device_t *dev, vmm_resource_t *parent, resource_size_t start, resource_size_t n, const char *name);

#define vmm_devm_release_region(dev, start, n)     __vmm_devm_release_region(dev, &vmm_hostio_resource, (start), (n))
#define vmm_devm_release_mem_region(dev, start, n) __vmm_devm_release_region(dev, &vmm_hostmem_resource, (start), (n))

/**
 * @brief 释放托管的I/O或内存区域
 * @param dev 设备结构体指针
 * @param parent 父设备树节点
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param n 起始位置编号
 */
void __vmm_devm_release_region(vmm_device_t *dev, vmm_resource_t *parent, resource_size_t start, resource_size_t n);

/**
 * @brief 主机内存映射完整性检查
 * @param addr 地址值
 * @param size 数据大小（字节数）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_hostmem_map_sanity_check(resource_size_t addr, uint64_t size);

/**
 * @brief 检查主机内存资源是否为独占类型
 * @param addr 地址值
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_hostmem_is_exclusive(uint64_t addr);

/* True if any part of r1 overlaps r2 */
static inline bool vmm_resource_overlaps(vmm_resource_t *r1, vmm_resource_t *r2)
{
    return (r1->start <= r2->end && r1->end >= r2->start);
}

#endif
