/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file vmm_host_irq_domain.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @author Anup Patel (anup@brainfault.org)
 * @brief IRQ domain support, kind of Xvior compatible Linux IRQ domain.
 */

#ifndef _VMM_HOST_IRQDOMAIN_H__
#define _VMM_HOST_IRQDOMAIN_H__

#include <libs/list.h>
#include <vmm_device_tree.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

struct vmm_char_device;
struct vmm_host_irq_domain;
typedef struct vmm_char_device vmm_char_device_t;

/**
 * struct vmm_host_irq_domain_ops - Methods for vmm_host_irq_domain objects
 * @match: Match an interrupt controller device node to a host, returns
 *         1 on a match
 * @map: Create or update a mapping between a virtual irq number and a hw
 *       irq number. This is called only once for a given mapping.
 * @unmap: Dispose of such a mapping
 * @xlate: Given a device tree node and interrupt specifier, decode
 *         the hardware irq number and linux irq type value.
 * @alloc: Allocate a specified number of hardware irqs
 * @free: Free hardware irqs
 *
 * Functions below are provided by the driver and called whenever a new
 * mapping is created or an old mapping is disposed. The driver can then
 * proceed to whatever internal data structures management is required.
 * It also needs to setup the irq_desc when returning from map().
 */
struct vmm_host_irq_domain_ops {
    int (*match)(struct vmm_host_irq_domain *d, vmm_device_tree_node_t *node);
    int (*map)(struct vmm_host_irq_domain *d, uint32_t hirq, uint32_t hwirq);
    void (*unmap)(struct vmm_host_irq_domain *d, uint32_t hirq);
    int (*xlate)(
        struct vmm_host_irq_domain *d, vmm_device_tree_node_t *node, const uint32_t *intspec, uint32_t intsize, uint64_t *out_hwirq,
        uint32_t *out_type);
    int (*alloc)(struct vmm_host_irq_domain *d, uint32_t nr_irqs, void *arg);
    void (*free)(struct vmm_host_irq_domain *d, uint32_t hwirq, uint32_t nr_irqs);
};

/**
 * struct vmm_host_irq_domain - IRQ domain, kind of Linux IRQ domain
 * @head:   List head for registration
 * @base:   Base
 * @count:  The number of IRQs contained.
 * @ops:    Pointer to vmm_host_irq_domain methods.
 * @irqs:   The extended IRQ array
 *
 * Optional elements
 * @of_node:    The device node using this domain
 * @host_data:  The controller private data pointer. Not touched by extended
 *      IRQ core code.
 * @bmap_lock:  The IRQ domain bitmap lock
 * @bmap:   The IRQ domain bitmap
 */
struct vmm_host_irq_domain {
    double_list_t                         head;
    bool                                  uses_extend_irq;
    uint32_t                              base;
    uint32_t                              count;
    uint32_t                              end;
    const struct vmm_host_irq_domain_ops *ops;
    vmm_device_tree_node_t               *of_node;
    void                                 *host_data;
    vmm_spinlock_t                        bmap_lock;
    uint64_t                             *bmap;
};

/** Convert host IRQ to HW IRQ */
int vmm_host_irq_domain_to_hwirq(struct vmm_host_irq_domain *domain, uint32_t hirq);

/** Convert HW IRQ to host IRQ */
int vmm_host_irq_domain_to_hirq(struct vmm_host_irq_domain *domain, uint32_t hwirq);

/** Find host IRQ for givne HW IRQ */
int vmm_host_irq_domain_find_mapping(struct vmm_host_irq_domain *domain, uint32_t hwirq);

/** Find matching host IRQ domain based on given match function */
struct vmm_host_irq_domain *vmm_host_irq_domain_match(void *data, int (*fn)(struct vmm_host_irq_domain *, void *));

/** Dump host IRQ domain debug info */
void vmm_host_irq_domain_debug_dump(vmm_char_device_t *cdev);

/** Find host IRQ domain for given host IRQ */
struct vmm_host_irq_domain *vmm_host_irq_domain_get(uint32_t hirq);

/** Create mapping in host IRQ domain for given HW IRQ */
int vmm_host_irq_domain_create_mapping(struct vmm_host_irq_domain *domain, uint32_t hwirq);

/** Dispose mapping in host IRQ domain associated with given host IRQ */
void vmm_host_irq_domain_dispose_mapping(uint32_t hirq);

/** Allocate and map host IRQs */
int vmm_host_irq_domain_alloc(struct vmm_host_irq_domain *domain, uint32_t irq_count, void *arg);

/** Free and unmap host IRQs */
void vmm_host_irq_domain_free(struct vmm_host_irq_domain *domain, uint32_t hirq, uint32_t irq_count);

/** Translate device tree cells to HW IRQ for given host IRQ domain
 *  using xlate() callback provided in host IRQ domain ops.
 */
int vmm_host_irq_domain_xlate(struct vmm_host_irq_domain *domain, const uint32_t *intspec, uint32_t intsize, uint64_t *out_hwirq, uint32_t *out_type);

/** Common xlate() callback to translate one device tree cell */
int vmm_host_irq_domain_xlate_onecell(
    struct vmm_host_irq_domain *domain, vmm_device_tree_node_t *node, const uint32_t *intspec, uint32_t intsize, uint64_t *out_hwirq,
    uint32_t *out_type);

/** Common xlate() callback to translate two device tree cells */
int vmm_host_irq_domain_xlate_twocells(
    struct vmm_host_irq_domain *domain, vmm_device_tree_node_t *node, const uint32_t *intspec, uint32_t intsize, uint64_t *out_hwirq,
    uint32_t *out_type);

/**
 * Allocate and register a new host IRQ domain.
 * @of_node: pointer to interrupt controller's device tree node.
 * @base: Base host IRQ number. If < 0 then extended IRQs are created.
 * @size: Number of interrupts in the domain.
 * @ops: map/unmap domain callbacks.
 * @host_data: Controller private data pointer.
 */
struct vmm_host_irq_domain *vmm_host_irq_domain_add(
    vmm_device_tree_node_t *of_node, int base, uint32_t size, const struct vmm_host_irq_domain_ops *ops, void *host_data);

/** Remove existing host IRQ domain */
void vmm_host_irq_domain_remove(struct vmm_host_irq_domain *domain);

/** Initialize host IRQ domain framework */
int vmm_host_irq_domain_init(void);

extern const struct vmm_host_irq_domain_ops irq_domain_ops;

#endif /* _VMM_HOST_IRQDOMAIN_H__ */
