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
 * @file cmd_spi_device.c
 * @author Chaitanya Dhere (chaitanyadhere1@gmail.com)
 * @brief Implementation of spi_device command
 */

#include <drv/spi/spi_device.h>
#include <vmm_command_manager.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "SPI_DEV command"
#define MODULE_AUTHOR    "Chaitanya Dhere"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_spi_device_init
#define MODULE_EXIT      cmd_spi_device_exit

#define MAX_BUFLEN       256

static void cmd_spi_device_usage(vmm_char_device_t *cdev)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   spi_device list - Display SPI device list \n");
    vmm_cdev_printf(
        cdev, "   spi_device xfer <mode> <output_frequency> "
              "<bits_per_word> <id_num> <data_to_transfer> \n");
    vmm_cdev_printf(
        cdev, "Available modes - 0,1,2,3 \n Read supported "
              "frequencies from SoC datasheet / manual,\n"
              "Mode0 can be used for normal/loopback operations\n"
              "Example command:\n"
              "1. spi_device xfer 0 0x12 (Uses the default mode, "
              "frequency and bits per word)\n"
              "2. spi_device xfer 0 500000 8 "
              "(Uses user defined values)\n"
              "NOTE: Please use user defined options in the same "
              "order and format as mentioned in Example2\n");
}

static int cmd_spi_device_help(vmm_char_device_t *cdev, int __unused argc, char __unused **argv)
{
    cmd_spi_device_usage(cdev);
    return VMM_OK;
}

static int cmd_spi_device_list(vmm_char_device_t *cdev, int __unused argc, char __unused **argv)
{
    int                id = 0, num, i;
    struct spi_device *spi_device;

    if (argc < 1) {
        cmd_spi_device_usage(cdev);
        return VMM_ERR_FAIL;
    }

    id = atoi(argv[1]);

    if (id < 0) {
        cmd_spi_device_usage(cdev);
        return VMM_ERR_FAIL;
    }

    num = spi_device_count();
    vmm_cdev_printf(cdev, "Total %d spi_device instances found : \n", num);

    for (i = 0; i < num; i++) {
        spi_device = spi_device_get(i);
        vmm_cdev_printf(cdev, "\n id = %d and spi_device instance = %s\n", i, spi_device_name(spi_device));
    }

    return VMM_OK;
}

static int cmd_spi_device_xfer(vmm_char_device_t *cdev, int argc, char **argv)
{
    int                         id = 0, ret = 0, index = 0, num = 0;
    struct spi_device_xfer_data xfer;
    struct spi_device          *spi_device;

    xfer.mode = -1;

    if (argc < 4) {
        ret = VMM_ERR_INVALID;
        goto fail;
    } else if (argc > 4 && argc != 7) {
        ret = VMM_ERR_INVALID;
        goto fail;
    }

    if (argc > 4) {
        index     = 5;
        xfer.mode = atoi(argv[2]);

        if (xfer.mode < 0 || xfer.mode > 3) {
            ret = VMM_ERR_INVALID;
            goto fail;
        }

        xfer.out_frequency = atoi(argv[3]);

        if (xfer.out_frequency < 0) {
            ret = VMM_ERR_INVALID;
            goto fail;
        }

        xfer.bits_per_word = atoi(argv[4]);

        if (xfer.bits_per_word < 0) {
            ret = VMM_ERR_INVALID;
            goto fail;
        }
    } else {
        index = 2;
    }

    num = spi_device_count();
    id  = atoi(argv[index]);

    if (id < 0) {
        ret = VMM_ERR_INVALID;
        goto fail;
    } else if (id > num) {
        vmm_cdev_printf(
            cdev, "Please enter a valid ID using: "
                  "spi_device list command\n");
        ret = VMM_ERR_INVALID;
        goto fail;
    }

    spi_device = spi_device_get(id);

    if (!spi_device) {
        vmm_cdev_printf(cdev, "Failed to get spi_device from ID %d\n", id);
        ret = VMM_ERR_INVALID;
        goto fail;
    }

    xfer.tx_buf = vmm_zalloc(MAX_BUFLEN);

    if (xfer.tx_buf == NULL) {
        vmm_cdev_printf(cdev, "Failed to allocate buffer for Tx data \n");
        ret = VMM_ERR_NOMEM;
        goto fail;
    }

    xfer.rx_buf = vmm_zalloc(MAX_BUFLEN);

    if (xfer.rx_buf == NULL) {
        vmm_cdev_printf(cdev, "Failed to allocate buffer for Rx data \n");
        vmm_free(xfer.tx_buf);
        ret = VMM_ERR_NOMEM;
        goto fail;
    }

    /* TODO: We should parse bytes instead of strcpy */
    strcpy((char *)xfer.tx_buf, argv[index + 1]);
    xfer.len = strlen((const char *)xfer.tx_buf) + 1;

    vmm_cdev_printf(cdev, "Submitting: %s to SPI device \n", xfer.tx_buf);
    ret = spi_device_xfer(spi_device, &xfer);

    if (ret < 0) {
        vmm_cdev_printf(cdev, "Failed submit data to the SPI_DEV\n");
        vmm_free(xfer.rx_buf);
        vmm_free(xfer.tx_buf);
        return ret;
    }

    vmm_cdev_printf(cdev, "Received: %s as a reply from device \n", xfer.rx_buf);

    vmm_free(xfer.rx_buf);
    vmm_free(xfer.tx_buf);

    return VMM_OK;

fail:
    cmd_spi_device_usage(cdev);
    return ret;
}

static const struct {
    char *name;
    int (*function)(vmm_char_device_t *, int, char **);
} const command[] = {
    {"help", cmd_spi_device_help},
    {"list", cmd_spi_device_list},
    {"xfer", cmd_spi_device_xfer},
    {NULL,   NULL               },
};

static int cmd_spi_device_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    int index = 0;

    if (argc < 2) {
        goto fail;
    }

    while (command[index].name) {
        if (strcmp(argv[1], command[index].name) == 0) {
            return command[index].function(cdev, argc, argv);
        }

        index++;
    }

fail:
    cmd_spi_device_usage(cdev);
    return VMM_ERR_FAIL;
}

static vmm_command_t cmd_spi_device = {
    .name  = "spi_device",
    .desc  = "control commands for SPI_DEV devices",
    .usage = cmd_spi_device_usage,
    .exec  = cmd_spi_device_exec,
};

static int __init cmd_spi_device_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_spi_device);
}

static void __exit cmd_spi_device_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_spi_device);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
