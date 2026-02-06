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
 * @file vmm_device_driver.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Device driver framework source
 */

#include <libs/stringlib.h>
#include <vmm_compiler.h>
#include <vmm_device_driver.h>
#include <vmm_device_resource.h>
#include <vmm_error.h>
#include <vmm_mutex.h>
#include <vmm_platform.h>
#include <vmm_stdio.h>
#include <vmm_workqueue.h>

struct vmm_device_driver_ctrl {
    vmm_mutex_t   class_lock;
    double_list_t class_list;
    vmm_mutex_t   bus_lock;
    double_list_t bus_list;

    vmm_mutex_t   deferred_probe_lock;
    double_list_t deferred_probe_list;
    vmm_work_t    deferred_probe_work;
};

static struct vmm_device_driver_ctrl ddctrl;

static void __bus_probe_this_device(vmm_bus_t *bus, vmm_device_t *dev);

static void deferred_probe_work_func(vmm_work_t *work)
{
    uint32_t      dcount;
    vmm_device_t *d;

    vmm_mutex_lock(&ddctrl.deferred_probe_lock);

    dcount = 0;
    list_for_each_entry(d, &ddctrl.deferred_probe_list, deferred_head)
    {
        dcount++;
    }

    while (dcount && !list_empty(&ddctrl.deferred_probe_list)) {
        d = list_first_entry(&ddctrl.deferred_probe_list, vmm_device_t, deferred_head);
        list_del_init(&d->deferred_head);
        dcount--;

        vmm_mutex_unlock(&ddctrl.deferred_probe_lock);

        if (d->bus) {
            vmm_mutex_lock(&d->bus->lock);
            __bus_probe_this_device(d->bus, d);
            vmm_mutex_unlock(&d->bus->lock);
        }

        vmm_mutex_lock(&ddctrl.deferred_probe_lock);
    }

    vmm_mutex_unlock(&ddctrl.deferred_probe_lock);
}

static void deferred_probe_invoke(void)
{
    if (!vmm_workqueue_work_inprogress(&ddctrl.deferred_probe_work)) {
        vmm_workqueue_schedule_work(NULL, &ddctrl.deferred_probe_work);
    }
}

static void deferred_probe_add(vmm_device_t *dev)
{
    bool          found = FALSE;
    vmm_device_t *d;

    vmm_mutex_lock(&ddctrl.deferred_probe_lock);

    found = FALSE;
    list_for_each_entry(d, &ddctrl.deferred_probe_list, deferred_head)
    {
        if (d == dev) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        list_add_tail(&dev->deferred_head, &ddctrl.deferred_probe_list);
    }

    vmm_mutex_unlock(&ddctrl.deferred_probe_lock);

    if (!found) {
        deferred_probe_invoke();
    }
}

static void deferred_probe_del(vmm_device_t *dev)
{
    vmm_device_t *d;

    vmm_mutex_lock(&ddctrl.deferred_probe_lock);

    list_for_each_entry(d, &ddctrl.deferred_probe_list, deferred_head)
    {
        if (d == dev) {
            list_del(&dev->deferred_head);
            break;
        }
    }

    vmm_mutex_unlock(&ddctrl.deferred_probe_lock);
}

/* Note: Must be called with bus->lock held */
static int __bus_probe_device_driver(vmm_bus_t *bus, vmm_device_t *dev, vmm_driver_t *drv)
{
    int rc = VMM_OK;

    /* Device should be registered but not having any driver */
    if (!dev->is_registered || dev->autoprobe_disabled || dev->driver) {
        /* Note: we return OK so that caller
         * does not try more drivers
         */
        return VMM_OK;
    }

    /* Device should match the driver */
    if (bus->match && !bus->match(dev, drv)) {
        return VMM_ENODEV;
    }

    /* Notify bus event listeners */
    vmm_blocking_notifier_call(&bus->event_listeners, VMM_BUS_NOTIFY_BIND_DRIVER, dev);

    /* If bus probe is available then device should
     * probe without failure
     */
    dev->driver = drv;

    if (bus->probe) {
#if defined(CONFIG_VERBOSE_MODE)
        vmm_printf(
            "devdrv: bus=\"%s\" device=\"%s\" "
            "driver=\"%s\" bus probe.\n",
            bus->name, dev->name, dev->driver->name);
#endif
        rc = bus->probe(dev);
    } else if (drv->probe) {
#if defined(CONFIG_VERBOSE_MODE)
        vmm_printf(
            "devdrv: bus=\"%s\" device=\"%s\" "
            "driver=\"%s\" probe.\n",
            bus->name, dev->name, dev->driver->name);
#endif
        rc = drv->probe(dev);
    }

    if (rc) {
#if defined(CONFIG_VERBOSE_MODE)

        if (rc != VMM_EPROBE_DEFER) {
            vmm_printf(
                "devdrv: bus=\"%s\" device=\"%s\" "
                "probe error %d\n",
                bus->name, dev->name, rc);
        }

#endif
        dev->driver = NULL;
        vmm_device_resource_release_all(dev);
    } else {
        /* Notify bus event listeners */
        vmm_blocking_notifier_call(&bus->event_listeners, VMM_BUS_NOTIFY_BOUND_DRIVER, dev);
    }

    return rc;
}

/* Note: Must be called with bus->lock held */
static void __bus_remove_device_driver(vmm_bus_t *bus, vmm_device_t *dev)
{
    int rc = VMM_OK;

    /* Device should be registered and having a driver */
    if (!dev->is_registered || !dev->driver) {
        return;
    }

    /* Notify bus event listeners */
    vmm_blocking_notifier_call(&bus->event_listeners, VMM_BUS_NOTIFY_UNBIND_DRIVER, dev);

    if (bus->remove) {
#if defined(CONFIG_VERBOSE_MODE)
        vmm_printf(
            "devdrv: bus=\"%s\" device=\"%s\" "
            "driver=\"%s\" bus remove.\n",
            bus->name, dev->name, dev->driver->name);
#endif
        rc = bus->remove(dev);
    } else if (dev->driver->remove) {
#if defined(CONFIG_VERBOSE_MODE)
        vmm_printf(
            "devdrv: bus=\"%s\" device=\"%s\" "
            "driver=\"%s\" remove.\n",
            bus->name, dev->name, dev->driver->name);
#endif
        rc = dev->driver->remove(dev);
    }

    if (rc) {
        vmm_printf(
            "devdrv: bus=\"%s\" device=\"%s\" "
            "remove error %d\n",
            bus->name, dev->name, rc);
    } else {
        /* Notify bus event listeners */
        vmm_blocking_notifier_call(&bus->event_listeners, VMM_BUS_NOTIFY_UNBOUND_DRIVER, dev);
    }

    /* Purge all managed resources */
    rc = vmm_device_resource_release_all(dev);

    if (rc) {
        vmm_printf(
            "devdrv: bus=\"%s\" device=\"%s\" "
            "resource remove all error %d\n",
            bus->name, dev->name, rc);
    }

    dev->driver = NULL;
}

/* Note: Must be called with bus->lock held */
static void __bus_shutdown_device_driver(vmm_bus_t *bus, vmm_device_t *dev)
{
    if (bus->shutdown) {
#if defined(CONFIG_VERBOSE_MODE)
        vmm_printf(
            "devdrv: bus=\"%s\" device=\"%s\" "
            "shutdown\n",
            bus->name, dev->name);
#endif
        bus->shutdown(dev);
    }
}

/* Note: Must be called with bus->lock held */
static void __bus_probe_this_device(vmm_bus_t *bus, vmm_device_t *dev)
{
    int           rc = VMM_OK;
    vmm_driver_t *drv;

    /* Try each and every driver of this bus */
    list_for_each_entry(drv, &bus->driver_list, head)
    {
        rc = __bus_probe_device_driver(bus, dev, drv);

        if (!rc || rc == VMM_EPROBE_DEFER) {
            break;
        }
    }

    /* Defer device probing if rc == VMM_EPROBE_DEFER */
    if (rc == VMM_EPROBE_DEFER) {
        /* Add device to deferred list */
        deferred_probe_add(dev);
    }
}

/* Note: Must be called with bus->lock held */
static void __bus_remove_this_device(vmm_bus_t *bus, vmm_device_t *dev)
{
    /* Remove device from deferred list */
    deferred_probe_del(dev);

    __bus_remove_device_driver(bus, dev);
}

/* Note: Must be called with bus->lock held */
static void __bus_shutdown_this_device(vmm_bus_t *bus, vmm_device_t *dev)
{
    /* Remove device from deferred list */
    deferred_probe_del(dev);

    __bus_shutdown_device_driver(bus, dev);
}

/* Note: Must be called with bus->lock held */
static void __bus_probe_this_driver(vmm_bus_t *bus, vmm_driver_t *drv)
{
    int           rc;
    vmm_device_t *dev;

    /* Try each and every device of this bus */
    list_for_each_entry(dev, &bus->device_list, bus_head)
    {
        /* If already probed then continue */
        if (dev->driver) {
            continue;
        }

        rc = __bus_probe_device_driver(bus, dev, drv);

        if (rc == VMM_EPROBE_DEFER) {
            /* Add device to deferred list */
            deferred_probe_add(dev);
        }
    }

    /* Invoke deferred device probing */
    deferred_probe_invoke();
}

/* Note: Must be called with bus->lock held */
static void __bus_remove_this_driver(vmm_bus_t *bus, vmm_driver_t *drv)
{
    vmm_device_t *dev;

    /* Try each and every device of this bus */
    list_for_each_entry(dev, &bus->device_list, bus_head)
    {
        /* If device not probed with this driver then continue */
        if (dev->driver != drv) {
            continue;
        }

        __bus_remove_device_driver(bus, dev);
    }
}

/* Note: Must be called with bus->lock held */
static void __bus_shutdown(vmm_bus_t *bus)
{
    vmm_device_t *d;

    /* Forcefully destroy all devices */
    while (!list_empty(&bus->device_list)) {
        d = list_first_entry(&bus->device_list, vmm_device_t, bus_head);

        /* Bus shutdown/cleanup this device */
        __bus_shutdown_this_device(bus, d);

        /* Notify bus event listeners */
        vmm_blocking_notifier_call(&bus->event_listeners, VMM_BUS_NOTIFY_DEL_DEVICE, d);

        /* Update parent child list */
        if (d->parent) {
            vmm_mutex_lock(&d->parent->child_list_lock);
            list_del(&d->child_head);
            vmm_mutex_unlock(&d->parent->child_list_lock);
            vmm_device_driver_dref_device(d->parent);
            d->parent = NULL;
        }

        /* Unregister from device list */
        list_del(&d->bus_head);
        d->is_registered = FALSE;

        /* Decrement reference count of device */
        vmm_device_driver_dref_device(d);
    }
}

/* Note: Must be called with cls->lock held */
static void __class_release(vmm_class_t *cls)
{
    double_list_t *l;
    vmm_device_t  *d;

    /* Forcefully destroy all devices */
    while (!list_empty(&cls->device_list)) {
        l = list_first(&cls->device_list);
        d = list_entry(l, vmm_device_t, class_head);

        /* Update parent child list */
        if (d->parent) {
            vmm_mutex_lock(&d->parent->child_list_lock);
            list_del(&d->child_head);
            vmm_mutex_unlock(&d->parent->child_list_lock);
            vmm_device_driver_dref_device(d->parent);
            d->parent = NULL;
        }

        /* Update class device list */
        list_del(&d->class_head);
        d->is_registered = FALSE;

        /* Decrement reference count of device */
        vmm_device_driver_dref_device(d);
    }
}

int vmm_device_driver_register_class(vmm_class_t *cls)
{
    bool         found;
    vmm_class_t *c;

    if (cls == NULL) {
        return VMM_EFAIL;
    }

    c     = NULL;
    found = FALSE;

    vmm_mutex_lock(&ddctrl.class_lock);

    list_for_each_entry(c, &ddctrl.class_list, head)
    {
        if (strcmp(c->name, cls->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&ddctrl.class_lock);
        return VMM_EINVALID;
    }

    INIT_LIST_HEAD(&cls->head);
    INIT_MUTEX(&cls->lock);
    INIT_LIST_HEAD(&cls->device_list);

    list_add_tail(&cls->head, &ddctrl.class_list);

    vmm_mutex_unlock(&ddctrl.class_lock);

    return VMM_OK;
}

int vmm_device_driver_unregister_class(vmm_class_t *cls)
{
    bool         found;
    vmm_class_t *c;

    vmm_mutex_lock(&ddctrl.class_lock);

    if (cls == NULL || list_empty(&ddctrl.class_list)) {
        vmm_mutex_unlock(&ddctrl.class_lock);
        return VMM_EFAIL;
    }

    c     = NULL;
    found = FALSE;
    list_for_each_entry(c, &ddctrl.class_list, head)
    {
        if (strcmp(c->name, cls->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_mutex_unlock(&ddctrl.class_lock);
        return VMM_ENOTAVAIL;
    }

    /* Clean release to nuke all devices */
    vmm_mutex_lock(&c->lock);
    __class_release(c);
    vmm_mutex_unlock(&c->lock);

    list_del(&c->head);

    vmm_mutex_unlock(&ddctrl.class_lock);

    return VMM_OK;
}

vmm_class_t *vmm_device_driver_find_class(const char *cname)
{
    bool         found;
    vmm_class_t *c;

    if (!cname) {
        return NULL;
    }

    found = FALSE;
    c     = NULL;

    vmm_mutex_lock(&ddctrl.class_lock);

    list_for_each_entry(c, &ddctrl.class_list, head)
    {
        if (strcmp(c->name, cname) == 0) {
            found = TRUE;
            break;
        }
    }

    vmm_mutex_unlock(&ddctrl.class_lock);

    if (!found) {
        return NULL;
    }

    return c;
}

int vmm_device_driver_class_iterate(vmm_class_t *start, void *data, int (*fn)(vmm_class_t *cls, void *data))
{
    int          rc          = VMM_OK;
    bool         start_found = (start) ? FALSE : TRUE;
    vmm_class_t *c           = NULL;

    if (!fn) {
        return VMM_EINVALID;
    }

    if (start) {
        return VMM_EINVALID;
    }

    vmm_mutex_lock(&ddctrl.class_lock);

    list_for_each_entry(c, &ddctrl.class_list, head)
    {
        if (!start_found) {
            if (start && start == c) {
                start_found = TRUE;
            } else {
                continue;
            }
        }

        rc = fn(c, data);

        if (rc) {
            break;
        }
    }

    vmm_mutex_unlock(&ddctrl.class_lock);

    return rc;
}

uint32_t vmm_device_driver_class_count(void)
{
    uint32_t     retval;
    vmm_class_t *c;

    retval = 0;

    vmm_mutex_lock(&ddctrl.class_lock);

    list_for_each_entry(c, &ddctrl.class_list, head)
    {
        retval++;
    }

    vmm_mutex_unlock(&ddctrl.class_lock);

    return retval;
}

static int devdrv_class_register_device(vmm_class_t *cls, vmm_device_t *dev)
{
    bool          found;
    vmm_device_t *d;

    if (!dev || !cls || (dev->class != cls)) {
        return VMM_EFAIL;
    }

    d     = NULL;
    found = FALSE;

    vmm_mutex_lock(&cls->lock);

    /* Check duplicacy */
    list_for_each_entry(d, &cls->device_list, class_head)
    {
        if ((strcmp(d->name, dev->name) == 0) && (d->parent == dev->parent)) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&cls->lock);
        return VMM_EINVALID;
    }

    /* Update class device list */
    INIT_LIST_HEAD(&dev->class_head);
    vmm_device_driver_ref_device(dev);
    list_add_tail(&dev->class_head, &cls->device_list);
    dev->is_registered = TRUE;

    /* Update parent child list */
    if (dev->parent) {
        vmm_device_driver_ref_device(dev->parent);
        vmm_mutex_lock(&dev->parent->child_list_lock);
        list_add_tail(&dev->child_head, &dev->parent->child_list);
        vmm_mutex_unlock(&dev->parent->child_list_lock);
    }

    vmm_mutex_unlock(&cls->lock);

    return VMM_OK;
}

static int devdrv_class_unregister_device(vmm_class_t *cls, vmm_device_t *dev)
{
    bool          found;
    vmm_device_t *d;

    if (!dev || !cls || (dev->class != cls)) {
        return VMM_EFAIL;
    }

    vmm_mutex_lock(&cls->lock);

    if (list_empty(&cls->device_list)) {
        vmm_mutex_unlock(&cls->lock);
        return VMM_EFAIL;
    }

    /* Check existance */
    d     = NULL;
    found = FALSE;
    list_for_each_entry(d, &cls->device_list, class_head)
    {
        if (strcmp(d->name, dev->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_mutex_unlock(&cls->lock);
        return VMM_ENOTAVAIL;
    }

    /* Update parent child list */
    if (d->parent) {
        vmm_mutex_lock(&d->parent->child_list_lock);
        list_del(&d->child_head);
        vmm_mutex_unlock(&d->parent->child_list_lock);
        vmm_device_driver_dref_device(d->parent);
        d->parent = NULL;
    }

    /* Update class device list */
    list_del(&d->class_head);
    d->is_registered = FALSE;

    /* Decrement reference count of device */
    vmm_device_driver_dref_device(d);

    vmm_mutex_unlock(&cls->lock);

    return VMM_OK;
}

vmm_device_t *vmm_device_driver_class_find_device(vmm_class_t *cls, void *data, int (*match)(vmm_device_t *, void *))
{
    bool          found;
    vmm_device_t *d;

    if (!cls || !match) {
        return NULL;
    }

    found = FALSE;
    d     = NULL;

    vmm_mutex_lock(&cls->lock);

    list_for_each_entry(d, &cls->device_list, class_head)
    {
        if (match(d, data)) {
            found = TRUE;
            break;
        }
    }

    vmm_mutex_unlock(&cls->lock);

    if (!found) {
        return NULL;
    }

    return d;
}

vmm_device_t *vmm_device_driver_class_find_device_by_name(vmm_class_t *cls, const char *dname)
{
    bool          found;
    vmm_device_t *d;

    if (!cls || !dname) {
        return NULL;
    }

    found = FALSE;
    d     = NULL;

    vmm_mutex_lock(&cls->lock);

    list_for_each_entry(d, &cls->device_list, class_head)
    {
        if (strcmp(d->name, dname) == 0) {
            found = TRUE;
            break;
        }
    }

    vmm_mutex_unlock(&cls->lock);

    if (!found) {
        return NULL;
    }

    return d;
}

int vmm_device_driver_class_device_iterate(vmm_class_t *cls, vmm_device_t *start, void *data, int (*fn)(vmm_device_t *dev, void *data))
{
    int           rc          = VMM_OK;
    bool          start_found = (start) ? FALSE : TRUE;
    vmm_device_t *d           = NULL;

    if (!cls || !fn) {
        return VMM_EINVALID;
    }

    if (start && start->class != cls) {
        return VMM_EINVALID;
    }

    vmm_mutex_lock(&cls->lock);

    list_for_each_entry(d, &cls->device_list, class_head)
    {
        if (!start_found) {
            if (start && start == d) {
                start_found = TRUE;
            } else {
                continue;
            }
        }

        rc = fn(d, data);

        if (rc) {
            break;
        }
    }

    vmm_mutex_unlock(&cls->lock);

    return rc;
}

uint32_t vmm_device_driver_class_device_count(vmm_class_t *cls)
{
    uint32_t      retval;
    vmm_device_t *d;

    if (!cls) {
        return 0;
    }

    retval = 0;

    vmm_mutex_lock(&cls->lock);

    list_for_each_entry(d, &cls->device_list, class_head)
    {
        retval++;
    }

    vmm_mutex_unlock(&cls->lock);

    return retval;
}

int vmm_device_driver_register_bus(vmm_bus_t *bus)
{
    bool       found;
    vmm_bus_t *b;

    if (bus == NULL) {
        return VMM_EFAIL;
    }

    b     = NULL;
    found = FALSE;

    vmm_mutex_lock(&ddctrl.bus_lock);

    list_for_each_entry(b, &ddctrl.bus_list, head)
    {
        if (strcmp(b->name, bus->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&ddctrl.bus_lock);
        return VMM_EINVALID;
    }

    INIT_LIST_HEAD(&bus->head);
    INIT_MUTEX(&bus->lock);
    INIT_LIST_HEAD(&bus->device_list);
    INIT_LIST_HEAD(&bus->driver_list);
    BLOCKING_INIT_NOTIFIER_CHAIN(&bus->event_listeners);

    list_add_tail(&bus->head, &ddctrl.bus_list);

    vmm_mutex_unlock(&ddctrl.bus_lock);

    return VMM_OK;
}

int vmm_device_driver_unregister_bus(vmm_bus_t *bus)
{
    bool       found;
    vmm_bus_t *b;

    vmm_mutex_lock(&ddctrl.bus_lock);

    if (bus == NULL || list_empty(&ddctrl.bus_list)) {
        vmm_mutex_unlock(&ddctrl.bus_lock);
        return VMM_EFAIL;
    }

    b     = NULL;
    found = FALSE;
    list_for_each_entry(b, &ddctrl.bus_list, head)
    {
        if (strcmp(b->name, bus->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_mutex_unlock(&ddctrl.bus_lock);
        return VMM_ENOTAVAIL;
    }

    vmm_mutex_lock(&b->lock);

    /* Bus shutdown to nuke all devices */
    __bus_shutdown(b);

    vmm_mutex_unlock(&b->lock);

    list_del(&b->head);

    vmm_mutex_unlock(&ddctrl.bus_lock);

    return VMM_OK;
}

vmm_bus_t *vmm_device_driver_find_bus(const char *bname)
{
    bool       found;
    vmm_bus_t *b;

    if (!bname) {
        return NULL;
    }

    found = FALSE;
    b     = NULL;

    vmm_mutex_lock(&ddctrl.bus_lock);

    list_for_each_entry(b, &ddctrl.bus_list, head)
    {
        if (strcmp(b->name, bname) == 0) {
            found = TRUE;
            break;
        }
    }

    vmm_mutex_unlock(&ddctrl.bus_lock);

    if (!found) {
        return NULL;
    }

    return b;
}

int vmm_device_driver_bus_iterate(vmm_bus_t *start, void *data, int (*fn)(vmm_bus_t *bus, void *data))
{
    int        rc          = VMM_OK;
    bool       start_found = (start) ? FALSE : TRUE;
    vmm_bus_t *b           = NULL;

    if (!fn) {
        return VMM_EINVALID;
    }

    if (start) {
        return VMM_EINVALID;
    }

    vmm_mutex_lock(&ddctrl.bus_lock);

    list_for_each_entry(b, &ddctrl.bus_list, head)
    {
        if (!start_found) {
            if (start && start == b) {
                start_found = TRUE;
            } else {
                continue;
            }
        }

        rc = fn(b, data);

        if (rc) {
            break;
        }
    }

    vmm_mutex_unlock(&ddctrl.bus_lock);

    return rc;
}

uint32_t vmm_device_driver_bus_count(void)
{
    uint32_t   retval;
    vmm_bus_t *b;

    retval = 0;

    vmm_mutex_lock(&ddctrl.bus_lock);

    list_for_each_entry(b, &ddctrl.bus_list, head)
    {
        retval++;
    }

    vmm_mutex_unlock(&ddctrl.bus_lock);

    return retval;
}

static int devdrv_bus_register_device(vmm_bus_t *bus, vmm_device_t *dev)
{
    bool          found;
    vmm_device_t *d;

    if (!dev || !bus || (dev->bus != bus)) {
        return VMM_EFAIL;
    }

    d     = NULL;
    found = FALSE;

    vmm_mutex_lock(&bus->lock);

    /* Check duplicacy */
    list_for_each_entry(d, &bus->device_list, bus_head)
    {
        if ((strcmp(d->name, dev->name) == 0) && (d->parent == dev->parent)) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&bus->lock);
        return VMM_EINVALID;
    }

    /* Update bus device list */
    INIT_LIST_HEAD(&dev->bus_head);
    vmm_device_driver_ref_device(dev);
    list_add_tail(&dev->bus_head, &bus->device_list);
    dev->is_registered = TRUE;

    /* Update parent child list */
    if (dev->parent) {
        vmm_device_driver_ref_device(dev->parent);
        vmm_mutex_lock(&dev->parent->child_list_lock);
        list_add_tail(&dev->child_head, &dev->parent->child_list);
        vmm_mutex_unlock(&dev->parent->child_list_lock);
    }

    /* Notify bus event listeners */
    vmm_blocking_notifier_call(&bus->event_listeners, VMM_BUS_NOTIFY_ADD_DEVICE, dev);

    /* Bus probe this device */
    __bus_probe_this_device(bus, dev);

    vmm_mutex_unlock(&bus->lock);

    return VMM_OK;
}

static int devdrv_bus_unregister_device(vmm_bus_t *bus, vmm_device_t *dev)
{
    bool          found;
    vmm_device_t *d;

    if (!dev || !bus || (dev->bus != bus)) {
        return VMM_EFAIL;
    }

    vmm_mutex_lock(&bus->lock);

    if (list_empty(&bus->device_list)) {
        vmm_mutex_unlock(&bus->lock);
        return VMM_EFAIL;
    }

    /* Check existance */
    d     = NULL;
    found = FALSE;
    list_for_each_entry(d, &bus->device_list, bus_head)
    {
        if (strcmp(d->name, dev->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_mutex_unlock(&bus->lock);
        return VMM_ENOTAVAIL;
    }

    /* Bus remove this device */
    __bus_remove_this_device(bus, d);

    /* Notify bus event listeners */
    vmm_blocking_notifier_call(&bus->event_listeners, VMM_BUS_NOTIFY_DEL_DEVICE, d);

    /* Update parent child list */
    if (d->parent) {
        vmm_mutex_lock(&d->parent->child_list_lock);
        list_del(&d->child_head);
        vmm_mutex_unlock(&d->parent->child_list_lock);
        vmm_device_driver_dref_device(d->parent);
        d->parent = NULL;
    }

    /* Update bus device list */
    list_del(&d->bus_head);
    d->is_registered = FALSE;

    /* Decrement reference count of device */
    vmm_device_driver_dref_device(d);

    vmm_mutex_unlock(&bus->lock);

    return VMM_OK;
}

vmm_device_t *vmm_device_driver_bus_find_device(vmm_bus_t *bus, vmm_device_t *start, void *data, int (*match)(vmm_device_t *, void *))
{
    bool          found       = FALSE;
    bool          start_found = (start) ? FALSE : TRUE;
    vmm_device_t *d           = NULL;

    if (!bus || !match) {
        return NULL;
    }

    vmm_mutex_lock(&bus->lock);

    list_for_each_entry(d, &bus->device_list, bus_head)
    {
        if (!start_found) {
            if (start && start == d) {
                start_found = TRUE;
            } else {
                continue;
            }
        }

        if (match(d, data)) {
            found = TRUE;
            break;
        }
    }

    vmm_mutex_unlock(&bus->lock);

    if (!found) {
        return NULL;
    }

    return d;
}

static int devdrv_name_match(vmm_device_t *dev, void *data)
{
    return (strcmp(dev->name, (const char *)data) == 0) ? 1 : 0;
}

vmm_device_t *vmm_device_driver_bus_find_device_by_name(vmm_bus_t *bus, vmm_device_t *start, const char *dname)
{
    return vmm_device_driver_bus_find_device(bus, start, (void *)dname, devdrv_name_match);
}

static int devdrv_node_match(vmm_device_t *dev, void *data)
{
    return (dev->of_node == data) ? 1 : 0;
}

vmm_device_t *vmm_device_driver_bus_find_device_by_node(vmm_bus_t *bus, vmm_device_t *start, vmm_device_tree_node_t *np)
{
    return vmm_device_driver_bus_find_device(bus, start, (void *)np, devdrv_node_match);
}

int vmm_device_driver_bus_device_iterate(vmm_bus_t *bus, vmm_device_t *start, void *data, int (*fn)(vmm_device_t *dev, void *data))
{
    int           rc          = VMM_OK;
    bool          start_found = (start) ? FALSE : TRUE;
    vmm_device_t *d           = NULL;

    if (!bus || !fn) {
        return VMM_EINVALID;
    }

    if (start && start->bus != bus) {
        return VMM_EINVALID;
    }

    vmm_mutex_lock(&bus->lock);

    list_for_each_entry(d, &bus->device_list, bus_head)
    {
        if (!start_found) {
            if (start && start == d) {
                start_found = TRUE;
            } else {
                continue;
            }
        }

        rc = fn(d, data);

        if (rc) {
            break;
        }
    }

    vmm_mutex_unlock(&bus->lock);

    return rc;
}

uint32_t vmm_device_driver_bus_device_count(vmm_bus_t *bus)
{
    uint32_t      retval;
    vmm_device_t *d;

    if (!bus) {
        return 0;
    }

    retval = 0;

    vmm_mutex_lock(&bus->lock);

    list_for_each_entry(d, &bus->device_list, bus_head)
    {
        retval++;
    }

    vmm_mutex_unlock(&bus->lock);

    return retval;
}

int vmm_device_driver_bus_register_driver(vmm_bus_t *bus, vmm_driver_t *drv)
{
    bool          found;
    vmm_driver_t *d;

    if (!drv || !bus || (drv->bus != bus)) {
        return VMM_EFAIL;
    }

    d     = NULL;
    found = FALSE;

    vmm_mutex_lock(&bus->lock);

    list_for_each_entry(d, &bus->driver_list, head)
    {
        if (strcmp(d->name, drv->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&bus->lock);
        return VMM_EINVALID;
    }

    INIT_LIST_HEAD(&drv->head);
    list_add_tail(&drv->head, &bus->driver_list);

    /* Bus probe this driver */
    __bus_probe_this_driver(bus, drv);

    vmm_mutex_unlock(&bus->lock);

    return VMM_OK;
}

int vmm_device_driver_bus_unregister_driver(vmm_bus_t *bus, vmm_driver_t *drv)
{
    bool          found;
    vmm_driver_t *d;

    if (!drv || !bus || (drv->bus != bus)) {
        return VMM_EFAIL;
    }

    vmm_mutex_lock(&bus->lock);

    if (list_empty(&bus->driver_list)) {
        vmm_mutex_unlock(&bus->lock);
        return VMM_EFAIL;
    }

    d     = NULL;
    found = FALSE;
    list_for_each_entry(d, &bus->driver_list, head)
    {
        if (strcmp(d->name, drv->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_mutex_unlock(&bus->lock);
        return VMM_ENOTAVAIL;
    }

    list_del(&d->head);

    /* Bus remove this driver */
    __bus_remove_this_driver(bus, d);

    vmm_mutex_unlock(&bus->lock);

    return VMM_OK;
}

vmm_driver_t *vmm_device_driver_bus_find_driver(vmm_bus_t *bus, const char *dname)
{
    bool          found;
    vmm_driver_t *d;

    if (!bus || !dname) {
        return NULL;
    }

    found = FALSE;
    d     = NULL;

    vmm_mutex_lock(&bus->lock);

    list_for_each_entry(d, &bus->driver_list, head)
    {
        if (strcmp(d->name, dname) == 0) {
            found = TRUE;
            break;
        }
    }

    vmm_mutex_unlock(&bus->lock);

    if (!found) {
        return NULL;
    }

    return d;
}

int vmm_device_driver_bus_driver_iterate(vmm_bus_t *bus, vmm_driver_t *start, void *data, int (*fn)(vmm_driver_t *drv, void *data))
{
    int           rc          = VMM_OK;
    bool          start_found = (start) ? FALSE : TRUE;
    vmm_driver_t *d           = NULL;

    if (!bus || !fn) {
        return VMM_EINVALID;
    }

    if (start && start->bus != bus) {
        return VMM_EINVALID;
    }

    vmm_mutex_lock(&bus->lock);

    list_for_each_entry(d, &bus->driver_list, head)
    {
        if (!start_found) {
            if (start && start == d) {
                start_found = TRUE;
            } else {
                continue;
            }
        }

        rc = fn(d, data);

        if (rc) {
            break;
        }
    }

    vmm_mutex_unlock(&bus->lock);

    return rc;
}

uint32_t vmm_device_driver_bus_driver_count(vmm_bus_t *bus)
{
    uint32_t      retval;
    vmm_driver_t *d;

    if (!bus) {
        return 0;
    }

    retval = 0;

    vmm_mutex_lock(&bus->lock);

    list_for_each_entry(d, &bus->driver_list, head)
    {
        retval++;
    }

    vmm_mutex_unlock(&bus->lock);

    return retval;
}

int vmm_device_driver_bus_register_notifier(vmm_bus_t *bus, vmm_notifier_block_t *nb)
{
    if (!bus || !nb) {
        return VMM_EINVALID;
    }

    return vmm_blocking_notifier_register(&bus->event_listeners, nb);
}

int vmm_device_driver_bus_unregister_notifier(vmm_bus_t *bus, vmm_notifier_block_t *nb)
{
    if (!bus || !nb) {
        return VMM_EINVALID;
    }

    return vmm_blocking_notifier_unregister(&bus->event_listeners, nb);
}

void vmm_device_driver_initialize_device(vmm_device_t *dev)
{
    if (!dev) {
        return;
    }

    /* Only initialize the private fields of device */
    INIT_LIST_HEAD(&dev->bus_head);
    INIT_LIST_HEAD(&dev->class_head);
    xref_init(&dev->ref_count);
    dev->is_registered = FALSE;
    INIT_LIST_HEAD(&dev->child_head);
    INIT_MUTEX(&dev->child_list_lock);
    INIT_LIST_HEAD(&dev->child_list);
    INIT_SPIN_LOCK(&dev->device_resource_lock);
    INIT_LIST_HEAD(&dev->device_resource_head);
    INIT_LIST_HEAD(&dev->deferred_head);
    INIT_LIST_HEAD(&dev->msi_list);
    dev->msi_domain = NULL;
}

vmm_device_t *vmm_device_driver_ref_device(vmm_device_t *dev)
{
    if (!dev) {
        return NULL;
    }

    xref_get(&dev->ref_count);
    return dev;
}

static void __devdrv_device_free(struct xref *ref)
{
    bool          released;
    vmm_device_t *dev = container_of(ref, vmm_device_t, ref_count);

    released          = TRUE;

    if (dev->release) {
        dev->release(dev);
    } else if (dev->type && dev->type->release) {
        dev->type->release(dev);

    } else if (dev->class && dev->class->release) {
        dev->class->release(dev);
    } else {

        released = FALSE;
    }

    WARN_ON(!released);
}

void vmm_device_driver_dref_device(vmm_device_t *dev)
{
    if (dev) {
        xref_put(&dev->ref_count, __devdrv_device_free);
    }
}

bool vmm_device_driver_isregistered_device(vmm_device_t *dev)
{
    return (dev) ? dev->is_registered : FALSE;
}

bool vmm_device_driver_isattached_device(vmm_device_t *dev)
{
    return (dev) ? ((dev->driver) ? TRUE : FALSE) : FALSE;
}

int vmm_device_driver_for_each_child(vmm_device_t *dev, void *data, int (*fn)(vmm_device_t *dev, void *data))
{
    int           err   = 0;
    vmm_device_t *child = NULL;
    vmm_device_t *temp  = NULL;

    if (!dev) {
        return VMM_EFAIL;
    }

    vmm_mutex_lock(&dev->child_list_lock);

    list_for_each_entry_safe(child, temp, &dev->child_list, child_head)
    {
        if (VMM_OK != (err = fn(child, data))) {
            vmm_mutex_unlock(&dev->child_list_lock);
            return err;
        }
    }

    vmm_mutex_unlock(&dev->child_list_lock);

    return VMM_OK;
}

int vmm_device_driver_register_device(vmm_device_t *dev)
{
    if (dev && dev->bus && !dev->class) {
        return devdrv_bus_register_device(dev->bus, dev);

    } else if (dev && !dev->bus && dev->class) {
        return devdrv_class_register_device(dev->class, dev);
    }

    return VMM_EFAIL;
}

int vmm_device_driver_attach_device(vmm_device_t *dev)
{
    vmm_bus_t *bus;

    /* Device should be registered with a valid bus */
    if (!dev || !dev->is_registered || !dev->bus) {
        return VMM_EFAIL;
    }

    bus = dev->bus;

    vmm_mutex_lock(&bus->lock);

    /* Bus probe this device */
    __bus_probe_this_device(bus, dev);

    vmm_mutex_unlock(&bus->lock);

    return VMM_OK;
}

int vmm_device_driver_dettach_device(vmm_device_t *dev)
{
    vmm_bus_t *bus;

    /* Device should be registered with a valid bus */
    if (!dev || !dev->is_registered || !dev->bus) {
        return VMM_EFAIL;
    }

    bus = dev->bus;

    vmm_mutex_lock(&bus->lock);

    /* Bus remove this device */
    __bus_remove_this_device(bus, dev);

    vmm_mutex_unlock(&bus->lock);

    return VMM_OK;
}

int vmm_device_driver_unregister_device(vmm_device_t *dev)
{
    if (dev && dev->bus && !dev->class) {
        return devdrv_bus_unregister_device(dev->bus, dev);

    } else if (dev && !dev->bus && dev->class) {
        return devdrv_class_unregister_device(dev->class, dev);
    }

    return VMM_EFAIL;
}

int vmm_device_driver_register_driver(vmm_driver_t *drv)
{
    if (!drv) {
        return VMM_EFAIL;
    }

    if (!drv->bus) {
        drv->bus = &platform_bus;
    }

    return vmm_device_driver_bus_register_driver(drv->bus, drv);
}

int vmm_device_driver_attach_driver(vmm_driver_t *drv)
{
    vmm_bus_t *bus;

    if (!drv || !drv->bus) {
        return VMM_EFAIL;
    }

    bus = drv->bus;

    vmm_mutex_lock(&bus->lock);

    /* Bus probe this driver */
    __bus_probe_this_driver(bus, drv);

    vmm_mutex_unlock(&bus->lock);

    return VMM_OK;
}

int vmm_device_driver_dettach_driver(vmm_driver_t *drv)
{
    vmm_bus_t *bus;

    if (!drv || !drv->bus) {
        return VMM_EFAIL;
    }

    bus = drv->bus;

    vmm_mutex_lock(&bus->lock);

    /* Bus remove this driver */
    __bus_remove_this_driver(bus, drv);

    vmm_mutex_unlock(&bus->lock);

    return VMM_OK;
}

int vmm_device_driver_unregister_driver(vmm_driver_t *drv)
{
    if (!drv || !drv->bus) {
        return VMM_EFAIL;
    }

    return vmm_device_driver_bus_unregister_driver(drv->bus, drv);
}

int __init vmm_device_driver_init(void)
{
    memset(&ddctrl, 0, sizeof(ddctrl));

    INIT_MUTEX(&ddctrl.class_lock);
    INIT_LIST_HEAD(&ddctrl.class_list);
    INIT_MUTEX(&ddctrl.bus_lock);
    INIT_LIST_HEAD(&ddctrl.bus_list);

    INIT_MUTEX(&ddctrl.deferred_probe_lock);
    INIT_LIST_HEAD(&ddctrl.deferred_probe_list);
    INIT_WORK(&ddctrl.deferred_probe_work, deferred_probe_work_func);

    return vmm_device_driver_register_bus(&platform_bus);
}
