/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file vsdaemon_char_device.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief vserial daemon char_device transport implementation
 */

#include <libs/vsdaemon.h>
#include <vmm_char_device.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_macros.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "vsdaemon char_device transport"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (VSDAEMON_IPRIORITY + 1)
#define MODULE_INIT      vsdaemon_char_device_init
#define MODULE_EXIT      vsdaemon_char_device_exit

struct vsdaemon_char_device {
    /* character device pointer */
    vmm_char_device_t *cdev;
};

static void vsdaemon_char_device_receive_char(struct vsdaemon *vsd, uint8_t ch)
{
    struct vsdaemon_char_device *vcdev = vsdaemon_transport_get_data(vsd);

    vmm_cdev_putc(vcdev->cdev, ch);
}

static int vsdaemon_char_device_main_loop(struct vsdaemon *vsd)
{
    char                         ch;
    struct vsdaemon_char_device *vcdev = vsdaemon_transport_get_data(vsd);

    while (1) {
        if (!vmm_scanchars(vcdev->cdev, &ch, 1, TRUE)) {
            while (!vmm_vserial_send(vsd->vser, (uint8_t *)&ch, 1))
                ;
        }
    }

    return VMM_OK;
}

static int vsdaemon_char_device_setup(struct vsdaemon *vsd, int argc, char **argv)
{
    struct vsdaemon_char_device *vcdev;

    if (argc < 1) {
        return VMM_ERR_INVALID;
    }

    vcdev = vmm_zalloc(sizeof(*vcdev));

    if (!vcdev) {
        return VMM_ERR_NOMEM;
    }

    vcdev->cdev = vmm_char_device_find(argv[0]);

    if (!vcdev->cdev) {
        vmm_free(vcdev);
        return VMM_ERR_INVALID;
    }

    vsdaemon_transport_set_data(vsd, vcdev);

    return VMM_OK;
}

static void vsdaemon_char_device_cleanup(struct vsdaemon *vsd)
{
    struct vsdaemon_char_device *vcdev = vsdaemon_transport_get_data(vsd);

    vsdaemon_transport_set_data(vsd, NULL);

    vmm_free(vcdev);
}

static struct vsdaemon_transport char_device = {
    .name         = "char_device",
    .setup        = vsdaemon_char_device_setup,
    .cleanup      = vsdaemon_char_device_cleanup,
    .main_loop    = vsdaemon_char_device_main_loop,
    .receive_char = vsdaemon_char_device_receive_char,
};

static int __init vsdaemon_char_device_init(void)
{
    return vsdaemon_transport_register(&char_device);
}

static void __exit vsdaemon_char_device_exit(void)
{
    vsdaemon_transport_unregister(&char_device);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
