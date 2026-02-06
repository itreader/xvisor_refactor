/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 * @file vmm_host_irq_domain.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @author Anup Patel (anup@brainfault.org)
 * @brief IRQ domain support, kind of Xvior compatible Linux IRQ domain.
 */

#include <libs/bitmap.h>
#include <libs/list.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_extend_irq.h>
#include <vmm_host_irq.h>
#include <vmm_host_irq_domain.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

struct vmm_host_irq_domain_ctrl {
    vmm_rwlock_t  lock;
    double_list_t domains;
};

static struct vmm_host_irq_domain_ctrl idctrl;

int vmm_host_irq_domain_to_hwirq(struct vmm_host_irq_domain *domain, uint32_t hirq)
{
    if (!domain) {
        return VMM_EINVALID;
    }

    if (hirq < domain->base || hirq >= domain->end) {
        return VMM_ENOTAVAIL;
    }

    return hirq - domain->base;
}

int vmm_host_irq_domain_to_hirq(struct vmm_host_irq_domain *domain, uint32_t hwirq)
{
    if (!domain) {
        return VMM_EINVALID;
    }

    if (hwirq >= domain->count) {
        return VMM_ERANGE;
    }

    return domain->base + hwirq;
}

int vmm_host_irq_domain_find_mapping(struct vmm_host_irq_domain *domain, uint32_t hwirq)
{
    if (!domain) {
        return VMM_EINVALID;
    }

    if (hwirq >= domain->count) {
        return VMM_ERANGE;
    }

    if (vmm_host_irq_get(domain->base + hwirq)) {
        return domain->base + hwirq;
    }

    return VMM_ENOTAVAIL;
}

struct vmm_host_irq_domain *vmm_host_irq_domain_match(void *data, int (*fn)(struct vmm_host_irq_domain *, void *))
{
    irq_flags_t                 flags;
    struct vmm_host_irq_domain *domain = NULL;
    struct vmm_host_irq_domain *found  = NULL;

    vmm_read_lock_irq_save_lite(&idctrl.lock, flags);

    list_for_each_entry(domain, &idctrl.domains, head)
    {
        if (fn(domain, data)) {
            found = domain;
            break;
        }
    }

    vmm_read_unlock_irq_restore_lite(&idctrl.lock, flags);

    return found;
}

void vmm_host_irq_domain_debug_dump(vmm_char_device_t *cdev)
{
    int                         idx = 0;
    irq_flags_t                 flags;
    struct vmm_host_irq        *irq    = NULL;
    struct vmm_host_irq_domain *domain = NULL;

    vmm_read_lock_irq_save_lite(&idctrl.lock, flags);

    list_for_each_entry(domain, &idctrl.domains, head)
    {
        vmm_cdev_printf(cdev, "  Group from IRQ %d to %d:\n", domain->base, domain->end);

        for (idx = domain->base; idx < domain->end; ++idx) {
            irq = vmm_host_irq_get(idx);

            if (!irq) {
                continue;
            }

            if (idx != irq->num) {
                vmm_cdev_printf(
                    cdev,
                    "WARNING: IRQ %d "
                    "not correctly set\n",
                    irq->num);
            }

            vmm_cdev_printf(
                cdev,
                "    IRQ %d mapped, name: %s, "
                "chip: %s\n",
                idx, irq->name, irq->chip ? irq->chip->name : "None");
        }
    }

    vmm_read_unlock_irq_restore_lite(&idctrl.lock, flags);
}

struct vmm_host_irq_domain *vmm_host_irq_domain_get(uint32_t hirq)
{
    irq_flags_t                 flags;
    struct vmm_host_irq_domain *domain = NULL;

    vmm_read_lock_irq_save_lite(&idctrl.lock, flags);

    list_for_each_entry(domain, &idctrl.domains, head)
    {
        if ((hirq >= domain->base) && (hirq < domain->end)) {
            vmm_read_unlock_irq_restore_lite(&idctrl.lock, flags);
            return domain;
        }
    }

    vmm_read_unlock_irq_restore_lite(&idctrl.lock, flags);

    return NULL;
}

static int __irq_domain_create_mapping(struct vmm_host_irq_domain *domain, uint32_t hirq, uint32_t hwirq)
{
    int rc = VMM_OK;

    if (hirq < CONFIG_HOST_IRQ_COUNT) {
        rc = __vmm_host_irq_set_hwirq(hirq, hwirq);
    } else {
        rc = vmm_host_extend_irq_create_mapping(hirq, hwirq);
    }

    if (rc) {
        return rc;
    }

    if (domain->ops && domain->ops->map) {
        rc = domain->ops->map(domain, hirq, hwirq);

        if (rc) {
            if (hirq < CONFIG_HOST_IRQ_COUNT) {
                __vmm_host_irq_set_hwirq(hirq, hirq);
            } else {
                vmm_host_extend_irq_dispose_mapping(hirq);
            }

            return rc;
        }
    }

    return VMM_OK;
}

static void __irq_domain_dispose_mapping(struct vmm_host_irq_domain *domain, uint32_t hirq)
{
    if (domain->ops && domain->ops->unmap) {
        domain->ops->unmap(domain, hirq);
    }

    if (hirq < CONFIG_HOST_IRQ_COUNT) {
        __vmm_host_irq_set_hwirq(hirq, hirq);
    } else {
        vmm_host_extend_irq_dispose_mapping(hirq);
    }
}

int vmm_host_irq_domain_create_mapping(struct vmm_host_irq_domain *domain, uint32_t hwirq)
{
    int         rc = VMM_OK;
    uint32_t    hirq;
    irq_flags_t flags;

    if (!domain) {
        return VMM_ENOTAVAIL;
    }

    if (hwirq >= domain->count) {
        return VMM_ENOTAVAIL;
    }

    hirq = domain->base + hwirq;

    vmm_spin_lock_irq_save_lite(&domain->bmap_lock, flags);

    if (bitmap_isset(domain->bmap, hwirq)) {
        vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);
        return hirq;
    }

    bitmap_set(domain->bmap, hwirq, 1);
    vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);

    rc = __irq_domain_create_mapping(domain, hirq, hwirq);

    if (rc) {
        vmm_spin_lock_irq_save_lite(&domain->bmap_lock, flags);
        bitmap_clear(domain->bmap, hwirq, 1);
        vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);
        return rc;
    }

    return hirq;
}

void vmm_host_irq_domain_dispose_mapping(uint32_t hirq)
{
    int                         hwirq;
    irq_flags_t                 flags;
    struct vmm_host_irq_domain *domain = vmm_host_irq_domain_get(hirq);

    if (!domain) {
        return;
    }

    hwirq = vmm_host_irq_domain_to_hwirq(domain, hirq);

    if (hwirq < 0) {
        return;
    }

    vmm_spin_lock_irq_save_lite(&domain->bmap_lock, flags);

    if (!bitmap_isset(domain->bmap, hwirq)) {
        vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);
        return;
    }

    bitmap_clear(domain->bmap, hwirq, 1);
    vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);

    __irq_domain_dispose_mapping(domain, hirq);
}

int vmm_host_irq_domain_alloc(struct vmm_host_irq_domain *domain, uint32_t irq_count, void *arg)
{
    int         rc;
    irq_flags_t flags;
    bool        found = false;
    uint32_t    i, j, hirq, hwirq, count;

    if (!domain || !irq_count || (domain->count < irq_count)) {
        return VMM_EINVALID;
    }

    if (domain->ops->alloc) {
        rc = domain->ops->alloc(domain, irq_count, arg);

        if (rc < 0) {
            return rc;
        }

        hwirq = rc;
        vmm_spin_lock_irq_save_lite(&domain->bmap_lock, flags);
        bitmap_set(domain->bmap, hwirq, irq_count);
        vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);
    } else {
        vmm_spin_lock_irq_save_lite(&domain->bmap_lock, flags);

        if (!found) {
            count = 0;

            for (hwirq = 0; hwirq < domain->count; hwirq++) {
                if (bitmap_isset(domain->bmap, hwirq)) {
                    count = 0;
                } else {
                    count++;
                }

                if (count == irq_count) {
                    found = true;
                    hwirq = hwirq - (count - 1);
                    break;
                }
            }
        }

        if (!found) {
            vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);
            return VMM_ENOENT;
        }

        bitmap_set(domain->bmap, hwirq, irq_count);
        vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);
    }

    hirq = domain->base + hwirq;

    for (i = 0; i < irq_count; i++) {
        rc = __irq_domain_create_mapping(domain, hirq + i, hwirq + i);

        if (rc) {
            for (j = 0; j < i; j++) {
                __irq_domain_dispose_mapping(domain, hirq + j);
            }

            vmm_spin_lock_irq_save_lite(&domain->bmap_lock, flags);
            bitmap_clear(domain->bmap, hwirq, irq_count);
            vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);

            if (domain->ops->free) {
                domain->ops->free(domain, hwirq, irq_count);
            }

            return rc;
        }
    }

    return hirq;
}

void vmm_host_irq_domain_free(struct vmm_host_irq_domain *domain, uint32_t hirq, uint32_t irq_count)
{
    irq_flags_t flags;
    uint32_t    i, hwirq;

    if (!domain || (hirq < domain->base) || ((hirq + irq_count) < domain->base) || ((domain->base + domain->count) <= hirq) ||
        ((domain->base + domain->count) <= (hirq + irq_count))) {
        return;
    }

    for (i = 0; i < irq_count; i++) {
        __irq_domain_dispose_mapping(domain, hirq + i);
    }

    hwirq = hirq - domain->base;
    vmm_spin_lock_irq_save_lite(&domain->bmap_lock, flags);
    bitmap_clear(domain->bmap, hwirq, irq_count);
    vmm_spin_unlock_irq_restore_lite(&domain->bmap_lock, flags);

    if (domain->ops->free) {
        domain->ops->free(domain, hwirq, irq_count);
    }
}

int vmm_host_irq_domain_xlate(struct vmm_host_irq_domain *domain, const uint32_t *intspec, uint32_t intsize, uint64_t *out_hwirq, uint32_t *out_type)
{
    if (!domain || !intspec || !out_hwirq || !out_type) {
        return VMM_EINVALID;
    }

    /* If domain has no translation, then we assume interrupt line */
    if (!domain->ops || !domain->ops->xlate) {
        *out_hwirq = intspec[0];
    } else {
        return domain->ops->xlate(domain, domain->of_node, intspec, intsize, out_hwirq, out_type);
    }

    return VMM_OK;
}

int vmm_host_irq_domain_xlate_onecell(
    struct vmm_host_irq_domain *domain, vmm_device_tree_node_t *node, const uint32_t *intspec, uint32_t intsize, uint64_t *out_hwirq,
    uint32_t *out_type)
{
    if (WARN_ON(intsize != 1)) {
        return VMM_EINVALID;
    }

    *out_hwirq = intspec[0];
    *out_type  = VMM_IRQ_TYPE_NONE;

    return VMM_OK;
}

int vmm_host_irq_domain_xlate_twocells(
    struct vmm_host_irq_domain *domain, vmm_device_tree_node_t *node, const uint32_t *intspec, uint32_t intsize, uint64_t *out_hwirq,
    uint32_t *out_type)
{
    if (WARN_ON(intsize != 2)) {
        return VMM_EINVALID;
    }

    *out_hwirq = intspec[0];
    *out_type  = intspec[1] & VMM_IRQ_TYPE_SENSE_MASK;

    return VMM_OK;
}

struct vmm_host_irq_domain *vmm_host_irq_domain_add(
    vmm_device_tree_node_t *of_node, int base, uint32_t size, const struct vmm_host_irq_domain_ops *ops, void *host_data)
{
    int                         pos = 0;
    irq_flags_t                 flags;
    uint64_t                   *bmap;
    struct vmm_host_irq_domain *newdomain = NULL;

    if (!size || !ops) {
        return NULL;
    }

    if ((base >= 0) && ((CONFIG_HOST_IRQ_COUNT <= base) || (CONFIG_HOST_IRQ_COUNT <= (base + size)))) {
        return NULL;
    }

    if ((base >= 0) && (vmm_host_irq_domain_get(base) || vmm_host_irq_domain_get(base + size - 1))) {
        return NULL;
    }

    bmap = vmm_zalloc(bitmap_estimate_size(size));

    if (!bmap) {
        return NULL;
    }

    newdomain = vmm_zalloc(sizeof(struct vmm_host_irq_domain));

    if (!newdomain) {
        vmm_free(bmap);
        return NULL;
    }

    if (base < 0) {
        if ((pos = vmm_host_extend_irq_alloc_region(size)) < 0) {
            vmm_printf("%s: Failed to find available slot for IRQ\n", __func__);
            vmm_free(bmap);
            vmm_free(newdomain);
            return NULL;
        }
    } else {
        pos = base;
    }

    INIT_LIST_HEAD(&newdomain->head);
    newdomain->uses_extend_irq = (base < 0) ? TRUE : FALSE;
    newdomain->base            = pos;
    newdomain->count           = size;
    newdomain->end             = newdomain->base + size;
    newdomain->host_data       = host_data;

    if (of_node) {
        vmm_device_tree_ref_node(of_node);
        newdomain->of_node = of_node;
    }

    newdomain->ops = ops;
    INIT_SPIN_LOCK(&newdomain->bmap_lock);
    newdomain->bmap = bmap;

    vmm_write_lock_irq_save_lite(&idctrl.lock, flags);
    list_add_tail(&newdomain->head, &idctrl.domains);
    vmm_write_unlock_irq_restore_lite(&idctrl.lock, flags);

    return newdomain;
}

void vmm_host_irq_domain_remove(struct vmm_host_irq_domain *domain)
{
    uint32_t    pos = 0;
    irq_flags_t flags;

    if (!domain) {
        return;
    }

    vmm_write_lock_irq_save_lite(&idctrl.lock, flags);
    list_del(&domain->head);
    vmm_write_unlock_irq_restore_lite(&idctrl.lock, flags);

    for (pos = domain->base; pos < domain->end; ++pos) {
        vmm_host_extend_irq_dispose_mapping(pos);
    }

    if (domain->uses_extend_irq) {
        vmm_host_extend_irq_free_region(domain->base, domain->count);
    }

    if (domain->of_node) {
        vmm_device_tree_dref_node(domain->of_node);
    }

    vmm_free(domain);
}

int __init vmm_host_irq_domain_init(void)
{
    memset(&idctrl, 0, sizeof(struct vmm_host_irq_domain_ctrl));
    INIT_RW_LOCK(&idctrl.lock);
    INIT_LIST_HEAD(&idctrl.domains);

    return VMM_OK;
}

/* For future use */
const struct vmm_host_irq_domain_ops irq_domain_ops = {
    /* .xlate = extirq_xlate, */
};
