/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file pl061.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief PrimeCell PL061 GPIO Controller Emulator.
 *
 * The source has been largely adapted from QEMU hw/pl061.c
 *
 * Arm PrimeCell PL061 General Purpose IO with additional
 * Luminary Micro Stellaris bits.
 *
 * Copyright (c) 2007 CodeSourcery.
 * Written by Paul Brook
 *
 * The original code is licensed under the GPL.
 */

#include <emu/gpio_sync.h>
#include <vmm_device_emulate.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...) vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define MODULE_DESC      "PL061 GPIO Emulator"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      pl061_emulator_init
#define MODULE_EXIT      pl061_emulator_exit

struct pl061_state {
    vmm_emulate_device_t *edev;
    struct vmm_guest     *guest;
    vmm_spinlock_t        lock;

    uint8_t  id[12];
    uint32_t locked;
    uint32_t data;
    uint32_t old_out_data;
    uint32_t old_in_data;
    uint32_t dir;
    uint32_t isense;
    uint32_t ibe;
    uint32_t iev;
    uint32_t im;
    uint32_t istate;
    uint32_t afsel;
    uint32_t dr2r;
    uint32_t dr4r;
    uint32_t dr8r;
    uint32_t odr;
    uint32_t pur;
    uint32_t pdr;
    uint32_t slr;
    uint32_t den;
    uint32_t cr;
    uint32_t float_high;
    uint32_t amsel;

    uint32_t irq;
    uint32_t in_invert[8];
    uint32_t in_irq[8];
    uint32_t out_irq[8];
};

/* Call update function with lock held */
static void pl061_update(struct pl061_state *s)
{
    int     i;
    uint8_t changed, mask, out;

    DPRINTF("dir = %d, data = %d\n", s->dir, s->data);

    /* Outputs float high.  */
    /* FIXME: This is board dependent.  */
    out     = (s->data & s->dir) | ~s->dir;
    changed = s->old_out_data ^ out;

    if (changed) {
        s->old_out_data = out;

        for (i = 0; i < 8; i++) {
            mask = 1 << i;

            if (changed & mask) {
                DPRINTF("Set output %d = %d\n", i, (out & mask) != 0);
                vmm_device_emulate_emulate_irq(s->guest, s->out_irq[i], (out & mask) != 0);
            }
        }
    }

    /* Inputs */
    changed = (s->old_in_data ^ s->data) & ~s->dir;

    if (changed) {
        s->old_in_data = s->data;

        for (i = 0; i < 8; i++) {
            mask = 1 << i;

            if (changed & mask) {
                DPRINTF("Changed input %d = %d\n", i, (s->data & mask) != 0);

                if (!(s->isense & mask)) {
                    /* Edge interrupt */
                    if (s->ibe & mask) {
                        /* Any edge triggers
                         * the interrupt
                         */
                        s->istate |= mask;
                    } else {
                        /* Edge is selected by IEV */
                        s->istate |= ~(s->data ^ s->iev) & mask;
                    }
                }
            }
        }
    }

    /* Level interrupt */
    s->istate |= ~(s->data ^ s->iev) & s->isense;

    DPRINTF("istate = %02X\n", s->istate);

    vmm_device_emulate_emulate_irq(s->guest, s->irq, (s->istate & s->im) != 0);
}

static int pl061_reg_read(struct pl061_state *s, uint32_t offset, uint32_t *dst)
{
    int                  i, rc = VMM_OK;
    struct gpio_emu_sync sync;

    /* Syncup child GPIO slaves so that input lines are updated */
    for (i = 0; i < 8; i++) {
        sync.irq = s->in_irq[i];
        vmm_device_emulate_sync_children(s->guest, s->edev, GPIO_EMU_SYNC_VALUE, &sync);
    }

    vmm_spin_lock(&s->lock);

    if (offset >= 0xfd0 && offset < 0x1000) {
        *dst = *((uint32_t *)&s->id[(offset - 0xfd0) >> 2]);
    } else if (offset < 0x400) {
        *dst = s->data & (offset >> 2);
    } else {
        switch (offset & ~0x3) {
            case 0x400: /* Direction */
                *dst = s->dir;
                break;

            case 0x404: /* Interrupt sense */
                *dst = s->isense;
                break;

            case 0x408: /* Interrupt both edges */
                *dst = s->ibe;
                break;

            case 0x40c: /* Interrupt event */
                *dst = s->iev;
                break;

            case 0x410: /* Interrupt mask */
                *dst = s->im;
                break;

            case 0x414: /* Raw interrupt status */
                *dst = s->istate;
                break;

            case 0x418: /* Masked interrupt status */
                *dst = s->istate | s->im;
                break;

            case 0x420: /* Alternate function select */
                *dst = s->afsel;
                break;

            case 0x500: /* 2mA drive */
                *dst = s->dr2r;
                break;

            case 0x504: /* 4mA drive */
                *dst = s->dr4r;
                break;

            case 0x508: /* 8mA drive */
                *dst = s->dr8r;
                break;

            case 0x50c: /* Open drain */
                *dst = s->odr;
                break;

            case 0x510: /* Pull-up */
                *dst = s->pur;
                break;

            case 0x514: /* Pull-down */
                *dst = s->pdr;
                break;

            case 0x518: /* Slew rate control */
                *dst = s->slr;
                break;

            case 0x51c: /* Digital enable */
                *dst = s->den;
                break;

            case 0x520: /* Lock */
                *dst = s->locked;
                break;

            case 0x524: /* Commit */
                *dst = s->cr;
                break;

            case 0x528: /* Analog mode select */
                *dst = s->amsel;
                break;

            default:
                rc = VMM_ERR_FAIL;
                break;
        };
    }

    vmm_spin_unlock(&s->lock);

    return rc;
}

static int pl061_reg_write(struct pl061_state *s, uint32_t offset, uint32_t regmask, uint32_t regval)
{
    uint8_t              mask;
    uint32_t             dir;
    int                  i, rc = VMM_OK;
    struct gpio_emu_sync sync;

    vmm_spin_lock(&s->lock);

    if (offset < 0x400) {
        mask    = (offset >> 2) & s->dir;
        s->data = (s->data & ~mask) | (regval & mask);
    } else {
        switch (offset & ~0x3) {
            case 0x400: /* Direction */
                s->dir &= regmask;
                s->dir |= (regval & 0xFF);
                dir = s->dir;
                vmm_spin_unlock(&s->lock);

                for (i = 0; i < 8; i++) {
                    if (dir & (1 << i)) {
                        sync.irq = s->out_irq[i];
                        vmm_device_emulate_sync_children(s->guest, s->edev, GPIO_EMU_SYNC_DIRECTION_OUT, &sync);
                    } else {
                        sync.irq = s->in_irq[i];
                        vmm_device_emulate_sync_children(s->guest, s->edev, GPIO_EMU_SYNC_DIRECTION_IN, &sync);
                    }
                }

                vmm_spin_lock(&s->lock);
                break;

            case 0x404: /* Interrupt sense */
                s->isense &= regmask;
                s->isense |= (regval & 0xFF);
                break;

            case 0x408: /* Interrupt both edges */
                s->ibe &= regmask;
                s->ibe |= (regval & 0xFF);
                break;

            case 0x40c: /* Interrupt event */
                s->iev &= regmask;
                s->iev |= (regval & 0xFF);
                break;

            case 0x410: /* Interrupt mask */
                s->im &= regmask;
                s->im |= (regval & 0xFF);
                break;

            case 0x41c: /* Interrupt clear */
                s->istate &= ~regval;
                break;

            case 0x420: /* Alternate function select */
                mask     = s->cr;
                s->afsel = (s->afsel & ~mask) | (regval & mask);
                break;

            case 0x500: /* 2mA drive */
                s->dr2r &= regmask;
                s->dr2r |= (regval & 0xFF);
                break;

            case 0x504: /* 4mA drive */
                s->dr4r &= regmask;
                s->dr4r |= (regval & 0xFF);
                break;

            case 0x508: /* 8mA drive */
                s->dr8r &= regmask;
                s->dr8r |= (regval & 0xFF);
                break;

            case 0x50c: /* Open drain */
                s->odr &= regmask;
                s->odr |= (regval & 0xFF);
                break;

            case 0x510: /* Pull-up */
                s->pur &= regmask;
                s->pur |= (regval & 0xFF);
                break;

            case 0x514: /* Pull-down */
                s->pdr &= regmask;
                s->pdr |= (regval & 0xFF);
                break;

            case 0x518: /* Slew rate control */
                s->slr &= regmask;
                s->slr |= (regval & 0xFF);
                break;

            case 0x51c: /* Digital enable */
                s->den &= regmask;
                s->den |= (regval & 0xFF);
                break;

            case 0x520: /* Lock */
                s->locked = (regval != 0xacce551);
                break;

            case 0x524: /* Commit */
                if (!s->locked) {
                    s->cr &= regmask;
                    s->cr |= (regval & 0xFF);
                }

                break;

            case 0x528:
                s->amsel &= regmask;
                s->amsel |= (regval & 0xFF);
                break;

            default:
                rc = VMM_ERR_FAIL;
                break;
        }
    }

    if (rc == VMM_OK) {
        pl061_update(s);
    }

    vmm_spin_unlock(&s->lock);

    return rc;
}

static int pl061_emulator_read8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = pl061_reg_read(edev->private, offset, &regval);

    if (!rc) {
        *dst = regval & 0xFF;
    }

    return rc;
}

static int pl061_emulator_read16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = pl061_reg_read(edev->private, offset, &regval);

    if (!rc) {
        *dst = regval & 0xFFFF;
    }

    return rc;
}

static int pl061_emulator_read32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst)
{
    return pl061_reg_read(edev->private, offset, dst);
}

static int pl061_emulator_write8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t src)
{
    return pl061_reg_write(edev->private, offset, 0xFFFFFF00, src);
}

static int pl061_emulator_write16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t src)
{
    return pl061_reg_write(edev->private, offset, 0xFFFF0000, src);
}

static int pl061_emulator_write32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src)
{
    return pl061_reg_write(edev->private, offset, 0x00000000, src);
}

static int pl061_emulator_reset(vmm_emulate_device_t *edev)
{
    struct pl061_state *s = edev->private;

    vmm_spin_lock(&s->lock);

    s->data         = 0;
    s->old_out_data = 0;
    s->old_in_data  = 0;
    s->dir          = 0;
    s->isense       = 0;
    s->ibe          = 0;
    s->iev          = 0;
    s->im           = 0;
    s->istate       = 0;
    s->afsel        = 0;
    s->dr2r         = 0xff;
    s->dr4r         = 0;
    s->dr8r         = 0;
    s->odr          = 0;
    s->pur          = 0;
    s->pdr          = 0;
    s->slr          = 0;
    s->den          = 0;
    s->locked       = 1;
    s->cr           = 0xff;
    s->amsel        = 0;

    vmm_spin_unlock(&s->lock);

    return VMM_OK;
}

/* Process IRQ asserted in device emulation framework */
static void pl061_irq_handle(uint32_t irq, int cpu, int level, void *opaque)
{
    uint8_t             mask;
    int                 i, line;
    struct pl061_state *s = opaque;

    line                  = -1;

    for (i = 0; i < 8; i++) {
        if (s->in_irq[i] == irq) {
            line = i;
            break;
        }
    }

    if (line == -1) {
        return;
    }

    if (s->in_invert[line]) {
        level = (level) ? 0 : 1;
    }

    mask = 1 << line;

    vmm_spin_lock(&s->lock);

    if ((s->dir & mask) == 0) {
        s->data &= ~mask;

        if (level) {
            s->data |= mask;
        }

        pl061_update(s);
    }

    vmm_spin_unlock(&s->lock);
}

static struct vmm_device_emulation_irqchip pl061_irqchip = {
    .name   = "PL061",
    .handle = pl061_irq_handle,
};

static int pl061_emulator_probe(struct vmm_guest *guest, vmm_emulate_device_t *edev, const struct vmm_device_tree_nodeid *eid)
{
    int                 rc = VMM_OK;
    struct pl061_state *s;

    s = vmm_zalloc(sizeof(struct pl061_state));

    if (!s) {
        rc = VMM_ERR_FAIL;
        goto pl061_emulator_probe_done;
    }

    if (eid->data) {
        s->id[0]  = ((const uint8_t *)eid->data)[0];
        s->id[1]  = ((const uint8_t *)eid->data)[1];
        s->id[2]  = ((const uint8_t *)eid->data)[2];
        s->id[3]  = ((const uint8_t *)eid->data)[3];
        s->id[4]  = ((const uint8_t *)eid->data)[4];
        s->id[5]  = ((const uint8_t *)eid->data)[5];
        s->id[6]  = ((const uint8_t *)eid->data)[6];
        s->id[7]  = ((const uint8_t *)eid->data)[7];
        s->id[8]  = ((const uint8_t *)eid->data)[8];
        s->id[9]  = ((const uint8_t *)eid->data)[9];
        s->id[10] = ((const uint8_t *)eid->data)[10];
        s->id[11] = ((const uint8_t *)eid->data)[11];
    }

    rc = vmm_device_tree_read_u32_atindex(edev->node, VMM_DEVICE_TREE_INTERRUPTS_ATTR_NAME, &s->irq, 0);

    if (rc) {
        goto pl061_emulator_probe_freestate_failed;
    }

    rc = vmm_device_tree_read_u32_array(edev->node, "gpio_in_invert", s->in_invert, array_size(s->in_invert));

    if (rc) {
        goto pl061_emulator_probe_freestate_failed;
    }

    rc = vmm_device_tree_read_u32_array(edev->node, "gpio_in_irq", s->in_irq, array_size(s->in_irq));

    if (rc) {
        goto pl061_emulator_probe_freestate_failed;
    }

    rc = vmm_device_tree_read_u32_array(edev->node, "gpio_out_irq", s->out_irq, array_size(s->out_irq));

    if (rc) {
        goto pl061_emulator_probe_freestate_failed;
    }

    s->edev  = edev;
    s->guest = guest;
    INIT_SPIN_LOCK(&s->lock);

    vmm_device_emulate_register_irqchip(guest, s->in_irq[0], &pl061_irqchip, s);
    vmm_device_emulate_register_irqchip(guest, s->in_irq[1], &pl061_irqchip, s);
    vmm_device_emulate_register_irqchip(guest, s->in_irq[2], &pl061_irqchip, s);
    vmm_device_emulate_register_irqchip(guest, s->in_irq[3], &pl061_irqchip, s);
    vmm_device_emulate_register_irqchip(guest, s->in_irq[4], &pl061_irqchip, s);
    vmm_device_emulate_register_irqchip(guest, s->in_irq[5], &pl061_irqchip, s);
    vmm_device_emulate_register_irqchip(guest, s->in_irq[6], &pl061_irqchip, s);
    vmm_device_emulate_register_irqchip(guest, s->in_irq[7], &pl061_irqchip, s);

    edev->private = s;

    goto pl061_emulator_probe_done;

pl061_emulator_probe_freestate_failed:
    vmm_free(s);
pl061_emulator_probe_done:
    return rc;
}

static int pl061_emulator_remove(vmm_emulate_device_t *edev)
{
    struct pl061_state *s = edev->private;

    if (!s) {
        return VMM_ERR_FAIL;
    }

    vmm_device_emulate_unregister_irqchip(s->guest, s->in_irq[0], &pl061_irqchip, s);
    vmm_device_emulate_unregister_irqchip(s->guest, s->in_irq[1], &pl061_irqchip, s);
    vmm_device_emulate_unregister_irqchip(s->guest, s->in_irq[2], &pl061_irqchip, s);
    vmm_device_emulate_unregister_irqchip(s->guest, s->in_irq[3], &pl061_irqchip, s);
    vmm_device_emulate_unregister_irqchip(s->guest, s->in_irq[4], &pl061_irqchip, s);
    vmm_device_emulate_unregister_irqchip(s->guest, s->in_irq[5], &pl061_irqchip, s);
    vmm_device_emulate_unregister_irqchip(s->guest, s->in_irq[6], &pl061_irqchip, s);
    vmm_device_emulate_unregister_irqchip(s->guest, s->in_irq[7], &pl061_irqchip, s);
    vmm_free(s);
    edev->private = NULL;

    return VMM_OK;
}

static uint8_t pl061_id[12]                              = {0x00, 0x00, 0x00, 0x00, 0x61, 0x10, 0x04, 0x00, 0x0d, 0xf0, 0x05, 0xb1};

static struct vmm_device_tree_nodeid pl061_emuid_table[] = {
    {
     .type       = "gpio",
     .compatible = "primecell,pl061",
     .data       = (void *)&pl061_id,
     },
    {/* end of list */},
};

static vmm_emulator_t pl061_emulator = {
    .name        = "pl061",
    .match_table = pl061_emuid_table,
    .endian      = VMM_DEVICE_EMULATE_LITTLE_ENDIAN,
    .probe       = pl061_emulator_probe,
    .read8       = pl061_emulator_read8,
    .write8      = pl061_emulator_write8,
    .read16      = pl061_emulator_read16,
    .write16     = pl061_emulator_write16,
    .read32      = pl061_emulator_read32,
    .write32     = pl061_emulator_write32,
    .reset       = pl061_emulator_reset,
    .remove      = pl061_emulator_remove,
};

static int __init pl061_emulator_init(void)
{
    return vmm_device_emulate_register_emulator(&pl061_emulator);
}

static void __exit pl061_emulator_exit(void)
{
    vmm_device_emulate_unregister_emulator(&pl061_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
