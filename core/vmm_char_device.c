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
 * @file vmm_char_device.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Character Device framework source
 */

#include <libs/stringlib.h>
#include <vmm_char_device.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_scheduler.h>

int vmm_char_device_doioctl(vmm_char_device_t *cdev, int cmd, void *arg)
{
    if (cdev && cdev->ioctl) {
        return cdev->ioctl(cdev, cmd, arg);
    } else {
        return VMM_EFAIL;
    }
}

uint32_t vmm_char_device_doread(vmm_char_device_t *cdev, uint8_t *dest, size_t len, off_t *off, bool block)
{
    uint32_t b;
    bool     sleep;

    if (cdev && cdev->read) {
        if (block) {
            b     = 0;
            sleep = vmm_scheduler_orphan_context() ? TRUE : FALSE;

            while (b < len) {
                b += cdev->read(cdev, &dest[b], len - b, off, sleep);
            }

            return b;
        } else {
            return cdev->read(cdev, dest, len, off, FALSE);
        }
    } else {
        return 0;
    }
}

uint32_t vmm_char_device_dowrite(vmm_char_device_t *cdev, uint8_t *src, size_t len, off_t *off, bool block)
{
    uint32_t b;
    bool     sleep;

    if (cdev && cdev->write) {
        if (block) {
            b     = 0;
            sleep = vmm_scheduler_orphan_context() ? TRUE : FALSE;

            while (b < len) {
                b += cdev->write(cdev, &src[b], len - b, off, sleep);
            }

            return b;
        } else {
            return cdev->write(cdev, src, len, off, FALSE);
        }
    } else {
        return 0;
    }
}

static vmm_class_t char_device_class = {
    .name = VMM_CHARDEV_CLASS_NAME,
};

int vmm_char_device_register(vmm_char_device_t *cdev)
{
    if (!(cdev && cdev->read && cdev->write)) {
        return VMM_EFAIL;
    }

    vmm_device_driver_initialize_device(&cdev->dev);

    if (strlcpy(cdev->dev.name, cdev->name, sizeof(cdev->dev.name)) >= sizeof(cdev->dev.name)) {
        return VMM_EOVERFLOW;
    }

    cdev->dev.class = &char_device_class;
    vmm_device_driver_set_data(&cdev->dev, cdev);

    return vmm_device_driver_register_device(&cdev->dev);
}

int vmm_char_device_unregister(vmm_char_device_t *cdev)
{
    if (!cdev) {
        return VMM_EFAIL;
    }

    return vmm_device_driver_unregister_device(&cdev->dev);
}

vmm_char_device_t *vmm_char_device_find(const char *name)
{
    vmm_device_t *dev;

    dev = vmm_device_driver_class_find_device_by_name(&char_device_class, name);

    if (!dev) {
        return NULL;
    }

    return vmm_device_driver_get_data(dev);
}

struct char_device_iterate_priv {
    void *data;
    int (*fn)(vmm_char_device_t *dev, void *data);
};

static int char_device_iterate(vmm_device_t *dev, void *data)
{
    struct char_device_iterate_priv *p    = data;
    vmm_char_device_t               *cdev = vmm_device_driver_get_data(dev);

    return p->fn(cdev, p->data);
}

int vmm_char_device_iterate(vmm_char_device_t *start, void *data, int (*fn)(vmm_char_device_t *dev, void *data))
{
    vmm_device_t                   *st = (start) ? &start->dev : NULL;
    struct char_device_iterate_priv p;

    if (!fn) {
        return VMM_EINVALID;
    }

    p.data = data;
    p.fn   = fn;

    return vmm_device_driver_class_device_iterate(&char_device_class, st, &p, char_device_iterate);
}

uint32_t vmm_char_device_count(void)
{
    return vmm_device_driver_class_device_count(&char_device_class);
}

int __init vmm_char_device_init(void)
{
    return vmm_device_driver_register_class(&char_device_class);
}
