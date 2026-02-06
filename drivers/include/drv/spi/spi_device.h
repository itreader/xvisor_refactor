/**
 * Copyright (c) 2016 Chaitanya Dhere.
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
 * @file spi_device.h
 * @author Chaitanya Dhere (chaitanyadhere1@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic SPI_DEV driver interface
 *
 * The source has been largely adapted from Linux
 * include/linux/spi/spi_device.h
 *
 * The original code is licensed under the GPL.
 *
 * Copyright (C) 2006 SWAPP
 *  Andrea Paterniani <a.paterniani@swapp-eng.it>
 */

#ifndef __SPI_DEVICE_H__
#define __SPI_DEVICE_H__

#include <vmm_spinlocks.h>
#include <vmm_types.h>

/*
 * SPI_DEV module init priority level
 * Note: Ideally this should be (SPI_IPRIORITY + 1)
 * but to make spi_device.h independent of <linux/spi/spi.h>
 * we directly define
 */
#define SPI_DEVICE_IPRIORITY (2)

struct spi_device;

/** Opaque structure representing SPI_DEV */
struct spi_device {
    struct spi_device *spi;
    vmm_spinlock_t     spi_lock;
    int                busy;
    double_list_t      device_entry;
};

/*
 * SPI_DEVICE_xxx defines which are excatly same as
 * SPI_xxx defines provided in <linux/spi/spi.h>
 * so that users of SPI_DEV don't have to depend
 * on <linux/spi/spi.h>
 */

#define SPI_DEVICE_CPHA      0x01
#define SPI_DEVICE_CPOL      0x02

#define SPI_DEVICE_MODE_0    (0 | 0)
#define SPI_DEVICE_MODE_1    (0 | SPI_DEVICE_CPHA)
#define SPI_DEVICE_MODE_2    (SPI_DEVICE_CPOL | 0)
#define SPI_DEVICE_MODE_3    (SPI_DEVICE_CPOL | SPI_DEVICE_CPHA)

#define SPI_DEVICE_CS_HIGH   0x04
#define SPI_DEVICE_LSB_FIRST 0x08
#define SPI_DEVICE_3WIRE     0x10
#define SPI_DEVICE_LOOP      0x20
#define SPI_DEVICE_NO_CS     0x40
#define SPI_DEVICE_READY     0x80
#define SPI_DEVICE_TX_DUAL   0x100
#define SPI_DEVICE_TX_QUAD   0x200
#define SPI_DEVICE_RX_DUAL   0x400
#define SPI_DEVICE_RX_QUAD   0x800

/** Structure representing xfer on SPI_DEV */
struct spi_device_xfer_data {
    int      mode;
    int      out_frequency;
    int      bits_per_word;
    uint8_t *tx_buf;
    uint8_t *rx_buf;
    size_t   len;
};

/** Get count of available SPI_DEV instances */
int spi_device_count(void);

/** Get SPI_DEV instance */
struct spi_device *spi_device_get(int id);

/** Get SPI_DEV name */
const char *spi_device_name(struct spi_device *spi_device);

/** Do Xfer on SPI_DEV instances */
int spi_device_xfer(struct spi_device *spi_device, struct spi_device_xfer_data *xdata);

#endif /* __SPI_DEVICE_H__ */
