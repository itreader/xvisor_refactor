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
 * @file vmm_device_driver.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Device driver framework header
 */

#ifndef __VMM_DEVDRV_H_
#define __VMM_DEVDRV_H_

#include <libs/list.h>
#include <libs/xref.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_mutex.h>
#include <vmm_notifier.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#define VMM_DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL << (n)) - 1))

struct vmm_device;
struct vmm_driver;
struct vmm_iommu_ops;
struct vmm_iommu_group;
struct vmm_msi_domain;

typedef struct vmm_iommu_group vmm_iommu_group_t;
typedef struct vmm_device      vmm_device_t;
typedef struct vmm_driver      vmm_driver_t;
typedef struct vmm_iommu_ops   vmm_iommu_ops_t;
typedef struct vmm_msi_domain  vmm_msi_domain_t;

typedef struct vmm_class {
    /* Private fields (for device driver framework) */
    double_list_t head;
    vmm_mutex_t   lock;
    double_list_t device_list;
    /* Public fields */
    char          name[VMM_FIELD_NAME_SIZE];
    void (*release)(vmm_device_t *);
} vmm_class_t;

typedef struct vmm_bus {
    /* Private fields (for device driver framework) */
    double_list_t                 head;
    vmm_mutex_t                   lock;
    double_list_t                 device_list;
    double_list_t                 driver_list;
    vmm_blocking_notifier_chain_t event_listeners;
    /* Public fields */
    char                          name[VMM_FIELD_NAME_SIZE];
    vmm_iommu_ops_t              *iommu_ops;
    int (*match)(vmm_device_t *dev, vmm_driver_t *drv);
    int (*probe)(vmm_device_t *);
    int (*remove)(vmm_device_t *);
    void (*shutdown)(vmm_device_t *);
} vmm_bus_t;

typedef struct vmm_device_type {
    const char *name;
    void (*release)(vmm_device_t *);
} vmm_device_type_t;

struct vmm_device {
    /* Private fields (for device driver framework) */
    double_list_t           bus_head;
    double_list_t           class_head;
    struct xref             ref_count;
    bool                    is_registered;
    double_list_t           child_head;
    vmm_mutex_t             child_list_lock;
    double_list_t           child_list;
    vmm_spinlock_t          device_resource_lock;
    double_list_t           device_resource_head;
    double_list_t           deferred_head;
    double_list_t           msi_list;
    vmm_msi_domain_t       *msi_domain;
    /* Public fields */
    char                    name[VMM_FIELD_NAME_SIZE];
    bool                    autoprobe_disabled;
    vmm_bus_t              *bus;
    struct vmm_device_type *type;
    vmm_device_tree_node_t *of_node;
    vmm_device_t           *parent;
    vmm_class_t *class;
    vmm_driver_t      *driver;
    vmm_iommu_group_t *iommu_group;
    void              *iommu_private;
    uint64_t          *dma_mask;
    void              *pins;
    void (*release)(vmm_device_t *);
    void *private;
};

struct vmm_driver {
    /* Private fields (for device driver framework) */
    double_list_t                        head;
    /* Public fields */
    char                                 name[VMM_FIELD_NAME_SIZE];
    vmm_bus_t                           *bus;
    const struct vmm_device_tree_nodeid *match_table;
    int (*probe)(vmm_device_t *);
    int (*suspend)(vmm_device_t *, uint32_t);
    int (*resume)(vmm_device_t *);
    int (*remove)(vmm_device_t *);
};

/** Get driver data from device */
static inline void *vmm_device_driver_get_data(const vmm_device_t *dev)
{
    return (dev) ? dev->private : NULL;
}

/** Set driver data in device */
static inline void vmm_device_driver_set_data(vmm_device_t *dev, void *data)
{
    if (dev) {
        dev->private = data;
    }
}

/** Get MSI domain from device */
static inline vmm_msi_domain_t *vmm_device_driver_get_msi_domain(vmm_device_t *dev)
{
    return (dev) ? dev->msi_domain : NULL;
}

/** Set MSI domain in device */
static inline void vmm_device_driver_set_msi_domain(vmm_device_t *dev, vmm_msi_domain_t *domain)
{
    if (dev) {
        dev->msi_domain = domain;
    }
}

/** get the dma_mask from device */
static inline uint64_t vmm_dma_get_mask(vmm_device_t *dev)
{
    if (dev && dev->dma_mask && *dev->dma_mask) {
        return *dev->dma_mask;
    }

    return VMM_DMA_BIT_MASK(32);
}

/** set the dma_mask in device */
static inline int vmm_dma_set_mask(vmm_device_t *dev, uint64_t mask)
{
    if (!dev->dma_mask) {
        return VMM_EIO;
    }

    *dev->dma_mask = mask;
    return VMM_OK;
}

/** Register class */
int vmm_device_driver_register_class(vmm_class_t *cls);

/** Unregister class */
int vmm_device_driver_unregister_class(vmm_class_t *cls);

/** Find a registered class */
vmm_class_t *vmm_device_driver_find_class(const char *cname);

/* Iterate over each registered class */
int vmm_device_driver_class_iterate(vmm_class_t *start, void *data, int (*fn)(vmm_class_t *cls, void *data));

/** Count available classes */
uint32_t vmm_device_driver_class_count(void);

/** Find device of a class using match function */
vmm_device_t *vmm_device_driver_class_find_device(vmm_class_t *cls, void *data, int (*match)(vmm_device_t *, void *));

/** Find device of a class by name */
vmm_device_t *vmm_device_driver_class_find_device_by_name(vmm_class_t *cls, const char *dname);

/** Iterate over each device of a class with class->lock held */
int vmm_device_driver_class_device_iterate(vmm_class_t *cls, vmm_device_t *start, void *data, int (*fn)(vmm_device_t *dev, void *data));

/* Count available devices in a class */
uint32_t vmm_device_driver_class_device_count(vmm_class_t *cls);

/** Register bus */
int vmm_device_driver_register_bus(vmm_bus_t *bus);

/** Unregister bus */
int vmm_device_driver_unregister_bus(vmm_bus_t *bus);

/** Find a registered bus */
vmm_bus_t *vmm_device_driver_find_bus(const char *bname);

/* Iterate over each registered bus */
int vmm_device_driver_bus_iterate(vmm_bus_t *start, void *data, int (*fn)(vmm_bus_t *bus, void *data));

/** Count available buses */
uint32_t vmm_device_driver_bus_count(void);

/** Find device on a bus */
vmm_device_t *vmm_device_driver_bus_find_device(vmm_bus_t *bus, vmm_device_t *start, void *data, int (*match)(vmm_device_t *, void *));

/** Find device on a bus by name */
vmm_device_t *vmm_device_driver_bus_find_device_by_name(vmm_bus_t *bus, vmm_device_t *start, const char *dname);

/** Find device on a bus by node */
vmm_device_t *vmm_device_driver_bus_find_device_by_node(vmm_bus_t *bus, vmm_device_t *start, vmm_device_tree_node_t *np);

/** Iterate over each device of a bus with bus->lock held */
int vmm_device_driver_bus_device_iterate(vmm_bus_t *bus, vmm_device_t *start, void *data, int (*fn)(vmm_device_t *dev, void *data));

/** Count available devices on a bus */
uint32_t vmm_device_driver_bus_device_count(vmm_bus_t *bus);

/** Register driver on a bus */
int vmm_device_driver_bus_register_driver(vmm_bus_t *bus, vmm_driver_t *drv);

/** Unregister driver on a bus */
int vmm_device_driver_bus_unregister_driver(vmm_bus_t *bus, vmm_driver_t *drv);

/** Find driver for a bus */
vmm_driver_t *vmm_device_driver_bus_find_driver(vmm_bus_t *bus, const char *dname);

/** Iterate over each driver of a bus with bus->lock held */
int vmm_device_driver_bus_driver_iterate(vmm_bus_t *bus, vmm_driver_t *start, void *data, int (*fn)(vmm_driver_t *drv, void *data));

/** Count available drivers for a bus */
uint32_t vmm_device_driver_bus_driver_count(vmm_bus_t *bus);

/** Register a client for bus events */
int vmm_device_driver_bus_register_notifier(vmm_bus_t *bus, vmm_notifier_block_t *nb);

/** Unregister a client for bus events */
int vmm_device_driver_bus_unregister_notifier(vmm_bus_t *bus, vmm_notifier_block_t *nb);

/* All 4 notifers below get called with the target struct device *
 * as an argument. Note that those functions are likely to be called
 * with the device lock held in the core, so be careful.
 */
#define VMM_BUS_NOTIFY_ADD_DEVICE 0x00000001   /* device added */
#define VMM_BUS_NOTIFY_DEL_DEVICE 0x00000002   /* device removed */
#define VMM_BUS_NOTIFY_BIND_DRIVER                                                                                                                   \
    0x00000003                                 /* driver about to be                                                                                 \
                              bound */
#define VMM_BUS_NOTIFY_BOUND_DRIVER 0x00000004 /* driver bound to device */
#define VMM_BUS_NOTIFY_UNBIND_DRIVER                                                                                                                 \
    0x00000005                                 /* driver about to be                                                                                 \
                                unbound */
#define VMM_BUS_NOTIFY_UNBOUND_DRIVER                                                                                                                \
    0x00000006                                 /* driver is unbound                                                                                  \
                                from the device */

/** Initialize device */
void vmm_device_driver_initialize_device(vmm_device_t *dev);

/** Increment reference count of device */
vmm_device_t *vmm_device_driver_ref_device(vmm_device_t *dev);

/** Decrement reference count of device */
void vmm_device_driver_dref_device(vmm_device_t *dev);

/** Check whether device is registered or not */
bool vmm_device_driver_isregistered_device(vmm_device_t *dev);

/** Check whether device is attached to driver or not */
bool vmm_device_driver_isattached_device(vmm_device_t *dev);

/** Iterate over each child device of given device */
int vmm_device_driver_for_each_child(vmm_device_t *dev, void *data, int (*fn)(vmm_device_t *dev, void *data));

/** Register device */
int vmm_device_driver_register_device(vmm_device_t *dev);

/** Force attach device with device driver */
int vmm_device_driver_attach_device(vmm_device_t *dev);

/** Force dettach device with device driver */
int vmm_device_driver_dettach_device(vmm_device_t *dev);

/** Unregister device */
int vmm_device_driver_unregister_device(vmm_device_t *dev);

/** Register device driver */
int vmm_device_driver_register_driver(vmm_driver_t *drv);

/** Force attach device driver */
int vmm_device_driver_attach_driver(vmm_driver_t *drv);

/** Force dettach device driver */
int vmm_device_driver_dettach_driver(vmm_driver_t *drv);

/** Unregister device driver */
int vmm_device_driver_unregister_driver(vmm_driver_t *drv);

/** Initalize device driver framework */
int vmm_device_driver_init(void);

#endif /* __VMM_DEVDRV_H_ */
