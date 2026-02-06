/**
 * Copyright (c) 2016 Open Wide
 *               2016 Institut de Recherche Technologique SystemX
 *
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
 * @file cmd_gpio.c
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief Implementation of gpio command
 */

#include <libs/stringlib.h>
#include <linux/gpio.h>
#include <vmm_command_manager.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>
#include <vmm_version.h>

#define MODULE_DESC      "Command gpio"
#define MODULE_AUTHOR    "Jean Guyomarc'h"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      cmd_gpio_init
#define MODULE_EXIT      cmd_gpio_exit

static int cmd_gpio_help(vmm_char_device_t *cdev, int argc, char **argv)
{
    vmm_cdev_printf(cdev, "Usage:\n");
    vmm_cdev_printf(cdev, "   gpio help - Displays the help\n");
    vmm_cdev_printf(cdev, "   gpio list - Displays the GPIOs\n");
    vmm_cdev_printf(cdev, "   gpio set ID {in,out} [1/0] - Set direction and value\n");
    return VMM_OK;
}

static void cmd_gpio_usage(vmm_char_device_t *cdev)
{
    cmd_gpio_help(cdev, 0, NULL);
}

static int cmd_gpio_list(vmm_char_device_t *cdev, int argc, char **argv)
{
    if (argc != 1) {
        vmm_cdev_printf(cdev, "*** Invalid use of command\n");
        return VMM_EFAIL;
    }

    gpiolib_dump(cdev);
    return VMM_OK;
}

static int cmd_gpio_set(vmm_char_device_t *cdev, int argc, char **argv)
{
    const char *ptr;

    enum {
        GPIO_INVALID,
        GPIO_IN,
        GPIO_OUT
    } dir = GPIO_INVALID;

    uint32_t gpio;
    uint32_t val;
    int      rc;

    if ((argc < 2) || (argc > 4)) {
        goto fail;
    }

    gpio = atoi(argv[1]);
    ptr  = argv[2];

    if ((strcmp(ptr, "in") == 0)) {
        dir = GPIO_IN;
    } else if (strcmp(ptr, "out") == 0) {
        dir = GPIO_OUT;
    }

    if (argc == 3) {
        if (dir != GPIO_IN) {
            goto fail;
        }

        rc = gpio_direction_input(gpio);

        if (rc) {
            vmm_cdev_printf(cdev, "*** Error: %i\n", rc);
            return VMM_EFAIL;
        }

        rc = __gpio_get_value(gpio);
        vmm_cdev_printf(cdev, "value = %d\n", rc);
    } else if (argc == 4) {
        if (dir != GPIO_OUT) {
            goto fail;
        }

        ptr = argv[3];
        val = atoi(ptr);
        rc  = gpio_direction_output(gpio, val);

        if (rc) {
            vmm_cdev_printf(cdev, "*** Error: %i\n", rc);
            return VMM_EFAIL;
        }
    }

    return VMM_OK;
fail:
    vmm_cdev_printf(cdev, "*** Invalid use of command\n");
    return VMM_EFAIL;
}

static int cmd_gpio_exec(vmm_char_device_t *cdev, int argc, char **argv)
{
    struct {
        const char *name;
        int (*func)(vmm_char_device_t *cdev, int argc, char **argv);
    } const cmds[] = {
        {"help", cmd_gpio_help},
        {"list", cmd_gpio_list},
        {"set",  cmd_gpio_set },
        {NULL,   NULL         }
    };

    uint32_t idx = 0;

    if (argc == 1) {
        goto done;
    }

    while (cmds[idx].name) {
        if (strcmp(argv[1], cmds[idx].name) == 0) {
            return cmds[idx].func(cdev, argc - 1, argv + 1);
        }

        idx++;
    }

done:
    cmd_gpio_usage(cdev);
    return VMM_EFAIL;
}

static vmm_command_t cmd_gpio = {
    .name  = "gpio",
    .desc  = "Interact with GPIOs",
    .usage = cmd_gpio_usage,
    .exec  = cmd_gpio_exec,
};

static int __init cmd_gpio_init(void)
{
    return vmm_command_manager_register_cmd(&cmd_gpio);
}

static void __exit cmd_gpio_exit(void)
{
    vmm_command_manager_unregister_cmd(&cmd_gpio);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
