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
 * @file vmm_device_emulate.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief 设备模拟框架头文件
 */
#ifndef _VMM_DEVICE_EMULATE_H__
#define _VMM_DEVICE_EMULATE_H__

#include <vmm_device_tree.h>
#include <vmm_limits.h>
#include <vmm_manager.h>
#include <vmm_spinlocks.h>

struct vmm_emulate_device;
typedef struct vmm_emulate_device vmm_emulate_device_t;

/**
 * @brief 设备模拟字节序枚举，定义大端和小端模式
 */
enum vmm_device_emulate_endianness {
    VMM_DEVICE_EMULATE_UNKNOWN_ENDIAN = 0, /**< 0 */
    VMM_DEVICE_EMULATE_NATIVE_ENDIAN  = 1, /**< 1 */
    VMM_DEVICE_EMULATE_LITTLE_ENDIAN  = 2, /**< 2 */
    VMM_DEVICE_EMULATE_BIG_ENDIAN     = 3, /**< 3 */
    VMM_DEVICE_EMULATE_MAX_ENDIAN     = 4, /**< 4 */
};

/**
 * @brief 设备模拟器接口，定义内存和IO读写的模拟回调
 */
typedef struct vmm_emulator {
    double_list_t                        head; /**< 链表头 */
    char                                 name[VMM_FIELD_NAME_SIZE]; /**< 名称 */
    const vmm_device_tree_nodeid_t *match_table; /**< 匹配表 */
    enum vmm_device_emulate_endianness   endian; /**< endian成员 */
    int (*probe)(struct vmm_guest *guest, vmm_emulate_device_t *edev, const vmm_device_tree_nodeid_t *nodeid); /**< 探测函数 */
    int (*remove)(vmm_emulate_device_t *edev); /**< 移除函数 */
    int (*reset)(vmm_emulate_device_t *edev); /**< 复位 */
    int (*sync)(vmm_emulate_device_t *edev, uint64_t val, void *v); /**< 同步 */
    int (*read8)(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t *dst); /**< read8成员 */
    int (*write8)(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t src); /**< write8成员 */
    int (*read16)(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t *dst); /**< read16成员 */
    int (*write16)(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t src); /**< write16成员 */
    int (*read32)(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst); /**< read32成员 */
    int (*write32)(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src); /**< write32成员 */
    int (*read64)(vmm_emulate_device_t *edev, physical_addr_t offset, uint64_t *dst); /**< read64成员 */
    int (*write64)(vmm_emulate_device_t *edev, physical_addr_t offset, uint64_t src); /**< write64成员 */
    int (*read_simple)(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst, uint32_t size); /**< read_simple成员 */
    int (*write_simple)(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t regmask, uint32_t regval, uint32_t size); /**< write_simple成员 */
} vmm_emulator_t;

/**
 * @brief 设备简单8位读模拟
 * @param edev 模拟设备实例指针
 * @param offset 偏移量（字节）
 * @param dst 目标缓冲区指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_simple_read8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t *dst);
/**
 * @brief 设备简单16位读模拟
 * @param edev 模拟设备实例指针
 * @param offset 偏移量（字节）
 * @param dst 目标缓冲区指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_simple_read16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t *dst);
/**
 * @brief 设备简单32位读模拟
 * @param edev 模拟设备实例指针
 * @param offset 偏移量（字节）
 * @param dst 目标缓冲区指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_simple_read32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst);
/**
 * @brief 设备简单8位写模拟
 * @param edev 模拟设备实例指针
 * @param offset 偏移量（字节）
 * @param src 源设备树节点
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_simple_write8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t src);
/**
 * @brief 设备简单16位写模拟
 * @param edev 模拟设备实例指针
 * @param offset 偏移量（字节）
 * @param src 源设备树节点
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_simple_write16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t src);
/**
 * @brief 设备简单32位写模拟
 * @param edev 模拟设备实例指针
 * @param offset 偏移量（字节）
 * @param src 源设备树节点
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_simple_write32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src);

#define VMM_DECLARE_EMULATOR_SIMPLE(EMU, NAME, MATCH, ENDIAN, PROBE, REMOVE, RESET, SYNC, READ, WRITE)                                               \
    static vmm_emulator_t EMU = {                                                                                                                    \
        .name         = NAME,                                                                                                                        \
        .match_table  = MATCH,                                                                                                                       \
        .endian       = ENDIAN,                                                                                                                      \
        .probe        = PROBE,                                                                                                                       \
        .reset        = RESET,                                                                                                                       \
        .sync         = SYNC,                                                                                                                        \
        .remove       = REMOVE,                                                                                                                      \
        .read8        = vmm_device_emulate_simple_read8,                                                                                             \
        .write8       = vmm_device_emulate_simple_write8,                                                                                            \
        .read16       = vmm_device_emulate_simple_read16,                                                                                            \
        .write16      = vmm_device_emulate_simple_write16,                                                                                           \
        .read32       = vmm_device_emulate_simple_read32,                                                                                            \
        .write32      = vmm_device_emulate_simple_write32,                                                                                           \
        .read64       = NULL,                                                                                                                        \
        .write64      = NULL,                                                                                                                        \
        .read_simple  = READ,                                                                                                                        \
        .write_simple = WRITE,                                                                                                                       \
    }

/**
 * @brief 模拟设备结构，绑定模拟器和目标VCPU进行设备仿真
 */
struct vmm_emulate_device {
    vmm_spinlock_t             lock; /**< 自旋锁 */
    vmm_device_tree_node_t    *node; /**< 节点 */
    struct vmm_region         *reg; /**< 寄存器 */
    vmm_emulator_t            *emu; /**< 仿真 */
    struct vmm_emulate_device *parent; /**< 父节点 */
    double_list_t              head; /**< 链表头 */
    vmm_rwlock_t               child_list_lock; /**< 子节点链表锁 */
    double_list_t              child_list; /**< 子节点链表 */
    void *private; /**< 私有数据 */
#ifdef CONFIG_DEVICE_EMULATE_DEBUG
    uint32_t debug_info; /**< debug_info成员 */
#endif
};

/**
 * @brief 模拟中断控制器结构，封装中断路由和分发逻辑
 */
struct vmm_device_emulation_irqchip {
    const char *name; /**< 名称 */
    void (*handle)(uint32_t irq, int cpu, int level, void *opaque); /**< 句柄 */
    void (*handle2)(uint32_t irq, int cpu, int level0, int level1, void *opaque); /**< handle2成员 */
    void (*map_host2guest)(uint32_t irq, uint32_t host_irq, void *opaque); /**< map_host2guest成员 */
    void (*unmap_host2guest)(uint32_t irq, void *opaque); /**< unmap_host2guest成员 */
    void (*notify_enabled)(uint32_t irq, int cpu, void *opaque); /**< notify_enabled成员 */
    void (*notify_disabled)(uint32_t irq, int cpu, void *opaque); /**< notify_disabled成员 */
};

typedef struct vmm_device_emulation_irqchip vmm_device_emulation_irqchip_t;

/**
 * @brief 模拟给定VCPU对虚拟设备的内存读操作
 */
int vmm_device_emulate_emulate_read(
    vmm_vcpu_t *vcpu, physical_addr_t guest_physical_addr, void *dst, uint32_t dst_len, enum vmm_device_emulate_endianness dst_endian);

/**
 * @brief 模拟给定VCPU对虚拟设备的内存写操作
 */
int vmm_device_emulate_emulate_write(
    vmm_vcpu_t *vcpu, physical_addr_t guest_physical_addr, void *src, uint32_t src_len, enum vmm_device_emulate_endianness src_endian);

/**
 * @brief 为指定VCPU模拟IO读操作
 */
int vmm_device_emulate_emulate_ioread(
    vmm_vcpu_t *vcpu, physical_addr_t guest_physical_addr, void *dst, uint32_t dst_len, enum vmm_device_emulate_endianness dst_endian);

/**
 * @brief 模拟给定VCPU对虚拟设备的IO写操作
 */
int vmm_device_emulate_emulate_iowrite(
    vmm_vcpu_t *vcpu, physical_addr_t guest_physical_addr, void *src, uint32_t src_len, enum vmm_device_emulate_endianness src_endian);

/**
 * @brief 模拟IRQ的内部函数（不应直接调用）
 */
extern int __vmm_device_emulate_emulate_irq(struct vmm_guest *guest, uint32_t irq, int cpu, int level);

/**
 * @brief 模拟IRQ的内部函数（不应直接调用）
 */
extern int __vmm_device_emulate_emulate_irq2(struct vmm_guest *guest, uint32_t irq, int cpu, int level0, int level1);

/** Emulate single level change in shared irq for guest
 *  Note: This will only work after guest is created.
 */
#define vmm_device_emulate_emulate_irq(guest, irq, level)                        __vmm_device_emulate_emulate_irq(guest, irq, -1, level)

/** Emulate single level change in per_cpu irq for guest
 *  Note: This will only work after guest is created.
 */
#define vmm_device_emulate_emulate_per_cpu_irq(guest, irq, cpu, level)           __vmm_device_emulate_emulate_irq(guest, irq, cpu, level)

/** Emulate two level changes in shared irq for guest
 *  Note: This will only work after guest is created.
 */
#define vmm_device_emulate_emulate_irq2(guest, irq, level0, level1)              __vmm_device_emulate_emulate_irq2(guest, irq, -1, level0, level1)

/** Emulate two level changes in per_cpu irq for guest
 *  Note: This will only work after guest is created.
 */
#define vmm_device_emulate_emulate_per_cpu_irq2(guest, irq, cpu, level0, level1) __vmm_device_emulate_emulate_irq2(guest, irq, cpu, level0, level1)

/**
 * @brief 将主机中断映射为客户机虚拟中断
 * @param guest 指向客户机结构体的指针
 * @param irq 中断号
 * @param host_irq 中断号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_map_host2guest_irq(struct vmm_guest *guest, uint32_t irq, uint32_t host_irq);

/**
 * @brief 设备模拟取消主机到客户机的中断映射
 * @param guest 指向客户机结构体的指针
 * @param irq 中断号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_unmap_host2guest_irq(struct vmm_guest *guest, uint32_t irq);

/**
 * @brief 通知模拟设备某个虚拟中断已启用
 * @param guest 指向客户机结构体的指针
 * @param irq 中断号
 * @param cpu CPU编号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_notify_irq_enabled(struct vmm_guest *guest, uint32_t irq, int cpu);

/**
 * @brief 通知模拟设备某个虚拟中断已禁用
 * @param guest 指向客户机结构体的指针
 * @param irq 中断号
 * @param cpu CPU编号
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_notify_irq_disabled(struct vmm_guest *guest, uint32_t irq, int cpu);

/**
 * @brief 注册设备模拟的中断控制器
 * @param guest 指向客户机结构体的指针
 * @param irq 中断号
 * @param chip 芯片结构体指针
 * @param opaque 不透明数据指针（用户自定义上下文）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_register_irqchip(struct vmm_guest *guest, uint32_t irq, struct vmm_device_emulation_irqchip *chip, void *opaque);

/**
 * @brief 注销设备模拟的中断控制器
 * @param guest 指向客户机结构体的指针
 * @param irq 中断号
 * @param chip 芯片结构体指针
 * @param opaque 不透明数据指针（用户自定义上下文）
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_unregister_irqchip(struct vmm_guest *guest, uint32_t irq, struct vmm_device_emulation_irqchip *chip, void *opaque);

/**
 * @brief 获取模拟设备支持的中断的数量
 * @param guest 指向客户机结构体的指针
 * @return 数量值
 */
uint32_t vmm_device_emulate_count_irqs(struct vmm_guest *guest);

/**
 * @brief 注册设备模拟器
 * @param emu 模拟设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_register_emulator(vmm_emulator_t *emu);

/**
 * @brief 注销设备模拟器
 * @param emu 模拟设备指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_unregister_emulator(vmm_emulator_t *emu);

/**
 * @brief 查找设备模拟器
 * @param name 目标对象的名称
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_emulator_t *vmm_device_emulate_find_emulator(const char *name);

/**
 * @brief 设备模拟器操作
 * @param index 数组中的索引位置
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_emulator_t *vmm_device_emulate_emulator(int index);

/**
 * @brief 获取设备模拟器的数量
 * @return 数量值
 */
uint32_t vmm_device_emulate_emulator_count(void);

/**
 * @brief 同步设备模拟的子节点
 * @param guest 指向客户机结构体的指针
 * @param edev 模拟设备实例指针
 * @param val 待写入的值
 * @param v 通用值参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_sync_children(struct vmm_guest *guest, vmm_emulate_device_t *edev, uint64_t val, void *v);

/**
 * @brief 同步设备模拟的父节点
 * @param guest 指向客户机结构体的指针
 * @param edev 模拟设备实例指针
 * @param val 待写入的值
 * @param v 通用值参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_sync_parent(struct vmm_guest *guest, vmm_emulate_device_t *edev, uint64_t val, void *v);

/**
 * @brief 复位设备模拟上下文
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_reset_context(struct vmm_guest *guest);

/**
 * @brief 复位指定内存区域内的所有模拟设备
 * @param guest 指向客户机结构体的指针
 * @param reg 寄存器值或索引
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_reset_region(struct vmm_guest *guest, struct vmm_region *reg);

/**
 * @brief 移除指定内存区域内的所有模拟设备
 * @param guest 指向客户机结构体的指针
 * @param reg 寄存器值或索引
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_remove_region(struct vmm_guest *guest, struct vmm_region *reg);

/**
 * @brief 探测并初始化指定内存区域内的所有模拟设备
 * @param guest 指向客户机结构体的指针
 * @param reg 寄存器值或索引
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_probe_region(struct vmm_guest *guest, struct vmm_region *reg);

/**
 * @brief 初始化设备模拟上下文
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_init_context(struct vmm_guest *guest);

/**
 * @brief 反初始化设备模拟上下文
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_deinit_context(struct vmm_guest *guest);

/**
 * @brief 初始化设备模拟
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_emulate_init(void);

#endif
