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
 * @brief header file for hypervisor manager
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

enum vmm_region_flags {
    VMM_REGION_REAL        = 0x00000001,
    VMM_REGION_VIRTUAL     = 0x00000002,
    VMM_REGION_ALIAS       = 0x00000004,
    VMM_REGION_MEMORY      = 0x00000008,
    VMM_REGION_IO          = 0x00000010,
    VMM_REGION_CACHEABLE   = 0x00000020,
    VMM_REGION_BUFFERABLE  = 0x00000040,
    VMM_REGION_READONLY    = 0x00000080,
    VMM_REGION_IS_RAM      = 0x00000100,
    VMM_REGION_IS_ROM      = 0x00000200,
    VMM_REGION_IS_DEVICE   = 0x00000400,
    VMM_REGION_IS_RESERVED = 0x00000800,
    VMM_REGION_IS_ALLOCED  = 0x00001000,
    VMM_REGION_IS_COLORED  = 0x00002000,
    VMM_REGION_IS_SHARED   = 0x00004000,
    VMM_REGION_IS_DYNAMIC  = 0x00008000,
};

#define VMM_REGION_MANIFEST_MASK (VMM_REGION_REAL | VMM_REGION_VIRTUAL | VMM_REGION_ALIAS)

enum vmm_region_mapping_flags {
    VMM_REGION_MAPPING_ISHOSTRAM = 0x00000001,
};

struct vmm_region;
struct vmm_region_mapping;
struct vmm_guest_address_space;
struct vmm_vcpu_irqs;
struct vmm_vcpu;
struct vmm_guest;

typedef struct vmm_vcpu vmm_vcpu_t;

struct vmm_region_mapping {
    physical_addr_t hphys_addr;
    uint32_t        flags;
};

struct vmm_region {
    struct red_black_node           head;
    double_list_t                   phead;
    vmm_device_tree_node_t         *node;
    struct vmm_guest_address_space *aspace;
    uint32_t                        flags;
    physical_addr_t                 guest_physical_addr;
    physical_addr_t                 aphys_addr;
    physical_size_t                 phys_size;
    uint32_t                        first_color;
    uint32_t                        num_colors;
    vmm_share_memory_t             *share_memory;
    uint32_t                        align_order;
    uint32_t                        map_order;
    uint32_t                        maps_count;
    struct vmm_region_mapping      *maps;
    void                           *device_emulate_private;
    void *private;
};

#define VMM_REGION_NAME(reg)                  ((reg)->node->name)
#define VMM_REGION_GPHYS_START(reg)           ((reg)->guest_physical_addr)
#define VMM_REGION_GPHYS_END(reg)             ((reg)->guest_physical_addr + (reg)->phys_size)
#define VMM_REGION_PHYS_SIZE(reg)             ((reg)->phys_size)
#define VMM_REGION_GPHYS_TO_APHYS(reg, gphys) ((reg)->aphys_addr + ((gphys) - (reg)->guest_physical_addr))
#define VMM_REGION_FLAGS(reg)                 ((reg)->flags)
#define VMM_REGION_ALIGN_ORDER(reg)           ((reg)->align_order)
#define VMM_REGION_MAP_ORDER(reg)             ((reg)->map_order)
#define VMM_REGION_MAPS_COUNT(reg)            ((reg)->maps_count)

struct vmm_guest_address_space {
    vmm_device_tree_node_t *node;
    struct vmm_guest       *guest;
    bool                    initialized;

    vmm_rwlock_t          reg_iotree_lock;
    struct red_black_root reg_iotree;

    double_list_t reg_ioprobe_list;

    vmm_rwlock_t          reg_memory_tree_lock;
    struct red_black_root reg_memtree;
    double_list_t         reg_memprobe_list;
    void                 *device_emulate_private;
};

/* 描述一个对guest的操作请求 */
struct vmm_guest_request {
    double_list_t head;
    void         *data;
    void (*func)(struct vmm_guest *, void *);
};

struct vmm_vcpu_irq {
    atomic_t assert;
    uint64_t reason;
};

struct vmm_vcpu_irqs {
    uint32_t             irq_count;
    struct vmm_vcpu_irq *irq;
    atomic_t             execute_pending;
    atomic64_t           assert_count;
    atomic64_t           execute_count;
    atomic64_t           clear_count;
    atomic64_t           deassert_count;

    struct {
        vmm_spinlock_t lock;
        uint32_t       yield_count;
        bool           state;
        void *private;
    } wfi; /* 用于休眠 */
};

/**
 * 一个guest的相关信息描述
 */
struct vmm_guest {
    double_list_t head;  // guest链表

    /* General information */
    uint32_t                id;                         // 每个guest一个唯一全局ID
    char                    name[VMM_FIELD_NAME_SIZE];  // guest虚拟机名称
    vmm_device_tree_node_t *node;
    bool                    is_big_endian;              // 是否大端
    uint32_t                reset_count;                // guest复位的次数
    uint64_t                reset_timestamp;            // guest复位时的时间戳

    /* Request queue */
    vmm_spinlock_t request_lock;           /* 外部对guest的操作请求锁 */
    double_list_t  operation_request_list; /* 外部对guest的操作请求列表 */

    /* VCPU instances belonging to this Guest */
    vmm_rwlock_t  vcpu_lock;   // 虚拟CPU的读写锁
    uint32_t      vcpu_count;  // guest拥有的虚拟CPU数量，也就是vcpu_list的数量
    double_list_t vcpu_list;   // VCPU列表

    /* Guest address space */
    struct vmm_guest_address_space aspace;  // 用于映射guest的物理地址空间

    /* Architecture specific context */
    void *arch_private;  // 特定于体系结构的上下文
};

enum vmm_vcpu_states {
    VMM_VCPU_STATE_UNKNOWN = 0x01,
    VMM_VCPU_STATE_RESET   = 0x02,
    VMM_VCPU_STATE_READY   = 0x04,
    VMM_VCPU_STATE_RUNNING = 0x08,
    VMM_VCPU_STATE_PAUSED  = 0x10,
    VMM_VCPU_STATE_HALTED  = 0x20
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

struct vmm_vcpu_resource {
    double_list_t head;
    const char   *name;
    void (*cleanup)(vmm_vcpu_t *vcpu, struct vmm_vcpu_resource *res);
};

typedef struct vmm_vcpu_resource vmm_vcpu_resource_t;

/* 虚拟化管理平台管理的VCPU */
struct vmm_vcpu {
    double_list_t head;

    /* General information */
    uint32_t                id;     // VCPU的GUID号
    uint32_t                subid;  // VCPU所属的guest的ID号
    char                    name[VMM_FIELD_NAME_SIZE];
    vmm_device_tree_node_t *node;
    bool                    is_normal;
    bool                    is_poweroff;
    struct vmm_guest       *guest;

    /* Start PC and stack */
    virtual_addr_t start_pc;
    virtual_addr_t stack_va;
    virtual_size_t stack_size;

    /* Scheduler dynamic context */
    vmm_rwlock_t         sched_lock;
    uint32_t             host_cpu;
    const vmm_cpumask_t *cpu_affinity;
    atomic_t             state;
    uint64_t             state_tstamp;
    uint64_t             state_ready_nsecs;
    uint64_t             state_running_nsecs;
    uint64_t             state_paused_nsecs;
    uint64_t             state_halted_nsecs;
    uint64_t             system_nsecs;
    uint32_t             reset_count;
    uint64_t             reset_timestamp;
    uint32_t             preempt_count;
    bool                 resumed;
    void                *sched_private;

    /* Scheduler static context */
    uint8_t  priority;
    uint64_t time_slice;
    uint64_t deadline;
    uint64_t periodicity;

    /* Architecture specific context */
    arch_regs_t regs;
    void       *arch_private;

    /* Virtual IRQ context */
    struct vmm_vcpu_irqs irqs;

    /* Resources acquired */
    vmm_spinlock_t res_lock;
    double_list_t  res_head;

    /* Waitqueue context */
    double_list_t   wq_head;
    vmm_spinlock_t *wq_lock;
    void           *wq_private;

    /* Waitqueue cleanup callback */
    void (*wq_cleanup)(vmm_vcpu_t *);
};

/** Acquire manager lock */
void vmm_manager_lock(void);

/** Release manager lock */
void vmm_manager_unlock(void);

/** Maximum number of VCPUs */
uint32_t vmm_manager_max_vcpu_count(void);

/** Current number of VCPUs (orphan + normal) */
uint32_t vmm_manager_vcpu_count(void);

/** Retrieve VCPU with given ID.
 *  Returns NULL if there is no VCPU associated with given ID.
 */
vmm_vcpu_t *vmm_manager_vcpu(uint32_t vcpu_id);

/** Iterate over each VCPU with manager lock held */
int vmm_manager_vcpu_iterate(int (*iter)(vmm_vcpu_t *, void *), void *private);

/** Retriver VCPU state */
uint32_t vmm_manager_vcpu_get_state(vmm_vcpu_t *vcpu);

/** Update VCPU state
 *  Note: Avoid calling this function directly
 */
int vmm_manager_vcpu_set_state(vmm_vcpu_t *vcpu, uint32_t state);

/** Reset a VCPU */
#define vmm_manager_vcpu_reset(vcpu)  vmm_manager_vcpu_set_state((vcpu), VMM_VCPU_STATE_RESET)

/** Kick a VCPU out of reset state */
#define vmm_manager_vcpu_kick(vcpu)   vmm_manager_vcpu_set_state((vcpu), VMM_VCPU_STATE_READY)

/** Pause a VCPU */
#define vmm_manager_vcpu_pause(vcpu)  vmm_manager_vcpu_set_state((vcpu), VMM_VCPU_STATE_PAUSED)

/** Resume a VCPU */
#define vmm_manager_vcpu_resume(vcpu) vmm_manager_vcpu_set_state((vcpu), VMM_VCPU_STATE_READY)

/** Halt a VCPU */
#define vmm_manager_vcpu_halt(vcpu)   vmm_manager_vcpu_set_state((vcpu), VMM_VCPU_STATE_HALTED)

/** Retrive host CPU assigned to given VCPU */
int vmm_manager_vcpu_get_hcpu(vmm_vcpu_t *vcpu, uint32_t *host_cpu);

/** Check host CPU assigned to given VCPU is current host CPU */
bool vmm_manager_vcpu_check_current_hcpu(vmm_vcpu_t *vcpu);

/** Update host CPU assigned to given VCPU */
int vmm_manager_vcpu_set_hcpu(vmm_vcpu_t *vcpu, uint32_t host_cpu);

/** Force re-scheduling on host CPU assigned to given VCPU */
int vmm_manager_vcpu_hcpu_resched(vmm_vcpu_t *vcpu);

/** Call function on host CPU assigned to given VCPU if
 *  VCPU state matches given state_mask
 */
int vmm_manager_vcpu_hcpu_func(vmm_vcpu_t *vcpu, uint32_t state_mask, void (*func)(vmm_vcpu_t *, void *), void *data, bool use_async);

/** Retrive host CPU affinity of given VCPU */
const vmm_cpumask_t *vmm_manager_vcpu_get_affinity(vmm_vcpu_t *vcpu);

/** Update host CPU affinity of given VCPU */
int vmm_manager_vcpu_set_affinity(vmm_vcpu_t *vcpu, const vmm_cpumask_t *cpu_mask);

/** Add resource to VCPU resource list */
int vmm_manager_vcpu_resource_add(vmm_vcpu_t *vcpu, vmm_vcpu_resource_t *res);

/** Remove resource from VCPU resource list */
int vmm_manager_vcpu_resource_remove(vmm_vcpu_t *vcpu, vmm_vcpu_resource_t *res);

/** Create an orphan VCPU */
vmm_vcpu_t *vmm_manager_vcpu_orphan_create(
    const char *name, virtual_addr_t start_pc, virtual_size_t stack_size, uint8_t priority, uint64_t time_slice_nsecs, uint64_t deadline,
    uint64_t periodicity, const vmm_cpumask_t *affinity);

/** Destroy an orphan VCPU */
int vmm_manager_vcpu_orphan_destroy(vmm_vcpu_t *vcpu);

/** Maximum number of Guests */
uint32_t vmm_manager_max_guest_count(void);

/** Current number of Guests */
uint32_t vmm_manager_guest_count(void);

/** Retrieve Guest with given ID.
 *  Returns NULL if there is no Guest associated with given ID.
 */
struct vmm_guest *vmm_manager_guest(uint32_t guest_id);

/** Find Guest with given name.
 *  Returns NULL if there is no Guest with given name.
 */
struct vmm_guest *vmm_manager_guest_find(const char *guest_name);

/** Iterate over each Guest with manager lock held */
int vmm_manager_guest_iterate(int (*iter)(struct vmm_guest *, void *), void *private);

/** Number of VCPUs belonging to a given Guest */
uint32_t vmm_manager_guest_vcpu_count(struct vmm_guest *guest);

/** Retrieve VCPU belonging to a given Guest with particular subid */
vmm_vcpu_t *vmm_manager_guest_vcpu(struct vmm_guest *guest, uint32_t subid);

/** Get next VCPU of a given Guest */
vmm_vcpu_t *vmm_manager_guest_next_vcpu(const struct vmm_guest *guest, vmm_vcpu_t *current);

/** Iterate over each VCPU of a given Guest */
#define vmm_manager_for_each_guest_vcpu(__v, __g) for (__v = vmm_manager_guest_next_vcpu(__g, NULL); __v; __v = vmm_manager_guest_next_vcpu(__g, __v))

/** Iterate over each VCPU of a given Guest with guest->vcpu_lock held */
int vmm_manager_guest_vcpu_iterate(struct vmm_guest *guest, int (*iter)(vmm_vcpu_t *, void *), void *private);

/** Reset a Guest */
int vmm_manager_guest_reset(struct vmm_guest *guest);

/** Last Reset timestamp of a Guest */
uint64_t vmm_manager_guest_reset_timestamp(struct vmm_guest *guest);

/** Kick a Guest out of reset state */
int vmm_manager_guest_kick(struct vmm_guest *guest);

/** Pause a Guest */
int vmm_manager_guest_pause(struct vmm_guest *guest);

/** Resume a Guest */
int vmm_manager_guest_resume(struct vmm_guest *guest);

/** Halt a Guest */
int vmm_manager_guest_halt(struct vmm_guest *guest);

/** Schedule request callback for a Guest
 *  NOTE: Use this only for non-performance critical function
 *  because we use one vmm_work per-Guest to process all Guest
 *  request. For performance critical functions use different
 *  bottom-half mechanism for processing Guest request.
 *  NOTE: If Guest is destroyed then all its pending requests
 *  will be ignored.
 */
int vmm_manager_guest_operation_request(struct vmm_guest *guest, void (*req_func)(struct vmm_guest *, void *), void *req_data);

/** Schedule reboot request for a Guest
 *  NOTE: This request will first reset the Guest then kick it.
 */
int vmm_manager_guest_reboot_request(struct vmm_guest *guest);

/** Schedule shutdown request for a Guest
 *  NOTE: This request will only reset the Guest.
 */
int vmm_manager_guest_shutdown_request(struct vmm_guest *guest);

/** Create a Guest based on device tree configuration */
struct vmm_guest *vmm_manager_guest_create(vmm_device_tree_node_t *gnode);

/** Destroy a Guest */
int vmm_manager_guest_destroy(struct vmm_guest *guest);

/** Initialize manager */
int vmm_manager_init(void);

#endif
