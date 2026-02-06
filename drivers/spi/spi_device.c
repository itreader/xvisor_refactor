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
 * @file spi_device.c
 * @author Chaitanya Dhere (chaitanyadhere1@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic SPI_DEV driver source
 *
 * The source has been largely adapted from Linux
 * include/linux/spi/spi_device.h
 *
 * The original code is licensed under the GPL.
 *
 * Copyright (C) 2006 SWAPP
 *  Andrea Paterniani <a.paterniani@swapp-eng.it>
 */

#include <drv/spi/spi_device.h>
#include <libs/list.h>
#include <vmm_completion.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_mutex.h>
#include <vmm_stdio.h>

#include <linux/spi/spi.h>

#define MODULE_DESC      "SPI_DEV driver"
#define MODULE_AUTHOR    "Chaitanya Dhere"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (SPI_DEVICE_IPRIORITY)
#define MODULE_INIT      spi_device_init
#define MODULE_EXIT      spi_device_exit

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

#define SPI_MODE_MASK                                                                                                                             \
    (SPI_CPHA | SPI_CPOL | SPI_CS_HIGH | SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP | SPI_NO_CS | SPI_READY | SPI_TX_DUAL | SPI_TX_QUAD | SPI_RX_DUAL | \
     SPI_RX_QUAD)

int spi_device_count(void)
{
    int                num = 0;
    struct spi_device *spi_device;

    list_for_each_entry(spi_device, &device_list, device_entry) num++;

    return num;
}

VMM_EXPORT_SYMBOL(spi_device_count);

struct spi_device *spi_device_get(int id)
{
    int                num = 0;
    struct spi_device *spi_device, *ptr = NULL;

    list_for_each_entry(spi_device, &device_list, device_entry)
    {
        if (id == num) {
            ptr = spi_device;
            break;
        }

        num++;
    }

    return ptr;
}

VMM_EXPORT_SYMBOL(spi_device_get);

const char *spi_device_name(struct spi_device *spi_device)
{
    if (!spi_device) {
        return NULL;
    }

    return dev_name(&spi_device->spi->dev);
}

VMM_EXPORT_SYMBOL(spi_device_name);

static void spi_device_complete(void *arg)
{
    vmm_completion_complete(arg);
}

static ssize_t spi_device_sync(struct spi_device *spi_device, struct spi_message *msg)
{
    int              status;
    uint64_t         flags;
    vmm_completion_t done;

    INIT_COMPLETION(&done);
    msg->complete = spi_device_complete;
    msg->context  = &done;

    vmm_spin_lock_irq_save(&spi_device->spi_lock, flags);

    if (spi_device->spi == NULL) {
        status = VMM_ENOTAVAIL;
    } else if (spi_device->busy) {
        status = VMM_EBUSY;
    } else {
        spi_device->busy = 1;
        status           = spi_async(spi_device->spi, msg);
    }

    vmm_spin_unlock_irq_restore(&spi_device->spi_lock, flags);

    if (status == 0) {
        vmm_completion_wait(&done);
        vmm_spin_lock_irq_save(&spi_device->spi_lock, flags);
        spi_device->busy = 0;
        vmm_spin_unlock_irq_restore(&spi_device->spi_lock, flags);
        status = msg->status;

        if (status == 0) {
            status = msg->actual_length;
        }
    }

    return status;
}

int spi_device_xfer(struct spi_device *spi_device, struct spi_device_xfer_data *xdata)
{
    int                 mask, ret = 0;
    struct spi_transfer t = {
        .tx_buf = xdata->tx_buf,
        .rx_buf = xdata->rx_buf,
        .len    = xdata->len,
    };
    struct spi_message m;

    if (!spi_device || !xdata) {
        return VMM_EINVALID;
    }

    if (xdata->mode == -1) {
        spi_device->spi->mode          = SPI_MODE_0;
        spi_device->spi->bits_per_word = 8;
        spi_device->spi->max_speed_hz  = 500000;
        mask                           = spi_device->spi->mode & ~SPI_MODE_MASK;
        spi_device->spi->mode          = (uint16_t)mask;
    } else {
        switch (xdata->mode) {
            case 0:
                spi_device->spi->mode = SPI_MODE_0;
                break;

            case 1:
                spi_device->spi->mode = SPI_MODE_1;
                break;

            case 2:
                spi_device->spi->mode = SPI_MODE_2;
                break;

            case 3:
                spi_device->spi->mode = SPI_MODE_3;
                break;
        };

        spi_device->spi->bits_per_word = xdata->bits_per_word;

        spi_device->spi->max_speed_hz  = xdata->out_frequency;

        mask                           = spi_device->spi->mode & ~SPI_MODE_MASK;

        spi_device->spi->mode          = (uint16_t)mask;
    }

    ret = spi_setup(spi_device->spi);

    if (ret < 0) {
        vmm_lerror("SPI_DEV", "Setting up SPI failed\n");
        return VMM_EINVALID;
    }

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);

    ret = spi_device_sync(spi_device, &m);

    if (ret < 0) {
        vmm_lerror("SPI_DEV", "Submitting data to SPI failed\n");

        if (ret == VMM_EBUSY) {
            return ret;
        }

        spi_device->busy = 0; /* This is required since in case of a failure other then VMM_EBUSY, the busy bit is not set to 0 causing erronous
                             conditions in subsequent operations */
        return VMM_EIO;
    }

    return ret;
}

EXPORT_SYMBOL(spi_device_xfer);

static int spi_device_probe(struct spi_device *spi)
{
    struct spi_device *spi_device;

    spi_device = vmm_zalloc(sizeof(*spi_device));

    if (!spi_device) {
        return VMM_ENOMEM;
    }

    spi_device->spi = spi;
    spin_lock_init(&spi_device->spi_lock);
    spi_device->busy = 0;
    INIT_LIST_HEAD(&spi_device->device_entry);

    vmm_mutex_lock(&device_list_lock);
    list_add_tail(&spi_device->device_entry, &device_list);
    vmm_mutex_unlock(&device_list_lock);

    spi_set_drvdata(spi, spi_device);

    return 0;
}

static int spi_device_remove(struct spi_device *spi)
{
    struct spi_device *spi_device = spi_get_drvdata(spi);

    spi_device->spi               = NULL;

    vmm_mutex_lock(&device_list_lock);
    list_del(&spi_device->device_entry);
    vmm_mutex_unlock(&device_list_lock);

    vmm_free(spi_device);

    return 0;
}

static const struct of_device_id spi_device_match[] = {
    {
     .compatible = "spi_device",
     },
    {}
};

static struct spi_driver spi_device_spi_driver = {
    .driver =
        {
                 .match_table = spi_device_match,
                 },
    .probe  = spi_device_probe,
    .remove = spi_device_remove,
};

static int __init spi_device_init(void)
{
    return spi_register_driver(&spi_device_spi_driver);
}

static void __exit spi_device_exit(void)
{
    spi_unregister_driver(&spi_device_spi_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
