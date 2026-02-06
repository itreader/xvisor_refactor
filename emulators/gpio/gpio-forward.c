/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file gpio-forward.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief GPIO Forward Slave Emulator.
 */

#include <emu/gpio_sync.h>
#include <vmm_device_emulate.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...) vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define MODULE_DESC      "GPIO Forward Emulator"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      gpio_forward_emulator_init
#define MODULE_EXIT      gpio_forward_emulator_exit

struct gpio_forward_state {
    struct vmm_guest *guest;

    uint32_t           in_count;
    uint32_t          *in_irq;
    struct gpio_desc **out_gpio;

    uint32_t           out_count;
    uint32_t          *out_irq;
    struct gpio_desc **in_gpio;
    bool              *in_gpio_bidir;
};

static int gpio_forward_emulator_reset(vmm_emulate_device_t *edev)
{
    int                        i;
    struct gpio_forward_state *s = edev->private;

    for (i = 0; i < s->in_count; i++) {
        gpiod_direction_output(s->out_gpio[i], 0);
    }

    for (i = 0; i < s->out_count; i++) {
        if (!s->in_gpio_bidir[i]) {
            gpiod_direction_input(s->in_gpio[i]);
        }
    }

    return VMM_OK;
}

static void gpio_forward_do_direction_input(struct vmm_guest *g, void *p)
{
    struct gpio_desc *gpio = p;

    if (gpiod_get_direction(gpio) != GPIOF_DIR_IN) {
        DPRINTF("%s: make gpio=%d as input\n", __func__, desc_to_gpio(gpio));
        gpiod_direction_input(gpio);
    }
}

static void gpio_forward_do_direction_output(struct vmm_guest *g, void *p)
{
    struct gpio_desc *gpio = p;

    if (gpiod_get_direction(gpio) != GPIOF_DIR_OUT) {
        DPRINTF("%s: make gpio=%d as output\n", __func__, desc_to_gpio(gpio));
        gpiod_direction_output(gpio, 0);
    }
}

static int gpio_forward_emulator_sync(vmm_emulate_device_t *edev, uint64_t val, void *v)
{
    bool                       bidir;
    struct gpio_desc          *gpio;
    int                        i, j, level, rc = VMM_OK;
    struct gpio_forward_state *s    = edev->private;
    struct gpio_emu_sync      *sync = v;

    if (!sync) {
        return VMM_EINVALID;
    }

    DPRINTF("%s: type=%d irq=%d\n", __func__, (int)val, (int)sync->irq);

    switch (val) {
        case GPIO_EMU_SYNC_DIRECTION_IN:
            gpio  = NULL;
            bidir = FALSE;

            for (i = 0; i < s->out_count; i++) {
                if (s->out_irq[i] == sync->irq) {
                    gpio  = s->in_gpio[i];
                    bidir = s->in_gpio_bidir[i];
                    break;
                }
            }

            if (gpio && bidir) {
                DPRINTF("%s: bi-direction gpio=%d\n", __func__, desc_to_gpio(gpio));
                vmm_manager_guest_operation_request(s->guest, gpio_forward_do_direction_input, gpio);
            }

            break;

        case GPIO_EMU_SYNC_DIRECTION_OUT:
            gpio  = NULL;
            bidir = FALSE;

            for (i = 0; i < s->in_count; i++) {
                if (s->in_irq[i] == sync->irq) {
                    gpio = s->out_gpio[i];

                    for (j = 0; j < s->out_count; j++) {
                        if (s->in_gpio[j] == gpio) {
                            bidir = s->in_gpio_bidir[j];
                            break;
                        }
                    }

                    break;
                }
            }

            if (gpio && bidir) {
                DPRINTF("%s: bi-direction gpio=%d\n", __func__, desc_to_gpio(gpio));
                vmm_manager_guest_operation_request(s->guest, gpio_forward_do_direction_output, gpio);
            }

            break;

        case GPIO_EMU_SYNC_VALUE:
            gpio = NULL;

            for (i = 0; i < s->out_count; i++) {
                if (s->out_irq[i] == sync->irq) {
                    gpio = s->in_gpio[i];
                    break;
                }
            }

            if (gpio) {
                level = __gpio_get_value(desc_to_gpio(gpio));
                vmm_device_emulate_emulate_irq(s->guest, sync->irq, level);
            }

            break;

        default:
            rc = VMM_EINVALID;
            break;
    };

    return rc;
}

/* Process IRQ asserted in device emulation framework */
static void gpio_forward_irq_handle(uint32_t irq, int cpu, int level, void *opaque)
{
    int                        i, line;
    struct gpio_forward_state *s = opaque;

    DPRINTF("%s: irq=%d cpu=%d level=%d\n", __func__, irq, cpu, level);

    line = -1;

    for (i = 0; i < s->in_count; i++) {
        if (s->in_irq[i] == irq) {
            line = i;
            break;
        }
    }

    if (line == -1) {
        return;
    }

    __gpio_set_value(desc_to_gpio(s->out_gpio[line]), (level) ? 1 : 0);
}

static struct vmm_device_emulation_irqchip gpio_forward_irqchip = {
    .name   = "GPIO_FORWARD",
    .handle = gpio_forward_irq_handle,
};

static int gpio_forward_emulator_probe(struct vmm_guest *guest, vmm_emulate_device_t *edev, const struct vmm_device_tree_nodeid *eid)
{
    int                        rc = VMM_OK;
    uint32_t                   i, j, out_gpio, in_gpio;
    struct gpio_forward_state *s;

    s = vmm_zalloc(sizeof(struct gpio_forward_state));

    if (!s) {
        rc = VMM_ENOMEM;
        goto gpio_forward_emulator_probe_done;
    }

    s->guest     = guest;

    s->in_count  = vmm_device_tree_attrlen(edev->node, "in_irq");
    s->in_count  = s->in_count / sizeof(uint32_t);

    s->out_count = vmm_device_tree_attrlen(edev->node, "out_irq");
    s->out_count = s->out_count / sizeof(uint32_t);

    if (s->in_count) {
        s->in_irq = vmm_zalloc(s->in_count * sizeof(*s->in_irq));

        if (!s->in_irq) {
            rc = VMM_ENOMEM;
            goto gpio_forward_emulator_probe_freestate;
        }

        s->out_gpio = vmm_zalloc(s->in_count * sizeof(*s->out_gpio));

        if (!s->out_gpio) {
            rc = VMM_ENOMEM;
            goto gpio_forward_emulator_probe_freeinirq;
        }

        rc = vmm_device_tree_read_u32_array(edev->node, "in_irq", s->in_irq, s->in_count);

        if (rc) {
            goto gpio_forward_emulator_probe_freeoutgpio;
        }

        for (i = 0; i < s->in_count; i++) {
            rc = vmm_device_tree_read_u32_atindex(edev->node, "out_gpio", &out_gpio, i);

            if (rc) {
                goto gpio_forward_emulator_probe_reloutgpio;
            }

            rc = gpio_request(out_gpio, guest->name);

            if (rc) {
                goto gpio_forward_emulator_probe_reloutgpio;
            }

            s->out_gpio[i] = gpio_to_desc(out_gpio);
        }
    }

    if (s->out_count) {
        s->out_irq = vmm_zalloc(s->out_count * sizeof(*s->out_irq));

        if (!s->out_irq) {
            rc = VMM_ENOMEM;
            goto gpio_forward_emulator_probe_freeoutgpio;
        }

        s->in_gpio = vmm_zalloc(s->out_count * sizeof(*s->in_gpio));

        if (!s->in_gpio) {
            rc = VMM_ENOMEM;
            goto gpio_forward_emulator_probe_freeoutirq;
        }

        s->in_gpio_bidir = vmm_zalloc(s->out_count * sizeof(*s->in_gpio_bidir));

        if (!s->in_gpio_bidir) {
            rc = VMM_ENOMEM;
            goto gpio_forward_emulator_probe_freeingpio;
        }

        rc = vmm_device_tree_read_u32_array(edev->node, "out_irq", s->out_irq, s->out_count);

        if (rc) {
            goto gpio_forward_emulator_probe_freeingpiob;
        }

        for (i = 0; i < s->out_count; i++) {
            rc = vmm_device_tree_read_u32_atindex(edev->node, "in_gpio", &in_gpio, i);

            if (rc) {
                goto gpio_forward_emulator_probe_relingpio;
            }

            s->in_gpio[i]       = NULL;
            s->in_gpio_bidir[i] = FALSE;

            for (j = 0; j < s->in_count; j++) {
                if (in_gpio == desc_to_gpio(s->out_gpio[j])) {
                    s->in_gpio[i]       = s->out_gpio[j];
                    s->in_gpio_bidir[i] = TRUE;
                    break;
                }
            }

            if (s->in_gpio[i]) {
                continue;
            }

            rc = gpio_request(in_gpio, guest->name);

            if (rc) {
                goto gpio_forward_emulator_probe_relingpio;
            }

            s->in_gpio[i] = gpio_to_desc(in_gpio);
        }
    }

    for (i = 0; i < s->in_count; i++) {
        vmm_device_emulate_register_irqchip(guest, s->in_irq[i], &gpio_forward_irqchip, s);
    }

    edev->private = s;

    goto gpio_forward_emulator_probe_done;

gpio_forward_emulator_probe_relingpio:

    for (i = 0; i < s->out_count; i++) {
        if (s->in_gpio[i] && !s->in_gpio_bidir[i]) {
            gpio_free(desc_to_gpio(s->in_gpio[i]));
            s->in_gpio[i]       = NULL;
            s->in_gpio_bidir[i] = FALSE;
        }
    }

gpio_forward_emulator_probe_freeingpiob:

    if (s->in_gpio_bidir) {
        vmm_free(s->in_gpio_bidir);
    }

gpio_forward_emulator_probe_freeingpio:

    if (s->in_gpio) {
        vmm_free(s->in_gpio);
    }

gpio_forward_emulator_probe_freeoutirq:

    if (s->out_irq) {
        vmm_free(s->out_irq);
    }

gpio_forward_emulator_probe_reloutgpio:

    for (i = 0; i < s->in_count; i++) {
        if (s->out_gpio[i]) {
            gpio_free(desc_to_gpio(s->out_gpio[i]));
            s->out_gpio[i] = NULL;
        }
    }

gpio_forward_emulator_probe_freeoutgpio:

    if (s->out_gpio) {
        vmm_free(s->out_gpio);
    }

gpio_forward_emulator_probe_freeinirq:

    if (s->in_irq) {
        vmm_free(s->in_irq);
    }

gpio_forward_emulator_probe_freestate:
    vmm_free(s);
gpio_forward_emulator_probe_done:
    return rc;
}

static int gpio_forward_emulator_remove(vmm_emulate_device_t *edev)
{
    uint32_t                   i;
    struct gpio_forward_state *s = edev->private;

    if (!s) {
        return VMM_EFAIL;
    }

    for (i = 0; i < s->in_count; i++) {
        vmm_device_emulate_unregister_irqchip(s->guest, s->in_irq[i], &gpio_forward_irqchip, s);
    }

    for (i = 0; i < s->out_count; i++) {
        if (s->in_gpio[i] && !s->in_gpio_bidir[i]) {
            gpio_free(desc_to_gpio(s->in_gpio[i]));
            s->in_gpio[i]       = NULL;
            s->in_gpio_bidir[i] = FALSE;
        }
    }

    if (s->in_gpio_bidir) {
        vmm_free(s->in_gpio_bidir);
    }

    if (s->in_gpio) {
        vmm_free(s->in_gpio);
    }

    if (s->out_irq) {
        vmm_free(s->out_irq);
    }

    for (i = 0; i < s->in_count; i++) {
        if (s->out_gpio[i]) {
            gpio_free(desc_to_gpio(s->out_gpio[i]));
            s->out_gpio[i] = NULL;
        }
    }

    if (s->out_gpio) {
        vmm_free(s->out_gpio);
    }

    if (s->in_irq) {
        vmm_free(s->in_irq);
    }

    vmm_free(s);
    edev->private = NULL;

    return VMM_OK;
}

static struct vmm_device_tree_nodeid gpio_forward_emuid_table[] = {
    {
     .type       = "gpio-slave",
     .compatible = "gpio-forward",
     },
    {/* end of list */                         },
};

static vmm_emulator_t gpio_forward_emulator = {
    .name        = "gpio-forward",
    .match_table = gpio_forward_emuid_table,
    .endian      = VMM_DEVICE_EMULATE_NATIVE_ENDIAN,
    .probe       = gpio_forward_emulator_probe,
    .reset       = gpio_forward_emulator_reset,
    .sync        = gpio_forward_emulator_sync,
    .remove      = gpio_forward_emulator_remove,
};

static int __init gpio_forward_emulator_init(void)
{
    return vmm_device_emulate_register_emulator(&gpio_forward_emulator);
}

static void __exit gpio_forward_emulator_exit(void)
{
    vmm_device_emulate_unregister_emulator(&gpio_forward_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
