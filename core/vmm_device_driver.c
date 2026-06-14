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
 * @brief 设备驱动框架实现
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

/**
 * @brief 设备驱动子系统控制结构，管理总线、设备和驱动的注册状态
 */
struct vmm_device_driver_ctrl {
    vmm_mutex_t   class_lock; /**< 类锁 */
    double_list_t class_list; /**< 类链表 */
    vmm_mutex_t   bus_lock; /**< 总线锁 */
    double_list_t bus_list; /**< 总线链表 */

    vmm_mutex_t   deferred_probe_lock; /**< 延迟探测锁 */
    double_list_t deferred_probe_list; /**< 延迟探测链表 */
    vmm_work_t    deferred_probe_work; /**< 延迟探测工作项 */
};

static struct vmm_device_driver_ctrl ddctrl;

static void __bus_probe_this_device(vmm_bus_t *bus, vmm_device_t *dev);

/**
 * @brief 延迟探测工作项的回调处理函数
 * @param work 指向工作项结构体的指针
 */
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

/**
 * @brief 调用延迟探测函数
 */
static void deferred_probe_invoke(void)
{
    if (!vmm_workqueue_work_inprogress(&ddctrl.deferred_probe_work)) {
        vmm_workqueue_schedule_work(NULL, &ddctrl.deferred_probe_work);
    }
}

/**
 * @brief 将设备加入延迟探测队列
 * @param dev 设备结构体指针
 */
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

/**
 * @brief 将设备从延迟探测队列中移除
 * @param dev 设备结构体指针
 */
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
/**
 * @brief 尝试在总线上将设备与驱动进行匹配
 * @param bus 设备总线结构体指针
 * @param dev 设备结构体指针
 * @param drv 设备驱动结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
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
        return VMM_ERR_NODEV;
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

        if (rc != VMM_ERR_PROBE_DEFER) {
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
/**
 * @brief 从总线上移除设备与驱动的绑定
 * @param bus 设备总线结构体指针
 * @param dev 设备结构体指针
 */
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
/**
 * @brief 在总线上执行设备的关机操作
 * @param bus 设备总线结构体指针
 * @param dev 设备结构体指针
 */
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
/**
 * @brief 为指定设备在总线上查找匹配的驱动
 * @param bus 设备总线结构体指针
 * @param dev 设备结构体指针
 */
static void __bus_probe_this_device(vmm_bus_t *bus, vmm_device_t *dev)
{
    int           rc = VMM_OK;
    vmm_driver_t *drv;

    /* Try each and every driver of this bus */
    list_for_each_entry(drv, &bus->driver_list, head)
    {
        rc = __bus_probe_device_driver(bus, dev, drv);

        if (!rc || rc == VMM_ERR_PROBE_DEFER) {
            break;
        }
    }

    /* Defer device probing if rc == VMM_ERR_PROBE_DEFER */
    if (rc == VMM_ERR_PROBE_DEFER) {
        /* Add device to deferred list */
        deferred_probe_add(dev);
    }
}

/* Note: Must be called with bus->lock held */
/**
 * @brief 将指定设备从总线驱动上解绑
 * @param bus 设备总线结构体指针
 * @param dev 设备结构体指针
 */
static void __bus_remove_this_device(vmm_bus_t *bus, vmm_device_t *dev)
{
    /* Remove device from deferred list */
    deferred_probe_del(dev);

    __bus_remove_device_driver(bus, dev);
}

/* Note: Must be called with bus->lock held */
/**
 * @brief 对指定设备执行总线关机操作
 * @param bus 设备总线结构体指针
 * @param dev 设备结构体指针
 */
static void __bus_shutdown_this_device(vmm_bus_t *bus, vmm_device_t *dev)
{
    /* Remove device from deferred list */
    deferred_probe_del(dev);

    __bus_shutdown_device_driver(bus, dev);
}

/* Note: Must be called with bus->lock held */
/**
 * @brief 为指定驱动在总线上查找匹配的设备
 * @param bus 设备总线结构体指针
 * @param drv 设备驱动结构体指针
 */
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

        if (rc == VMM_ERR_PROBE_DEFER) {
            /* Add device to deferred list */
            deferred_probe_add(dev);
        }
    }

    /* Invoke deferred device probing */
    deferred_probe_invoke();
}

/* Note: Must be called with bus->lock held */
/**
 * @brief 从总线上移除指定驱动的所有绑定
 * @param bus 设备总线结构体指针
 * @param drv 设备驱动结构体指针
 */
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
/**
 * @brief   总线 关机
 * @param bus 设备总线结构体指针
 */
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
/**
 * @brief   设备类释放回调
 * @param cls 设备类结构体指针
 */
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

/**
 * @brief 注册设备类
 *
 * 此函数用于向设备驱动框架注册一个新的设备类。
 *
 * @param cls 指向要注册的设备类结构的指针
 * @return VMM_OK 注册成功，其他值表示错误
 */
int vmm_device_driver_register_class(vmm_class_t *cls)
{
    bool         found;
    vmm_class_t *c;

    if (cls == NULL) {
        return VMM_ERR_FAIL;
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
        return VMM_ERR_INVALID;
    }

    INIT_LIST_HEAD(&cls->head);
    INIT_MUTEX(&cls->lock);
    INIT_LIST_HEAD(&cls->device_list);

    list_add_tail(&cls->head, &ddctrl.class_list);

    vmm_mutex_unlock(&ddctrl.class_lock);

    return VMM_OK;
}

/**
 * @brief 注销设备类
 *
 * 此函数用于从设备驱动框架中注销一个设备类。
 *
 * @param cls 指向要注销的设备类结构的指针
 * @return VMM_OK 注销成功，其他值表示错误
 */
int vmm_device_driver_unregister_class(vmm_class_t *cls)
{
    bool         found;
    vmm_class_t *c;

    vmm_mutex_lock(&ddctrl.class_lock);

    if (cls == NULL || list_empty(&ddctrl.class_list)) {
        vmm_mutex_unlock(&ddctrl.class_lock);
        return VMM_ERR_FAIL;
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
        return VMM_ERR_NOTAVAIL;
    }

    /* Clean release to nuke all devices */
    vmm_mutex_lock(&c->lock);
    __class_release(c);
    vmm_mutex_unlock(&c->lock);

    list_del(&c->head);

    vmm_mutex_unlock(&ddctrl.class_lock);

    return VMM_OK;
}

/**
 * @brief 查找设备类
 *
 * 此函数根据名称查找已注册的设备类。
 *
 * @param cname 要查找的设备类名称
 * @return 找到的设备类指针，如果未找到则返回NULL
 */
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

/**
 * @brief 遍历设备类
 *
 * 此函数遍历所有已注册的设备类，并对每个设备类调用指定的回调函数。
 *
 * @param start 遍历起始的设备类指针，如果为NULL则从头开始
 * @param data 传递给回调函数的用户数据
 * @param fn 回调函数指针，参数为设备类和用户数据
 * @return VMM_OK 遍历完成，其他值表示错误或回调函数返回的错误
 */
int vmm_device_driver_class_iterate(vmm_class_t *start, void *data, int (*fn)(vmm_class_t *cls, void *data))
{
    int          rc          = VMM_OK;
    bool         start_found = (start) ? FALSE : TRUE;
    vmm_class_t *c           = NULL;

    if (!fn) {
        return VMM_ERR_INVALID;
    }

    if (start) {
        return VMM_ERR_INVALID;
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

/**
 * @brief 获取设备类的数量
 *
 * 此函数返回当前已注册的设备类总数。
 *
 * @return 已注册的设备类数量
 */
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

/**
 * @brief 在设备类中注册设备
 * @param cls 设备类结构体指针
 * @param dev 设备结构体指针
 * @return 数量值
 */
static int devdrv_class_register_device(vmm_class_t *cls, vmm_device_t *dev)
{
    bool          found;
    vmm_device_t *d;

    if (!dev || !cls || (dev->class != cls)) {
        return VMM_ERR_FAIL;
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
        return VMM_ERR_INVALID;
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

/**
 * @brief 从设备类中注销设备
 * @param cls 设备类结构体指针
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int devdrv_class_unregister_device(vmm_class_t *cls, vmm_device_t *dev)
{
    bool          found;
    vmm_device_t *d;

    if (!dev || !cls || (dev->class != cls)) {
        return VMM_ERR_FAIL;
    }

    vmm_mutex_lock(&cls->lock);

    if (list_empty(&cls->device_list)) {
        vmm_mutex_unlock(&cls->lock);
        return VMM_ERR_FAIL;
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
        return VMM_ERR_NOTAVAIL;
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

/**
 * @brief 在设备类中查找设备
 *
 * 此函数在指定的设备类中查找匹配的设备。
 *
 * @param cls 要查找的设备类指针
 * @param data 传递给匹配函数的用户数据
 * @param match 匹配函数指针，用于判断设备是否匹配
 * @return 找到的设备指针，如果未找到则返回NULL
 */
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

/**
 * @brief 根据名称在设备类中查找设备
 *
 * 此函数在指定的设备类中根据设备名称查找设备。
 *
 * @param cls 要查找的设备类指针
 * @param dname 要查找的设备名称
 * @return 找到的设备指针，如果未找到则返回NULL
 */
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

/**
 * @brief 遍历设备类中的设备
 *
 * 此函数遍历指定设备类中的所有设备，并对每个设备调用指定的回调函数。
 *
 * @param cls 要遍历的设备类指针
 * @param start 遍历起始的设备指针，如果为NULL则从头开始
 * @param data 传递给回调函数的用户数据
 * @param fn 回调函数指针，参数为设备和用户数据
 * @return VMM_OK 遍历完成，其他值表示错误或回调函数返回的错误
 */
int vmm_device_driver_class_device_iterate(vmm_class_t *cls, vmm_device_t *start, void *data, int (*fn)(vmm_device_t *dev, void *data))
{
    int           rc          = VMM_OK;
    bool          start_found = (start) ? FALSE : TRUE;
    vmm_device_t *d           = NULL;

    if (!cls || !fn) {
        return VMM_ERR_INVALID;
    }

    if (start && start->class != cls) {
        return VMM_ERR_INVALID;
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

/**
 * @brief 获取设备类中的设备的数量
 *
 * 此函数返回指定设备类中已注册的设备总数。
 *
 * @param cls 要查询的设备类指针
 * @return 设备类中的设备数量
 */
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

/**
 * @brief 注册总线
 *
 * 此函数用于向设备驱动框架注册一个新的总线。
 *
 * @param bus 指向要注册的总线结构的指针
 * @return VMM_OK 注册成功，其他值表示错误
 */
int vmm_device_driver_register_bus(vmm_bus_t *bus)
{
    bool       found;
    vmm_bus_t *b;

    if (bus == NULL) {
        return VMM_ERR_FAIL;
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
        return VMM_ERR_INVALID;
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

/**
 * @brief 注销总线
 *
 * 此函数用于从设备驱动框架中注销一个总线。
 *
 * @param bus 指向要注销的总线结构的指针
 * @return VMM_OK 注销成功，其他值表示错误
 */
int vmm_device_driver_unregister_bus(vmm_bus_t *bus)
{
    bool       found;
    vmm_bus_t *b;

    vmm_mutex_lock(&ddctrl.bus_lock);

    if (bus == NULL || list_empty(&ddctrl.bus_list)) {
        vmm_mutex_unlock(&ddctrl.bus_lock);
        return VMM_ERR_FAIL;
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
        return VMM_ERR_NOTAVAIL;
    }

    vmm_mutex_lock(&b->lock);

    /* Bus shutdown to nuke all devices */
    __bus_shutdown(b);

    vmm_mutex_unlock(&b->lock);

    list_del(&b->head);

    vmm_mutex_unlock(&ddctrl.bus_lock);

    return VMM_OK;
}

/**
 * @brief 查找总线
 *
 * 此函数根据名称查找已注册的总线。
 *
 * @param bname 要查找的总线名称
 * @return 找到的总线指针，如果未找到则返回NULL
 */
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

/**
 * @brief 遍历总线
 *
 * 此函数遍历所有已注册的总线，并对每个总线调用指定的回调函数。
 *
 * @param start 遍历起始的总线指针，如果为NULL则从头开始
 * @param data 传递给回调函数的用户数据
 * @param fn 回调函数指针，参数为总线和用户数据
 * @return VMM_OK 遍历完成，其他值表示错误或回调函数返回的错误
 */
int vmm_device_driver_bus_iterate(vmm_bus_t *start, void *data, int (*fn)(vmm_bus_t *bus, void *data))
{
    int        rc          = VMM_OK;
    bool       start_found = (start) ? FALSE : TRUE;
    vmm_bus_t *b           = NULL;

    if (!fn) {
        return VMM_ERR_INVALID;
    }

    if (start) {
        return VMM_ERR_INVALID;
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

/**
 * @brief 获取总线的数量
 *
 * 此函数返回当前已注册的总线总数。
 *
 * @return 已注册的总线数量
 */
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

/**
 * @brief 在设备总线上注册设备
 * @param bus 设备总线结构体指针
 * @param dev 设备结构体指针
 * @return 数量值
 */
static int devdrv_bus_register_device(vmm_bus_t *bus, vmm_device_t *dev)
{
    bool          found;
    vmm_device_t *d;

    if (!dev || !bus || (dev->bus != bus)) {
        return VMM_ERR_FAIL;
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
        return VMM_ERR_INVALID;
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

/**
 * @brief 从设备总线上注销设备
 * @param bus 设备总线结构体指针
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int devdrv_bus_unregister_device(vmm_bus_t *bus, vmm_device_t *dev)
{
    bool          found;
    vmm_device_t *d;

    if (!dev || !bus || (dev->bus != bus)) {
        return VMM_ERR_FAIL;
    }

    vmm_mutex_lock(&bus->lock);

    if (list_empty(&bus->device_list)) {
        vmm_mutex_unlock(&bus->lock);
        return VMM_ERR_FAIL;
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
        return VMM_ERR_NOTAVAIL;
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

/**
 * @brief 在总线上查找匹配条件的设备
 * @param bus 设备总线结构体指针
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*match 指针参数
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
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

/**
 * @brief 根据设备名称与驱动进行匹配
 * @param dev 设备结构体指针
 * @param data 用户自定义数据指针
 * @return 查找结果，失败返回错误码
 */
static int devdrv_name_match(vmm_device_t *dev, void *data)
{
    return (strcmp(dev->name, (const char *)data) == 0) ? 1 : 0;
}

/**
 * @brief 在总线上根据名称查找设备
 * @param bus 设备总线结构体指针
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param dname 设备名称字符串
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_t *vmm_device_driver_bus_find_device_by_name(vmm_bus_t *bus, vmm_device_t *start, const char *dname)
{
    return vmm_device_driver_bus_find_device(bus, start, (void *)dname, devdrv_name_match);
}

/**
 * @brief 根据设备树节点ID与驱动进行匹配
 * @param dev 设备结构体指针
 * @param data 用户自定义数据指针
 * @return 查找结果，失败返回错误码
 */
static int devdrv_node_match(vmm_device_t *dev, void *data)
{
    return (dev->of_node == data) ? 1 : 0;
}

/**
 * @brief 在总线上根据设备树节点查找设备
 * @param bus 设备总线结构体指针
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param np 设备树节点指针
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
vmm_device_t *vmm_device_driver_bus_find_device_by_node(vmm_bus_t *bus, vmm_device_t *start, vmm_device_tree_node_t *np)
{
    return vmm_device_driver_bus_find_device(bus, start, (void *)np, devdrv_node_match);
}

/**
 * @brief 遍历总线上的所有设备，对每个设备执行回调
 * @param bus 设备总线结构体指针
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_bus_device_iterate(vmm_bus_t *bus, vmm_device_t *start, void *data, int (*fn)(vmm_device_t *dev, void *data))
{
    int           rc          = VMM_OK;
    bool          start_found = (start) ? FALSE : TRUE;
    vmm_device_t *d           = NULL;

    if (!bus || !fn) {
        return VMM_ERR_INVALID;
    }

    if (start && start->bus != bus) {
        return VMM_ERR_INVALID;
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

/**
 * @brief 获取设备驱动总线设备的数量
 * @param bus 设备总线结构体指针
 * @return 数量值
 */
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

/**
 * @brief 在总线上注册设备驱动
 * @param bus 设备总线结构体指针
 * @param drv 设备驱动结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_bus_register_driver(vmm_bus_t *bus, vmm_driver_t *drv)
{
    bool          found;
    vmm_driver_t *d;

    if (!drv || !bus || (drv->bus != bus)) {
        return VMM_ERR_FAIL;
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
        return VMM_ERR_INVALID;
    }

    INIT_LIST_HEAD(&drv->head);
    list_add_tail(&drv->head, &bus->driver_list);

    /* Bus probe this driver */
    __bus_probe_this_driver(bus, drv);

    vmm_mutex_unlock(&bus->lock);

    return VMM_OK;
}

/**
 * @brief 从总线上注销设备驱动
 * @param bus 设备总线结构体指针
 * @param drv 设备驱动结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_bus_unregister_driver(vmm_bus_t *bus, vmm_driver_t *drv)
{
    bool          found;
    vmm_driver_t *d;

    if (!drv || !bus || (drv->bus != bus)) {
        return VMM_ERR_FAIL;
    }

    vmm_mutex_lock(&bus->lock);

    if (list_empty(&bus->driver_list)) {
        vmm_mutex_unlock(&bus->lock);
        return VMM_ERR_FAIL;
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
        return VMM_ERR_NOTAVAIL;
    }

    list_del(&d->head);

    /* Bus remove this driver */
    __bus_remove_this_driver(bus, d);

    vmm_mutex_unlock(&bus->lock);

    return VMM_OK;
}

/**
 * @brief 在总线上根据名称查找设备驱动
 * @param bus 设备总线结构体指针
 * @param dname 设备名称字符串
 * @return 成功返回匹配的对象指针，未找到返回NULL
 */
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

/**
 * @brief 遍历总线上的所有驱动，对每个驱动执行回调
 * @param bus 设备总线结构体指针
 * @param start 遍历起始节点（NULL表示从头开始）
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_bus_driver_iterate(vmm_bus_t *bus, vmm_driver_t *start, void *data, int (*fn)(vmm_driver_t *drv, void *data))
{
    int           rc          = VMM_OK;
    bool          start_found = (start) ? FALSE : TRUE;
    vmm_driver_t *d           = NULL;

    if (!bus || !fn) {
        return VMM_ERR_INVALID;
    }

    if (start && start->bus != bus) {
        return VMM_ERR_INVALID;
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

/**
 * @brief 获取设备驱动总线驱动的数量
 * @param bus 设备总线结构体指针
 * @return 数量值
 */
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

/**
 * @brief 在总线上注册设备事件通知器
 * @param bus 设备总线结构体指针
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_bus_register_notifier(vmm_bus_t *bus, vmm_notifier_block_t *nb)
{
    if (!bus || !nb) {
        return VMM_ERR_INVALID;
    }

    return vmm_blocking_notifier_register(&bus->event_listeners, nb);
}

/**
 * @brief 从总线上注销设备事件通知器
 * @param bus 设备总线结构体指针
 * @param nb 通知器块指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_bus_unregister_notifier(vmm_bus_t *bus, vmm_notifier_block_t *nb)
{
    if (!bus || !nb) {
        return VMM_ERR_INVALID;
    }

    return vmm_blocking_notifier_unregister(&bus->event_listeners, nb);
}

/**
 * @brief 初始化设备结构体的基本字段
 * @param dev 设备结构体指针
 */
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

/**
 * @brief 增加设备的引用计数并返回设备指针
 * @param dev 设备结构体指针
 * @return 增加引用计数后返回的节点指针
 */
vmm_device_t *vmm_device_driver_ref_device(vmm_device_t *dev)
{
    if (!dev) {
        return NULL;
    }

    xref_get(&dev->ref_count);
    return dev;
}

/**
 * @brief 释放设备及其关联的内存资源
 * @param ref 引用计数结构体指针
 */
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

/**
 * @brief 减少设备的引用计数，计数归零时释放设备
 * @param dev 设备结构体指针
 */
void vmm_device_driver_dref_device(vmm_device_t *dev)
{
    if (dev) {
        xref_put(&dev->ref_count, __devdrv_device_free);
    }
}

/**
 * @brief 检查设备是否已注册到设备驱动框架
 * @param dev 设备结构体指针
 * @return 已注册返回TRUE，否则返回FALSE
 */
bool vmm_device_driver_isregistered_device(vmm_device_t *dev)
{
    return (dev) ? dev->is_registered : FALSE;
}

/**
 * @brief 检查设备是否已绑定驱动
 * @param dev 设备结构体指针
 * @return 已绑定返回TRUE，否则返回FALSE
 */
bool vmm_device_driver_isattached_device(vmm_device_t *dev)
{
    return (dev) ? ((dev->driver) ? TRUE : FALSE) : FALSE;
}

/**
 * @brief 遍历设备的所有子设备，对每个子设备执行回调
 * @param dev 设备结构体指针
 * @param data 用户自定义数据指针
 * @param (*fn 指针参数
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_for_each_child(vmm_device_t *dev, void *data, int (*fn)(vmm_device_t *dev, void *data))
{
    int           err   = 0;
    vmm_device_t *child = NULL;
    vmm_device_t *temp  = NULL;

    if (!dev) {
        return VMM_ERR_FAIL;
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

/**
 * @brief 将设备注册到设备驱动框架
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_register_device(vmm_device_t *dev)
{
    if (dev && dev->bus && !dev->class) {
        return devdrv_bus_register_device(dev->bus, dev);

    } else if (dev && !dev->bus && dev->class) {
        return devdrv_class_register_device(dev->class, dev);
    }

    return VMM_ERR_FAIL;
}

/**
 * @brief 将设备附加到匹配的驱动（绑定设备与驱动）
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_attach_device(vmm_device_t *dev)
{
    vmm_bus_t *bus;

    /* Device should be registered with a valid bus */
    if (!dev || !dev->is_registered || !dev->bus) {
        return VMM_ERR_FAIL;
    }

    bus = dev->bus;

    vmm_mutex_lock(&bus->lock);

    /* Bus probe this device */
    __bus_probe_this_device(bus, dev);

    vmm_mutex_unlock(&bus->lock);

    return VMM_OK;
}

/**
 * @brief 将设备从驱动上分离（解绑设备与驱动）
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_dettach_device(vmm_device_t *dev)
{
    vmm_bus_t *bus;

    /* Device should be registered with a valid bus */
    if (!dev || !dev->is_registered || !dev->bus) {
        return VMM_ERR_FAIL;
    }

    bus = dev->bus;

    vmm_mutex_lock(&bus->lock);

    /* Bus remove this device */
    __bus_remove_this_device(bus, dev);

    vmm_mutex_unlock(&bus->lock);

    return VMM_OK;
}

/**
 * @brief 从设备驱动框架注销设备
 * @param dev 设备结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_unregister_device(vmm_device_t *dev)
{
    if (dev && dev->bus && !dev->class) {
        return devdrv_bus_unregister_device(dev->bus, dev);

    } else if (dev && !dev->bus && dev->class) {
        return devdrv_class_unregister_device(dev->class, dev);
    }

    return VMM_ERR_FAIL;
}

/**
 * @brief 将驱动注册到设备驱动框架
 * @param drv 设备驱动结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_register_driver(vmm_driver_t *drv)
{
    if (!drv) {
        return VMM_ERR_FAIL;
    }

    if (!drv->bus) {
        drv->bus = &platform_bus;
    }

    return vmm_device_driver_bus_register_driver(drv->bus, drv);
}

/**
 * @brief 将驱动附加到匹配的设备（绑定驱动与设备）
 * @param drv 设备驱动结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_attach_driver(vmm_driver_t *drv)
{
    vmm_bus_t *bus;

    if (!drv || !drv->bus) {
        return VMM_ERR_FAIL;
    }

    bus = drv->bus;

    vmm_mutex_lock(&bus->lock);

    /* Bus probe this driver */
    __bus_probe_this_driver(bus, drv);

    vmm_mutex_unlock(&bus->lock);

    return VMM_OK;
}

/**
 * @brief 将驱动从设备上分离（解绑驱动与设备）
 * @param drv 设备驱动结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_dettach_driver(vmm_driver_t *drv)
{
    vmm_bus_t *bus;

    if (!drv || !drv->bus) {
        return VMM_ERR_FAIL;
    }

    bus = drv->bus;

    vmm_mutex_lock(&bus->lock);

    /* Bus remove this driver */
    __bus_remove_this_driver(bus, drv);

    vmm_mutex_unlock(&bus->lock);

    return VMM_OK;
}

/**
 * @brief 从设备驱动框架注销驱动
 * @param drv 设备驱动结构体指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_device_driver_unregister_driver(vmm_driver_t *drv)
{
    if (!drv || !drv->bus) {
        return VMM_ERR_FAIL;
    }

    return vmm_device_driver_bus_unregister_driver(drv->bus, drv);
}

/**
 * @brief 初始化设备驱动
 * @return 成功返回VMM_OK，失败返回错误码
 */
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
