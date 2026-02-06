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
 * @file vmm_char_device.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Character Device framework header
 */

#ifndef __VMM_CHARDEV_H_
#define __VMM_CHARDEV_H_

#include <vmm_device_driver.h>
#include <vmm_limits.h>
#include <vmm_types.h>

#define VMM_CHARDEV_CLASS_NAME "char"
struct vmm_char_device;
typedef struct vmm_char_device vmm_char_device_t;

struct vmm_char_device {
    char         name[VMM_FIELD_NAME_SIZE];
    vmm_device_t dev;
    int (*ioctl)(vmm_char_device_t *cdev, int cmd, void *arg);
    uint32_t (*read)(vmm_char_device_t *cdev, uint8_t *dest, size_t len, off_t *off, bool sleep);
    uint32_t (*write)(vmm_char_device_t *cdev, uint8_t *src, size_t len, off_t *off, bool sleep);
    void *private;
};

/** Do ioctl operation on a character device */
int vmm_char_device_doioctl(vmm_char_device_t *cdev, int cmd, void *arg);

/** Do read operation on a character device */
uint32_t vmm_char_device_doread(vmm_char_device_t *cdev, uint8_t *dest, size_t len, off_t *off, bool block);

/** Do write operation on a character device */
uint32_t vmm_char_device_dowrite(vmm_char_device_t *cdev, uint8_t *src, size_t len, off_t *off, bool block);

/** Register character device to device driver framework */
int vmm_char_device_register(vmm_char_device_t *cdev);

/** Unregister character device from device driver framework */
int vmm_char_device_unregister(vmm_char_device_t *cdev);

/** Find a character device in device driver framework */
vmm_char_device_t *vmm_char_device_find(const char *name);

/** Iterate over each character device */
int vmm_char_device_iterate(vmm_char_device_t *start, void *data, int (*fn)(vmm_char_device_t *dev, void *data));

/** Count number of character devices */
uint32_t vmm_char_device_count(void);

/** Initalize character device framework */
int vmm_char_device_init(void);

#endif /* __VMM_CHARDEV_H_ */
