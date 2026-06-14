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
 * @file vmm_manager.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Hypervisor管理器头文件
 */
#ifndef _VMM_MANAGER_H__
#define _VMM_MANAGER_H__

#include <arch_atomic.h>
#include <arch_atomic64.h>
#include <arch_regs.h>
#include <libs/list.h>
#include <libs/red_black_tree.h>
#include <vmm_cpumask.h>
#include <vmm_device_tree.h>
#include <vmm_limits.h>
#include <vmm_share_memory.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

/**
 * @brief 客户区域标志枚举，定义内存区域的读写和共享属性
 */
enum vmm_region_flags {
    VMM_REGION_REAL        = 0x00000001, /**< 真实区域 */
    VMM_REGION_VIRTUAL     = 0x00000002, /**< 虚拟区域 */
    VMM_REGION_ALIAS       = 0x00000004, /**< 别名区域 */
    VMM_REGION_MEMORY      = 0x00000008, /**< 内存区域 */
    VMM_REGION_IO          = 0x00000010, /**< IO区域 */
    VMM_REGION_CACHEABLE   = 0x00000020, /**< 区域可以有cache的能力 */
    VMM_REGION_BUFFERABLE  = 0x00000040, /**< 区域是可缓冲的 */
    VMM_REGION_READONLY    = 0x00000080, /**< 区域是只读的 */
    VMM_REGION_IS_RAM      = 0x00000100, /**< RAM区域 */
    VMM_REGION_IS_ROM      = 0x00000200, /**< ROM区域 */
    VMM_REGION_IS_DEVICE   = 0x00000400, /**< 设备区域 */
    VMM_REGION_IS_RESERVED = 0x00000800, /**< 保留区域 */
    VMM_REGION_IS_ALLOCED  = 0x00001000, /**< 已分配区域 */
    VMM_REGION_IS_COLORED  = 0x00002000, /**< 已着色区域 */
    VMM_REGION_IS_SHARED   = 0x00004000, /**< 共享区域 */
    VMM_REGION_IS_DYNAMIC  = 0x00008000, /**< 动态区域 */
};

#define VMM_REGION_MANIFEST_MASK (VMM_REGION_REAL | VMM_REGION_VIRTUAL | VMM_REGION_ALIAS)

/**
 * @brief 区域映射标志枚举，定义映射的权限和类型
 */
enum vmm_region_mapping_flags {
    VMM_REGION_MAPPING_ISHOSTRAM = 0x00000001, /**< 主机RAM映射 */
};

struct vmm_region;
struct vmm_region_mapping;
struct vmm_guest_address_space;
struct vmm_vcpu_irqs;
struct vmm_vcpu;
struct vmm_guest;

typedef struct vmm_vcpu vmm_vcpu_t;

/**
 * @brief 区域映射结构，描述客户物理地址到主机物理地址的映射关系
 */
struct vmm_region_mapping {
    physical_addr_t hphys_addr; /**< 主机物理地址 */
    uint32_t        flags;      /**< 映射标志 */
};

/**
 * @brief 客户内存区域，描述一个连续的地址空间及其映射列表
 */
struct vmm_region {
    red_black_node_t           head;        /**< 红黑树节点 */
    double_list_t                   phead;    /**< 链表节点 */
    vmm_device_tree_node_t         *node;    /**< 设备树节点 */
    struct vmm_guest_address_space *addr_space; /**< 所属地址空间 */
    uint32_t                        flags;   /**< 区域标志 */
    physical_addr_t                 guest_physical_addr; /**< 客户机物理地址 */
    physical_addr_t                 alias_physical_addr; /**< 别名物理地址 */
    physical_size_t                 phys_size; /**< 物理大小 */
    uint32_t                        first_color; /**< 第一个颜色 */
    uint32_t                        num_colors;  /**< 颜色数量 */
    vmm_share_memory_t             *share_memory; /**< 共享内存 */
    uint32_t                        align_order; /**< 对齐阶 */
    uint32_t                        map_order;   /**< 映射阶 */
    uint32_t                        maps_count;  /**< 映射计数 */
    struct vmm_region_mapping      *maps;        /**< 映射数组 */
    void                           *device_emulate_private; /**< 设备仿真私有数据 */
    void *private; /**< 私有数据 */
};

#define VMM_REGION_NAME(reg)                  ((reg)->node->name)
#define VMM_REGION_GPHYS_START(reg)           ((reg)->guest_physical_addr)
#define VMM_REGION_GPHYS_END(reg)             ((reg)->guest_physical_addr + (reg)->phys_size)
#define VMM_REGION_PHYS_SIZE(reg)             ((reg)->phys_size)
#define VMM_REGION_GPHYS_TO_APHYS(reg, gphys) ((reg)->alias_physical_addr + ((gphys) - (reg)->guest_physical_addr))
#define VMM_REGION_FLAGS(reg)                 ((reg)->flags)
#define VMM_REGION_ALIGN_ORDER(reg)           ((reg)->align_order)
#define VMM_REGION_MAP_ORDER(reg)             ((reg)->map_order)
#define VMM_REGION_MAPS_COUNT(reg)            ((reg)->maps_count)

/* 负责管理客户机操作系统可见的内存和IO 区域，包括物理内存区域和设备 IO 区域两类 */
/**
 * @brief 客户地址空间结构，维护客户物理地址到主机地址的映射
 */
struct vmm_guest_address_space {
    vmm_device_tree_node_t *node;      /**< 设备树节点 */
    struct vmm_guest       *guest;     /**< 所属guest */
    bool                    initialized; /**< 是否已经初始化完成 */

    vmm_rwlock_t          reg_iotree_lock;  /**< I/O 区域树锁 */
    red_black_root_t reg_iotree;           /**< I/O 区域树，vmm_region类型的红黑树 */
    double_list_t         reg_ioprobe_list; /**< IO 区域探测链表 */

    vmm_rwlock_t          reg_memory_tree_lock;  /**< 内存区域树锁 */
    red_black_root_t reg_memtree;                /**< 内存区域树，vmm_region类型的红黑树 */
    double_list_t         reg_memprobe_list;     /**< 内存区域探测链表 */

    void *device_emulate_private;  /**< 设备仿真上下文 */
};

/* 描述一个对guest的操作请求 */
/**
 * @brief 客户请求结构，封装客户机创建/销毁等管理操作请求
 */
struct vmm_guest_request {
    double_list_t head;  /**< 链表节点 */
    void         *data;  /**< 请求数据 */
    void (*func)(struct vmm_guest *, void *);  /**< 请求处理函数 */
};

/**
 * @brief VCPU中断结构，描述单个中断源的编号和处理状态
 */
struct vmm_vcpu_irq {
    atomic_t assert; /**< 中断断言标志 */
    uint64_t reason; /**< 中断原因 */
};

/**
 * @brief VCPU中断集合，维护一个VCPU上所有中断的数组和统计
 */
struct vmm_vcpu_irqs {
    uint32_t             irq_count;        /**< 中断数量 */
    struct vmm_vcpu_irq *irq;              /**< 中断数组 */
    atomic_t             execute_pending;  /**< 待执行中断标志 */
    atomic64_t           assert_count;     /**< 中断断言计数 */
    atomic64_t           execute_count;    /**< 中断执行计数 */
    atomic64_t           clear_count;      /**< 中断清除计数 */
    atomic64_t           deassert_count;   /**< 中断去断言计数 */

    struct {
        vmm_spinlock_t lock;        /**< WFI锁 */
        uint32_t       yield_count; /**< 让出计数 */
        bool           state;       /**< WFI状态 */
        void *private;              /**< 私有数据 */
    } wfi; /**< 用于休眠的WFI上下文 */
};

/**
 * 一个guest的相关信息描述
 */
struct vmm_guest {
    double_list_t head;  /**< guest链表 */

    /* General information */
    uint32_t                id;                         /**< 每个guest一个唯一全局ID */
    char                    name[VMM_FIELD_NAME_SIZE];  /**< guest虚拟机名称 */
    vmm_device_tree_node_t *node;                       /**< 设备树节点 */
    bool                    is_big_endian;              /**< 是否大端 */
    uint32_t                reset_count;                /**< guest复位的次数 */
    uint64_t                reset_timestamp;            /**< guest复位时的时间戳 */

    /* Request queue */
    vmm_spinlock_t request_lock;           /**< 外部对guest的操作请求锁 */
    double_list_t  operation_request_list; /**< 外部对guest的操作请求列表 */

    /* VCPU instances belonging to this Guest */
    vmm_rwlock_t  vcpu_lock;   /**< 虚拟CPU的读写锁 */
    uint32_t      vcpu_count;  /**< guest拥有的虚拟CPU数量 */
    double_list_t vcpu_list;   /**< VCPU列表 */

    /* Guest address space */
    struct vmm_guest_address_space addr_space;  /**< 用于映射guest的物理地址空间 */

    /* Architecture specific context */
    void *arch_private;  /**< 特定于体系结构的上下文 */
};

/**
 * @brief VCPU状态枚举，定义VCPU从创建到退出的所有运行状态
 */
enum vmm_vcpu_states {
    VMM_VCPU_STATE_UNKNOWN = 0x01, /**< 未知状态 */
    VMM_VCPU_STATE_RESET   = 0x02, /**< 复位状态 */
    VMM_VCPU_STATE_READY   = 0x04, /**< 就绪状态 */
    VMM_VCPU_STATE_RUNNING = 0x08, /**< 运行状态 */
    VMM_VCPU_STATE_PAUSED  = 0x10, /**< 暂停状态 */
    VMM_VCPU_STATE_HALTED  = 0x20  /**< 停止状态 */
};

#define VMM_VCPU_STATE_ALLMASK       0xff

#define VMM_VCPU_STATE_SAVEABLE      (VMM_VCPU_STATE_RUNNING | VMM_VCPU_STATE_PAUSED | VMM_VCPU_STATE_HALTED)

#define VMM_VCPU_STATE_INTERRUPTIBLE (VMM_VCPU_STATE_RUNNING | VMM_VCPU_STATE_READY | VMM_VCPU_STATE_PAUSED)

#define VMM_VCPU_MIN_PRIORITY        0
#define VMM_VCPU_MAX_PRIORITY        7
#define VMM_VCPU_DEF_PRIORITY        3
#define VMM_VCPU_DEF_TIME_SLICE      (CONFIG_TSLICE_MS * 1000000)
#define VMM_VCPU_DEF_DEADLINE        (VMM_VCPU_DEF_TIME_SLICE * 10)
#define VMM_VCPU_DEF_PERIODICITY     (VMM_VCPU_DEF_DEADLINE * 10)

/**
 * @brief VCPU资源结构，保存VCPU运行所需的架构相关资源
 */
struct vmm_vcpu_resource {
    double_list_t head;  /**< 链表节点 */
    const char   *name;  /**< 资源名称 */
    void (*cleanup)(vmm_vcpu_t *vcpu, struct vmm_vcpu_resource *res);  /**< 清理回调 */
};

typedef struct vmm_vcpu_resource vmm_vcpu_resource_t;

/* 虚拟化管理平台管理的VCPU */
/**
 * @brief 虚拟CPU核心结构，包含调度、状态、中断等完整VCPU运行时信息
 */
struct vmm_vcpu {
    double_list_t head;  /**< VCPU链表节点 */

    /* General information */
    uint32_t                id;     /**< VCPU的GUID号 */
    uint32_t                subid;  /**< VCPU所属的guest的ID号 */
    char                    name[VMM_FIELD_NAME_SIZE];  /**< VCPU名称 */
    vmm_device_tree_node_t *node;   /**< 设备树节点 */
    bool                    is_normal;    /**< 是否为普通VCPU */
    bool                    is_poweroff;  /**< 是否已关机 */
    struct vmm_guest       *guest;        /**< 所属guest */

    /* Start PC and stack */
    virtual_addr_t start_pc;                /**< 启动PC地址 */
    virtual_addr_t stack_virtual_address;   /**< 栈虚拟地址 */
    virtual_size_t stack_size;              /**< 栈大小 */

    /* Scheduler dynamic context */
    vmm_rwlock_t         sched_lock;        /**< 调度器读写锁 */
    uint32_t             host_cpu;          /**< 运行该VCPU的主机CPU */
    const vmm_cpumask_t *cpu_affinity;      /**< CPU亲和性掩码 */
    atomic_t             state;             /**< VCPU状态 */
    uint64_t             state_tstamp;      /**< 状态时间戳 */
    uint64_t             state_ready_nsecs; /**< 就绪状态时间 */
    uint64_t             state_running_nsecs; /**< 运行状态时间 */
    uint64_t             state_paused_nsecs; /**< 暂停状态时间 */
    uint64_t             state_halted_nsecs; /**< 停止状态时间 */
    uint64_t             system_nsecs;      /**< 系统时间 */
    uint32_t             reset_count;       /**< 复位次数 */
    uint64_t             reset_timestamp;   /**< 复位时间戳 */
    uint32_t             preempt_count;     /**< 抢占计数 */
    bool                 resumed;           /**< 是否已恢复 */
    void                *sched_private;     /**< 调度器私有数据 */

    /* Scheduler static context */
    uint8_t  priority;     /**< 优先级 */
    uint64_t time_slice;   /**< 时间片 */
    uint64_t deadline;     /**< 截止时间 */
    uint64_t periodicity;  /**< 周期性 */

    /* Architecture specific context */
    arch_regs_t regs;          /**< 架构相关寄存器 */
    void       *arch_private;  /**< 架构相关私有数据 */

    /* Virtual IRQ context */
    struct vmm_vcpu_irqs irqs;  /**< 虚拟中断上下文 */

    /* Resources acquired */
    vmm_spinlock_t res_lock;  /**< 资源锁 */
    double_list_t  res_head;  /**< 资源链表 */

    /* Waitqueue context */
    double_list_t   wait_queue_head;    /**< 等待队列头 */
    vmm_spinlock_t *wait_queue_lock;    /**< 等待队列锁 */
    void           *wait_queue_private; /**< 等待队列私有数据 */

    /* Waitqueue cleanup callback */
    void (*wait_queue_cleanup)(vmm_vcpu_t *);  /**< 等待队列清理回调 */
};

/**
 * @brief 获取管理器锁
 */
void vmm_manager_lock(void);

/**
 * @brief 释放管理器锁
 */
void vmm_manager_unlock(void);

/**
 * @brief 获取最大VCPU的数量
 * @return 返回最大VCPU数量
 */
uint32_t vmm_manager_max_vcpu_count(void);

/**
 * @brief 获取当前VCPU的数量（包括孤儿VCPU和普通VCPU）
 * @return 返回当前VCPU数量
 */
uint32_t vmm_manager_vcpu_count(void);

/**
 * @brief 根据ID获取VCPU
 * @param vcpu_id VCPU的ID
 * @return 返回VCPU指针，如果没有找到返回NULL
 */
vmm_vcpu_t *vmm_manager_vcpu(uint32_t vcpu_id);

/**
 * @brief 遍历每个VCPU（持有管理器锁）
 * @param iter 迭代回调函数
 * @param private 传递给回调函数的私有数据
 * @return 成功返回0，失败返回错误码
 */
int vmm_manager_vcpu_iterate(int (*iter)(vmm_vcpu_t *, void *), void *private);

/**
 * @brief 获取VCPU状态
 * @param vcpu 指向VCPU结构体的指针
 * @return 返回VCPU状态
 */
uint32_t vmm_manager_vcpu_get_state(vmm_vcpu_t *vcpu);

/**
 * @brief 更新VCPU状态
 * @param vcpu 指向VCPU结构体的指针
 * @param state 新的状态值
 * @return 成功返回0，失败返回错误码
 * @note 避免直接调用此函数
 */
int vmm_manager_vcpu_set_state(vmm_vcpu_t *vcpu, uint32_t state);

/**
 * @brief 复位VCPU
 * @param vcpu 指向VCPU结构体的指针
 */
#define vmm_manager_vcpu_reset(vcpu)  vmm_manager_vcpu_set_state((vcpu), VMM_VCPU_STATE_RESET)

/**
 * @brief 将VCPU从复位状态唤醒
 * @param vcpu 指向VCPU结构体的指针
 */
#define vmm_manager_vcpu_kick(vcpu)   vmm_manager_vcpu_set_state((vcpu), VMM_VCPU_STATE_READY)

/**
 * @brief 暂停VCPU
 * @param vcpu 指向VCPU结构体的指针
 */
#define vmm_manager_vcpu_pause(vcpu)  vmm_manager_vcpu_set_state((vcpu), VMM_VCPU_STATE_PAUSED)

/**
 * @brief 恢复VCPU
 * @param vcpu 指向VCPU结构体的指针
 */
#define vmm_manager_vcpu_resume(vcpu) vmm_manager_vcpu_set_state((vcpu), VMM_VCPU_STATE_READY)

/**
 * @brief 停止VCPU
 * @param vcpu 指向VCPU结构体的指针
 */
#define vmm_manager_vcpu_halt(vcpu)   vmm_manager_vcpu_set_state((vcpu), VMM_VCPU_STATE_HALTED)

/**
 * @brief 获取分配给VCPU的主机CPU
 * @param vcpu 指向VCPU结构体的指针
 * @param host_cpu 指向uint32_t的指针，用于返回主机CPU ID
 * @return 成功返回0，失败返回错误码
 */
int vmm_manager_vcpu_get_hcpu(vmm_vcpu_t *vcpu, uint32_t *host_cpu);

/**
 * @brief 检查分配给VCPU的主机CPU是否是当前主机CPU
 * @param vcpu 指向VCPU结构体的指针
 * @return 如果是当前主机CPU返回true，否则返回false
 */
bool vmm_manager_vcpu_check_current_hcpu(vmm_vcpu_t *vcpu);

/**
 * @brief 更新分配给VCPU的主机CPU
 * @param vcpu 指向VCPU结构体的指针
 * @param host_cpu 主机CPU ID
 * @return 成功返回0，失败返回错误码
 */
int vmm_manager_vcpu_set_hcpu(vmm_vcpu_t *vcpu, uint32_t host_cpu);

/**
 * @brief 强制在分配给VCPU的主机CPU上重新调度
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回0，失败返回错误码
 */
int vmm_manager_vcpu_hcpu_resched(vmm_vcpu_t *vcpu);

/**
 * @brief 在分配给VCPU的主机CPU上调用函数
 * @param vcpu 指向VCPU结构体的指针
 * @param state_mask 状态掩码
 * @param func 要调用的函数
 * @param data 传递给函数的数据
 * @param use_async 是否异步调用
 * @return 成功返回0，失败返回错误码
 */
int vmm_manager_vcpu_hcpu_func(vmm_vcpu_t *vcpu, uint32_t state_mask, void (*func)(vmm_vcpu_t *, void *), void *data, bool use_async);

/**
 * @brief 获取VCPU的主机CPU亲和性
 * @param vcpu 指向VCPU结构体的指针
 * @return 返回CPU亲和性掩码指针
 */
const vmm_cpumask_t *vmm_manager_vcpu_get_affinity(vmm_vcpu_t *vcpu);

/**
 * @brief 更新VCPU的主机CPU亲和性
 * @param vcpu 指向VCPU结构体的指针
 * @param cpu_mask CPU亲和性掩码
 * @return 成功返回0，失败返回错误码
 */
int vmm_manager_vcpu_set_affinity(vmm_vcpu_t *vcpu, const vmm_cpumask_t *cpu_mask);

/**
 * @brief 添加资源到VCPU资源列表
 * @param vcpu 指向VCPU结构体的指针
 * @param res 指向资源结构体的指针
 * @return 成功返回0，失败返回错误码
 */
int vmm_manager_vcpu_resource_add(vmm_vcpu_t *vcpu, vmm_vcpu_resource_t *res);

/**
 * @brief 从VCPU资源列表中移除资源
 * @param vcpu 指向VCPU结构体的指针
 * @param res 指向资源结构体的指针
 * @return 成功返回0，失败返回错误码
 */
int vmm_manager_vcpu_resource_remove(vmm_vcpu_t *vcpu, vmm_vcpu_resource_t *res);

/**
 * @brief 创建孤儿VCPU
 * @param name VCPU名称
 * @param start_pc 启动PC地址
 * @param stack_size 栈大小
 * @param priority 优先级
 * @param time_slice_nsecs 时间片（纳秒）
 * @param deadline 截止时间
 * @param periodicity 周期性
 * @param affinity CPU亲和性掩码
 * @return 成功返回VCPU指针，失败返回NULL
 */
vmm_vcpu_t *vmm_manager_vcpu_orphan_create(
    const char *name, virtual_addr_t start_pc, virtual_size_t stack_size, uint8_t priority, uint64_t time_slice_nsecs, uint64_t deadline,
    uint64_t periodicity, const vmm_cpumask_t *affinity);

/**
 * @brief 销毁孤儿VCPU
 * @param vcpu 指向VCPU结构体的指针
 * @return 成功返回0，失败返回错误码
 */
int vmm_manager_vcpu_orphan_destroy(vmm_vcpu_t *vcpu);

/**
 * @brief 获取最大Guest的数量
 * @return 返回最大Guest数量
 */
uint32_t vmm_manager_max_guest_count(void);

/**
 * @brief 获取当前Guest的数量
 * @return 返回当前Guest数量
 */
uint32_t vmm_manager_guest_count(void);

/**
 * @brief 根据ID获取Guest
 * @param guest_id Guest的ID
 * @return 返回Guest指针，如果没有找到返回NULL
 */
struct vmm_guest *vmm_manager_guest(uint32_t guest_id);

/**
 * @brief 根据名称查找Guest
 * @param guest_name Guest名称
 * @return 返回Guest指针，如果没有找到返回NULL
 */
struct vmm_guest *vmm_manager_guest_find(const char *guest_name);

/**
 * @brief 管理器 客户机 遍历
 * @param (*iter 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_manager_guest_iterate(int (*iter)(struct vmm_guest *, void *), void *private);

/**
 * @brief 获取客户机VCPU管理器的数量
 * @param guest 指向客户机结构体的指针
 * @return 数量值
 */
uint32_t vmm_manager_guest_vcpu_count(struct vmm_guest *guest);

/**
 * @brief 管理器 客户机 虚拟CPU
 * @param guest 指向客户机结构体的指针
 * @param subid 标识符
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_vcpu_t *vmm_manager_guest_vcpu(struct vmm_guest *guest, uint32_t subid);

/**
 * @brief 管理器 客户机 下一个 虚拟CPU
 * @param guest 指向客户机结构体的指针
 * @param current 指向VCPU结构体的指针
 * @return 成功返回目标指针，失败返回NULL
 */
vmm_vcpu_t *vmm_manager_guest_next_vcpu(const struct vmm_guest *guest, vmm_vcpu_t *current);

/** Iterate over each VCPU of a given Guest */
#define vmm_manager_for_each_guest_vcpu(__v, __g) for (__v = vmm_manager_guest_next_vcpu(__g, NULL); __v; __v = vmm_manager_guest_next_vcpu(__g, __v))

/**
 * @brief 管理器 客户机 虚拟CPU 遍历
 * @param guest 指向客户机结构体的指针
 * @param (*iter 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_manager_guest_vcpu_iterate(struct vmm_guest *guest, int (*iter)(vmm_vcpu_t *, void *), void *private);

/**
 * @brief 复位客户机管理器
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_manager_guest_reset(struct vmm_guest *guest);

/**
 * @brief 获取客户机最近一次复位的时间戳
 * @param guest 指向客户机结构体的指针
 * @return 返回64位无符号整数值
 */
uint64_t vmm_manager_guest_reset_timestamp(struct vmm_guest *guest);

/**
 * @brief 管理器 客户机 唤醒
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_manager_guest_kick(struct vmm_guest *guest);

/**
 * @brief 管理器 客户机 暂停
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_manager_guest_pause(struct vmm_guest *guest);

/**
 * @brief 管理器 客户机 恢复
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_manager_guest_resume(struct vmm_guest *guest);

/**
 * @brief 管理器 客户机 停止
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_manager_guest_halt(struct vmm_guest *guest);

/**
 * @brief 处理客户机操作请求（启动/停止/复位等）
 * @param guest 指向客户机结构体的指针
 * @param (*req_func 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_manager_guest_operation_request(struct vmm_guest *guest, void (*req_func)(struct vmm_guest *, void *), void *req_data);

/**
 * @brief 处理客户机重启请求
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_manager_guest_reboot_request(struct vmm_guest *guest);

/**
 * @brief 处理客户机关机请求
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_manager_guest_shutdown_request(struct vmm_guest *guest);

/** Create a Guest based on device tree configuration */
struct vmm_guest *vmm_manager_guest_create(vmm_device_tree_node_t *gnode);

/**
 * @brief 销毁客户机管理器
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_manager_guest_destroy(struct vmm_guest *guest);

/**
 * @brief 初始化管理器
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_manager_init(void);

#endif
