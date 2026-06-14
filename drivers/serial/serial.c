/**
 * Copyright (c) 2015 Anup Patel.
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
 * @file serial.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Serial port framework impelementation.
 */

#include <drv/serial.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>

#define MODULE_DESC      "Serial Port Framework"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (SERIAL_IPRIORITY)
#define MODULE_INIT      serial_init
#define MODULE_EXIT      serial_exit

static LIST_HEAD(serial_list);
static DEFINE_SPINLOCK(serial_list_lock);

static uint32_t serial_read(vmm_char_device_t *cdev, uint8_t *dest, size_t len, off_t __unused *off, bool sleep)
{
    uint32_t       i;
    struct serial *p;

    if (!(cdev && dest && cdev->private)) {
        return 0;
    }

    p = cdev->private;

    if (sleep) {
        for (i = 0; i < len; i++) {
            while (!fifo_dequeue(p->rx_fifo, &dest[i])) {
                vmm_completion_wait(&p->rx_avail);
            }
        }
    } else {
        for (i = 0; i < len; i++) {
            if (!fifo_dequeue(p->rx_fifo, &dest[i])) {
                break;
            }
        }
    }

    return i;
}

static uint32_t serial_write(vmm_char_device_t *cdev, uint8_t *src, size_t len, off_t __unused *off, bool sleep)
{
    uint32_t       ret;
    irq_flags_t    flags;
    struct serial *p;

    if (!(cdev && src && cdev->private)) {
        return 0;
    }

    p = cdev->private;

    vmm_spin_lock_irq_save_lite(&p->tx_lock, flags);
    ret = p->tx_func(p, src, len);
    vmm_spin_unlock_irq_restore_lite(&p->tx_lock, flags);

    return ret;
}

void serial_rx(struct serial *p, uint8_t *data, uint32_t len)
{
    uint32_t i;

    if (!p || !data || !len) {
        return;
    }

    for (i = 0; i < len; i++) {
        fifo_enqueue(p->rx_fifo, &data[i], FALSE);
    }

    vmm_completion_complete(&p->rx_avail);
}

struct serial *serial_create(vmm_device_t *dev, uint32_t rx_fifo_size, uint32_t (*tx_func)(struct serial *, uint8_t *, size_t), void *tx_private)
{
    int            rc = VMM_OK;
    struct serial *p;
    irq_flags_t    flags;

    /* Sanity check */
    if (!dev || !tx_func) {
        rc = VMM_ERR_INVALID;
        goto free_nothing;
    }

    /* Alloc Serial Port */
    p = vmm_zalloc(sizeof(struct serial));

    if (!p) {
        rc = VMM_ERR_NOMEM;
        goto free_nothing;
    }

    INIT_LIST_HEAD(&p->head);

    /* Setup character device */
    if (strlcpy(p->cdev.name, dev->name, sizeof(p->cdev.name)) >= sizeof(p->cdev.name)) {
        rc = VMM_ERR_OVERFLOW;
        goto free_port;
    }

    p->cdev.dev.parent = dev;
    p->cdev.ioctl      = NULL;
    p->cdev.read       = serial_read;
    p->cdev.write      = serial_write;
    p->cdev.private    = p;

    /* Alloc Rx FIFO & Rx Availabilty */
    p->rx_fifo         = fifo_alloc(1, rx_fifo_size);

    if (!p->rx_fifo) {
        rc = VMM_ERR_NOMEM;
        goto free_port;
    }

    INIT_COMPLETION(&p->rx_avail);

    /* Initialize Tx */
    INIT_SPIN_LOCK(&p->tx_lock);
    p->tx_func    = tx_func;
    p->tx_private = tx_private;

    /* Register character device */
    if ((rc = vmm_char_device_register(&p->cdev)) != VMM_OK) {
        goto free_fifo;
    }

    /* Add to list of Serial Ports */
    vmm_spin_lock_irq_save(&serial_list_lock, flags);
    list_add_tail(&p->head, &serial_list);
    vmm_spin_unlock_irq_restore(&serial_list_lock, flags);

    return p;

free_fifo:
    fifo_free(p->rx_fifo);
free_port:
    vmm_free(p);
free_nothing:
    return VMM_ERR_RR_PTR(rc);
}

VMM_ERR_XPORT_SYMBOL(serial_create);

void serial_destroy(struct serial *p)
{
    irq_flags_t flags;

    /* Sanity check */
    if (!p) {
        return;
    }

    /* Remove from list of RBD instances */
    vmm_spin_lock_irq_save(&serial_list_lock, flags);
    list_del(&p->head);
    vmm_spin_unlock_irq_restore(&serial_list_lock, flags);

    /* Unregister character device */
    vmm_char_device_unregister(&p->cdev);

    /* Free Rx FIFO */
    fifo_free(p->rx_fifo);

    /* Free Serial Port */
    vmm_free(p);
}

VMM_ERR_XPORT_SYMBOL(serial_destroy);

struct serial *serial_find(const char *name)
{
    bool           found;
    struct serial *p;
    irq_flags_t    flags;

    if (!name) {
        return NULL;
    }

    found = FALSE;
    p     = NULL;

    vmm_spin_lock_irq_save(&serial_list_lock, flags);

    list_for_each_entry(p, &serial_list, head)
    {
        if (strcmp(p->cdev.name, name) == 0) {
            found = TRUE;
            break;
        }
    }

    vmm_spin_unlock_irq_restore(&serial_list_lock, flags);

    if (!found) {
        return NULL;
    }

    return p;
}

VMM_ERR_XPORT_SYMBOL(serial_find);

uint32_t serial_count(void)
{
    uint32_t       retval = 0;
    struct serial *p;
    irq_flags_t    flags;

    vmm_spin_lock_irq_save(&serial_list_lock, flags);

    list_for_each_entry(p, &serial_list, head)
    {
        retval++;
    }

    vmm_spin_unlock_irq_restore(&serial_list_lock, flags);

    return retval;
}

VMM_ERR_XPORT_SYMBOL(serial_count);

static int __init serial_init(void)
{
    /* For now nothing to do here. */
    return VMM_OK;
}

static void __exit serial_exit(void)
{
    /* For now nothing to do here. */
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
