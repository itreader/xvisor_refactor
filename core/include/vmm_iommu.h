/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file vmm_iommu.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief IOMMU framework header for device pass-through
 *
 * The source has been largely adapted from Linux sources:
 * include/linux/iommu.h
 *
 * Copyright (C) 2007-2008 Advanced Micro Devices, Inc.
 * Author: Joerg Roedel <joerg.roedel@amd.com>
 *
 * The original source is licensed under GPL.
 */
#ifndef _VMM_IOMMU_H__
#define _VMM_IOMMU_H__

#include <libs/xref.h>
#include <vmm_device_driver.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_types.h>

#define VMM_IOMMU_CONTROLLER_CLASS_NAME "iommu"

struct vmm_iommu_ops;
struct vmm_iommu_group;
struct vmm_iommu_domain;
struct vmm_notifier_block;

typedef struct vmm_notifier_block vmm_notifier_block_t;
typedef struct vmm_iommu_ops      vmm_iommu_ops_t;
typedef struct vmm_iommu_domain   vmm_iommu_domain_t;
typedef struct vmm_iommu_group    vmm_iommu_group_t;

/* nodeid table based IOMMU initialization callback */
typedef int (*vmm_iommu_init_t)(vmm_device_tree_node_t *);

/* declare nodeid table based initialization for IOMMU */
#define VMM_IOMMU_INIT_DECLARE(name, compat, fn) VMM_DEVICE_TREE_NIDTBL_ENTRY(name, "iommu", "", "", compat, fn)

typedef struct vmm_iommu_controller {
    /* Public members */
    char          name[VMM_FIELD_NAME_SIZE];
    /* Private members */
    vmm_device_t  dev;
    vmm_mutex_t   groups_lock;
    double_list_t groups;
    vmm_mutex_t   domains_lock;
    double_list_t domains;
} vmm_iommu_controller_t;

struct vmm_iommu_group {
    char                   *name;
    vmm_iommu_controller_t *ctrl;
    double_list_t           head;

    struct xref                   ref_count;
    vmm_mutex_t                   mutex;
    vmm_iommu_domain_t           *domain;
    double_list_t                 devices;
    vmm_blocking_notifier_chain_t notifier;
    void                         *iommu_data;
    void (*iommu_data_release)(void *iommu_data);
};

/* iommu mapping attributes */
#define VMM_IOMMU_READ            (1 << 0)
#define VMM_IOMMU_WRITE           (1 << 1)
#define VMM_IOMMU_CACHE           (1 << 2) /* DMA cache coherency */
#define VMM_IOMMU_NOEXEC          (1 << 3)
#define VMM_IOMMU_MMIO            (1 << 4)

/* Domain feature flags */
#define __VMM_IOMMU_DOMAIN_PAGING (1U << 0)  /* Support for iommu_map/unmap */
#define __VMM_IOMMU_DOMAIN_DMA_API                                        \
    (1U << 1)                                /* Domain for use in DMA-API \
                            implementation              */
#define __VMM_IOMMU_DOMAIN_PT      (1U << 2) /* Domain is identity mapped   */

/*
 * This are the possible domain-types
 *
 *  VMM_IOMMU_DOMAIN_BLOCKED    - All DMA is blocked, can be used to isolate
 *                    devices
 *  VMM_IOMMU_DOMAIN_IDENTITY   - DMA addresses are system physical addresses
 *  VMM_IOMMU_DOMAIN_UNMANAGED  - DMA mappings managed by IOMMU-API user, used
 *                    for VMs
 *  VMM_IOMMU_DOMAIN_DMA        - Internally used for DMA-API implementations.
 *                    This flag allows IOMMU drivers to implement
 *                    certain optimizations for these domains
 */
#define VMM_IOMMU_DOMAIN_BLOCKED   (0U)
#define VMM_IOMMU_DOMAIN_IDENTITY  (__VMM_IOMMU_DOMAIN_PT)
#define VMM_IOMMU_DOMAIN_UNMANAGED (__VMM_IOMMU_DOMAIN_PAGING)
#define VMM_IOMMU_DOMAIN_DMA       (__VMM_IOMMU_DOMAIN_PAGING | __VMM_IOMMU_DOMAIN_DMA_API)
/* iommu fault flags */
#define VMM_IOMMU_FAULT_READ       0x0
#define VMM_IOMMU_FAULT_WRITE      0x1

typedef int (*vmm_iommu_fault_handler_t)(vmm_iommu_domain_t *, vmm_device_t *, physical_addr_t, int, void *);

struct vmm_iommu_domain_geometry {
    dma_addr_t aperture_start; /* First address that can be mapped    */
    dma_addr_t aperture_end;   /* Last address that can be mapped     */
    bool       force_aperture; /* DMA only allowed in mappable range? */
};

struct vmm_iommu_domain {
    /* Public members */
    char                    name[VMM_FIELD_NAME_SIZE];
    uint32_t                type;
    vmm_bus_t              *bus;
    vmm_iommu_controller_t *ctrl;
    /* Private members */
    double_list_t           head;
    struct xref             ref_count;
    vmm_iommu_ops_t        *ops;
    void *private;
    vmm_iommu_fault_handler_t        handler;
    void                            *handler_token;
    struct vmm_iommu_domain_geometry geometry;
};

enum vmm_iommu_cap {
    VMM_IOMMU_CAP_CACHE_COHERENCY, /* IOMMU can enforce cache coherent DMA
                      transactions */
    VMM_IOMMU_CAP_INTR_REMAP,      /* IOMMU supports interrupt isolation */
    VMM_IOMMU_CAP_NOEXEC,          /* IOMMU_NOEXEC flag */
};

/*
 * Following constraints are specifc to FSL_PAMUV1:
 *  -aperture must be power of 2, and naturally aligned
 *  -number of windows must be power of 2, and address space size
 *   of each window is determined by aperture size / # of windows
 *  -the actual size of the mapped region of a window must be power
 *   of 2 starting with 4KB and physical address must be naturally
 *   aligned.
 * DOMAIN_ATTR_FSL_PAMUV1 corresponds to the above mentioned contraints.
 * The caller can invoke iommu_domain_get_attr to check if the underlying
 * iommu implementation supports these constraints.
 */
enum vmm_iommu_attr {
    VMM_DOMAIN_ATTR_GEOMETRY,
    VMM_DOMAIN_ATTR_PAGING,
    VMM_DOMAIN_ATTR_WINDOWS,
    VMM_DOMAIN_ATTR_FSL_PAMU_STASH,
    VMM_DOMAIN_ATTR_FSL_PAMU_ENABLE,
    VMM_DOMAIN_ATTR_FSL_PAMUV1,
    VMM_DOMAIN_ATTR_MAX,
};

/**
 * IOMMU ops and capabilities
 * @capable: check capability
 * @domain_alloc: allocate iommu domain
 * @domain_free: free iommu domain
 * @attach_dev: attach device to an iommu domain
 * @detach_dev: detach device from an iommu domain
 * @map: map a physically contiguous memory region to an iommu domain
 * @unmap: unmap a physically contiguous memory region from an iommu domain
 * @iova_to_phys: translate iova to physical address
 * @add_device: add device to iommu grouping
 * @remove_device: remove device from iommu grouping
 * @domain_get_attr: Query domain attributes
 * @domain_set_attr: Change domain attributes
 * @domain_window_enable: Configure and enable a particular window for a domain
 * @domain_window_disable: Disable a particular window for a domain
 * @domain_set_windows: Set the number of windows for a domain
 * @domain_get_windows: Return the number of windows for a domain
 * @of_xlate: add OF master IDs to iommu grouping
 * @pgsize_bitmap: bitmap of all possible supported page sizes
 */
struct vmm_iommu_ops {
    bool (*capable)(enum vmm_iommu_cap);

    vmm_iommu_domain_t *(*domain_alloc)(uint32_t type, vmm_iommu_controller_t *ctrl);
    void (*domain_free)(vmm_iommu_domain_t *domain);
    int (*attach_dev)(vmm_iommu_domain_t *domain, vmm_device_t *dev);
    void (*detach_dev)(vmm_iommu_domain_t *domain, vmm_device_t *dev);
    int (*map)(vmm_iommu_domain_t *domain, physical_addr_t iova, physical_addr_t paddr, size_t size, int prot);
    size_t (*unmap)(vmm_iommu_domain_t *domain, physical_addr_t iova, size_t size);
    physical_addr_t (*iova_to_phys)(vmm_iommu_domain_t *domain, physical_addr_t iova);
    int (*add_device)(vmm_device_t *dev);
    void (*remove_device)(vmm_device_t *dev);

    int (*domain_get_attr)(vmm_iommu_domain_t *domain, enum vmm_iommu_attr attr, void *data);
    int (*domain_set_attr)(vmm_iommu_domain_t *domain, enum vmm_iommu_attr attr, void *data);

    /* Window handling functions */
    int (*domain_window_enable)(vmm_iommu_domain_t *domain, uint32_t wnd_nr, physical_addr_t paddr, uint64_t size, int prot);
    void (*domain_window_disable)(vmm_iommu_domain_t *domain, uint32_t wnd_nr);
    /* Set the numer of window per domain */
    int (*domain_set_windows)(vmm_iommu_domain_t *domain, uint32_t w_count);
    /* Get the numer of window per domain */
    uint32_t (*domain_get_windows)(vmm_iommu_domain_t *domain);

    int (*of_xlate)(vmm_device_t *dev, struct vmm_device_tree_phandle_args *args);

    uint64_t pgsize_bitmap;
};

#define VMM_IOMMU_GROUP_NOTIFY_ADD_DEVICE     1 /* Device added */
#define VMM_IOMMU_GROUP_NOTIFY_DEL_DEVICE     2 /* Pre Device removed */
#define VMM_IOMMU_GROUP_NOTIFY_BIND_DRIVER    3 /* Pre Driver bind */
#define VMM_IOMMU_GROUP_NOTIFY_BOUND_DRIVER   4 /* Post Driver bind */
#define VMM_IOMMU_GROUP_NOTIFY_UNBIND_DRIVER  5 /* Pre Driver unbind */
#define VMM_IOMMU_GROUP_NOTIFY_UNBOUND_DRIVER 6 /* Post Driver unbind */

/* =============== IOMMU Controller APIs =============== */

/** Register IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_controller_register(vmm_iommu_controller_t *ctrl);

/** Unregister IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_controller_unregister(vmm_iommu_controller_t *ctrl);

/** Find an IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
vmm_iommu_controller_t *vmm_iommu_controller_find(const char *name);

/** Iterate over each IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_controller_iterate(vmm_iommu_controller_t *start, void *data, int (*fn)(vmm_iommu_controller_t *, void *));

/** Count number of IOMMU controllers
 *  Note: This function must be called in Orphan (or Thread) context
 */
uint32_t vmm_iommu_controller_count(void);

/** Iterate over each IOMMU group of given IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_controller_for_each_group(vmm_iommu_controller_t *ctrl, void *data, int (*fn)(vmm_iommu_group_t *, void *));

/** Count number of IOMMU groups in given IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
uint32_t vmm_iommu_controller_group_count(vmm_iommu_controller_t *ctrl);

/** Iterate over each IOMMU domain of given IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_controller_for_each_domain(vmm_iommu_controller_t *ctrl, void *data, int (*fn)(vmm_iommu_domain_t *, void *));

/** Count number of IOMMU domains in given IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
uint32_t vmm_iommu_controller_domain_count(vmm_iommu_controller_t *ctrl);

/* =============== IOMMU Group APIs =============== */

/** Alloc new IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
vmm_iommu_group_t *vmm_iommu_group_alloc(const char *name, vmm_iommu_controller_t *ctrl);

/** Get IOMMU group of given device */
vmm_iommu_group_t *vmm_iommu_group_get(vmm_device_t *dev);

/** Put IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
void vmm_iommu_group_free(vmm_iommu_group_t *group);
#define vmm_iommu_group_put(group) vmm_iommu_group_free(group)

/** Get IOMMU group instance by ID */
vmm_iommu_group_t *vmm_iommu_group_get_by_id(int id);

/** Get private data for given IOMMU group */
void *vmm_iommu_group_get_iommudata(vmm_iommu_group_t *group);

/** Set private data for given IOMMU group */
void vmm_iommu_group_set_iommudata(vmm_iommu_group_t *group, void *iommu_data, void (*release)(void *iommu_data));

/** Add device to IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_group_add_device(vmm_iommu_group_t *group, vmm_device_t *dev);

/** Remove device from IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
void vmm_iommu_group_remove_device(vmm_device_t *dev);

/** Iterate over each device of given IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_group_for_each_dev(vmm_iommu_group_t *group, void *data, int (*fn)(vmm_device_t *, void *));

/** Register notifier client for IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_group_register_notifier(vmm_iommu_group_t *group, vmm_notifier_block_t *nb);

/** Unregister notifier client for IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_group_unregister_notifier(vmm_iommu_group_t *group, vmm_notifier_block_t *nb);

/** Get name for given IOMMU group */
const char *vmm_iommu_group_name(vmm_iommu_group_t *group);

/** Get IOMMU controller for given IOMMU group */
vmm_iommu_controller_t *vmm_iommu_group_controller(vmm_iommu_group_t *group);

/** Attach IOMMU domain to given IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_group_attach_domain(vmm_iommu_group_t *group, vmm_iommu_domain_t *domain);

/** Detach IOMMU domain from given IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
int vmm_iommu_group_detach_domain(vmm_iommu_group_t *group);

/** Get IOMMU domain of given IOMMU group
 *  Note: This function must be called in Orphan (or Thread) context
 */
vmm_iommu_domain_t *vmm_iommu_group_get_domain(vmm_iommu_group_t *group);

/* =============== IOMMU Domain APIs =============== */

/** Alloc new IOMMU domain for given bus type and IOMMU controller
 *  Note: This function must be called in Orphan (or Thread) context
 */
vmm_iommu_domain_t *vmm_iommu_domain_alloc(const char *name, vmm_bus_t *bus, vmm_iommu_controller_t *ctrl, uint32_t type);

/**  Increase reference count of a domain */
void vmm_iommu_domain_ref(vmm_iommu_domain_t *domain);

/** Free existing IOMMU domain
 *  Note: This function must be called in Orphan (or Thread) context
 */
void vmm_iommu_domain_free(vmm_iommu_domain_t *domain);
#define vmm_iommu_domain_dref(domain) vmm_iommu_domain_free(domain)

/** Set fault handler for given IOMMU domain */
void vmm_iommu_set_fault_handler(vmm_iommu_domain_t *domain, vmm_iommu_fault_handler_t handler, void *token);

/**
 * Report about an IOMMU fault to the IOMMU framework
 * @domain: the iommu domain where the fault has happened
 * @dev: the device where the fault has happened
 * @iova: the faulting address
 * @flags: mmu fault flags (e.g. VMM_IOMMU_FAULT_READ/VMM_IOMMU_FAULT_WRITE/...)
 *
 * This function should be called by the low-level IOMMU implementations
 * whenever IOMMU faults happen, to allow high-level users, that are
 * interested in such events, to know about them.
 *
 * This event may be useful for several possible use cases:
 * - mere logging of the event
 * - dynamic TLB/PTE loading
 * - if restarting of the faulting device is required
 *
 * Returns 0 on success and an appropriate error code otherwise (if dynamic
 * PTE/TLB loading will one day be supported, implementations will be able
 * to tell whether it succeeded or not according to this return value).
 *
 * Specifically, VMM_ENOSYS is returned if a fault handler isn't installed
 * (though fault handlers can also return VMM_ENOSYS, in case they want to
 * elicit the default behavior of the IOMMU drivers).
 */
static inline int vmm_report_iommu_fault(vmm_iommu_domain_t *domain, vmm_device_t *dev, physical_addr_t iova, int flags)
{
    int ret = VMM_ENOSYS;

    /*
     * if upper layers showed interest and installed a fault handler,
     * invoke it.
     */
    if (domain->handler) {
        ret = domain->handler(domain, dev, iova, flags, domain->handler_token);
    }

    return ret;
}

/** Get IO virtual addres mapping for given IOMMU domain */
physical_addr_t vmm_iommu_iova_to_phys(vmm_iommu_domain_t *domain, physical_addr_t iova);

/** Map IO virtual address to Physical address for given IOMMU domain */
int vmm_iommu_map(vmm_iommu_domain_t *domain, physical_addr_t iova, physical_addr_t paddr, size_t size, int prot);

/** Unmap IO virtual address for given IOMMU domain */
size_t vmm_iommu_unmap(vmm_iommu_domain_t *domain, physical_addr_t iova, size_t size);

/** Enable physical address window for IOMMU domain */
int vmm_iommu_domain_window_enable(vmm_iommu_domain_t *domain, uint32_t wnd_nr, physical_addr_t offset, uint64_t size, int prot);

/** Disable physical address window for IOMMU domain */
void vmm_iommu_domain_window_disable(vmm_iommu_domain_t *domain, uint32_t wnd_nr);

/** Get attributes of IOMMU domain */
int vmm_iommu_domain_get_attr(vmm_iommu_domain_t *domain, enum vmm_iommu_attr, void *data);

/** Set attributes for IOMMU domain */
int vmm_iommu_domain_set_attr(vmm_iommu_domain_t *domain, enum vmm_iommu_attr, void *data);

/* =============== IOMMU Misc APIs =============== */

/** Set IOMMU operations for given bus type */
int vmm_bus_set_iommu(vmm_bus_t *bus, vmm_iommu_ops_t *ops);

/** Check whethere IOMMU operations are available for given bus type */
bool vmm_iommu_present(vmm_bus_t *bus);

/**  Capability check on IOMMU for given bus type */
bool vmm_iommu_capable(vmm_bus_t *bus, enum vmm_iommu_cap cap);

/** Initialize IOMMU framework */
int __init vmm_iommu_init(void);

#endif
