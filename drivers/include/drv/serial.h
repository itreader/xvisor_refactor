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
 * @file serial.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Serial Port framework header.
 */

#ifndef __SERIAL_H_
#define __SERIAL_H_

#include <libs/fifo.h>
#include <libs/list.h>
#include <vmm_char_device.h>
#include <vmm_completion.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#define SERIAL_IPRIORITY (1)

/* Serial Port */
struct serial {
    double_list_t     head;
    vmm_char_device_t cdev;

    struct fifo     *rx_fifo;
    vmm_completion_t rx_avail;

    vmm_spinlock_t tx_lock;
    uint32_t (*tx_func)(struct serial *p, uint8_t *src, size_t len);
    void *tx_private;
};

/** Get private context for Serial Port Tx */
static inline void *serial_tx_private(struct serial *p)
{
    return (p) ? p->tx_private : NULL;
}

/** Receive data on Serial Port */
void serial_rx(struct serial *p, uint8_t *data, uint32_t len);

/** Create Serial Port */
struct serial *serial_create(vmm_device_t *dev, uint32_t rx_fifo_size, uint32_t (*tx_func)(struct serial *, uint8_t *, size_t), void *tx_private);

/** Destroy Serial Port */
void serial_destroy(struct serial *p);

/** Find a Serial Port with given name */
struct serial *serial_find(const char *name);

/** Count number of Serial Ports */
uint32_t serial_count(void);

#endif /* __SERIAL_H_ */
