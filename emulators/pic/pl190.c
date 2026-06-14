/**
 * Copyright (c) 2012 Jean-Chrsitophe Dubois.
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
 * @file pl190.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Versatile pl190 (Vector Interrupt Controller) Emulator.
 * @details This source file implements the Versatile pl190 (Vector Interrupt
 * Controller) emulator.
 *
 * The source has been largely adapted from QEMU 0.14.xx hw/pl190.c
 *
 * Arm PrimeCell PL190 Vector Interrupt Controller
 *
 * Copyright (c) 2006 CodeSourcery.
 * Written by Paul Brook
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_device_emulate.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_vcpu_irq.h>

#define MODULE_DESC      "ARM PL190 Emulator"
#define MODULE_AUTHOR    "Jean-Christophe Dubois"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      pl190_emulator_init
#define MODULE_EXIT      pl190_emulator_exit

/* The number of virtual priority levels.  16 user vectors plus the
   unvectored IRQ.  Chained interrupts would require an additional level
   if implemented.  */

#define PL190_NUM_PRIO   17

struct pl190_emulator_state {
    struct vmm_guest *guest;
    vmm_spinlock_t    lock;

    /* Configuration */
    uint8_t  id[8];
    uint32_t num_irq;
    uint32_t base_irq;
    bool     is_child_pic;
    uint32_t parent_irq;

    uint32_t level;
    uint32_t soft_level;
    uint32_t irq_enable;
    uint32_t fiq_select;
    uint8_t  vect_control[16];
    uint32_t vect_addr[PL190_NUM_PRIO];
    /* Mask containing interrupts with higher priority than this one.  */
    uint32_t prio_mask[PL190_NUM_PRIO + 1];
    int protected;
    /* Current priority level.  */
    int priority;
    int prev_prio[PL190_NUM_PRIO];
    int irq;
    int fiq;
};

static inline uint32_t pl190_irq_status(struct pl190_emulator_state *s)
{
    return (s->level | s->soft_level) & s->irq_enable & ~s->fiq_select;
}

/* Update interrupts.  */
static void pl190_update(struct pl190_emulator_state *s)
{
    uint32_t    status;
    vmm_vcpu_t *vcpu = vmm_manager_guest_vcpu(s->guest, 0);

    if (!vcpu) {
        return;
    }

    status = pl190_irq_status(s);

    if (s->is_child_pic) {
        vmm_device_emulate_emulate_irq(s->guest, s->parent_irq, status);
    } else {
        if (status & s->prio_mask[s->priority]) {
            vmm_vcpu_irq_assert(vcpu, s->parent_irq, 0x0);
        } else {
            vmm_vcpu_irq_deassert(vcpu, s->parent_irq);
        }

        if ((s->level | s->soft_level) & s->fiq_select) {
            vmm_vcpu_irq_assert(vcpu, s->parent_irq + 1, 0x0);
        } else {
            vmm_vcpu_irq_deassert(vcpu, s->parent_irq + 1);
        }
    }
}

static void pl190_set_irq(struct pl190_emulator_state *s, int irq, int level)
{
    if (level) {
        s->level |= 1u << irq;
    } else {
        s->level &= ~(1u << irq);
    }

    pl190_update(s);
}

/* Process IRQ asserted via device emulation framework */
static void pl190_irq_handle(uint32_t irq, int cpu, int level, void *opaque)
{
    irq_flags_t                  flags;
    struct pl190_emulator_state *s = opaque;

    irq -= s->base_irq;

    if (level == (s->level & (1u << irq))) {
        return;
    }

    vmm_spin_lock_irq_save(&s->lock, flags);

    pl190_set_irq(s, irq, level);

    vmm_spin_unlock_irq_restore(&s->lock, flags);
}

static void pl190_update_vectors(struct pl190_emulator_state *s)
{
    uint32_t mask;
    int      i;
    int      n;

    mask = 0;

    for (i = 0; i < 16; i++) {
        s->prio_mask[i] = mask;

        if (s->vect_control[i] & 0x20) {
            n = s->vect_control[i] & 0x1f;
            mask |= 1 << n;
        }
    }

    s->prio_mask[16] = mask;

    pl190_update(s);
}

static int pl190_reg_read(struct pl190_emulator_state *s, uint32_t offset, uint32_t *dst)
{
    int i;

    if (!s || !dst) {
        return VMM_ERR_FAIL;
    }

    if (offset >= 0xfe0 && offset < 0x1000) {
        *dst = s->id[(offset - 0xfe0) >> 2];
        return VMM_OK;
    }

    if (offset >= 0x100 && offset < 0x140) {
        *dst = s->vect_addr[(offset - 0x100) >> 2];
        return VMM_OK;
    }

    if (offset >= 0x200 && offset < 0x240) {
        *dst = s->vect_control[(offset - 0x200) >> 2];
        return VMM_OK;
    }

    switch (offset >> 2) {
        case 0: /* IRQSTATUS */
            *dst = pl190_irq_status(s);
            break;

        case 1: /* FIQSATUS */
            *dst = (s->level | s->soft_level) & s->fiq_select;
            break;

        case 2: /* RAWINTR */
            *dst = s->level | s->soft_level;
            break;

        case 3: /* INTSELECT */
            *dst = s->fiq_select;
            break;

        case 4: /* INTENABLE */
            *dst = s->irq_enable;
            break;

        case 6: /* SOFTINT */
            *dst = s->soft_level;
            break;

        case 8: /* PROTECTION */
            *dst = s->protected;
            break;

        case 11:
            /* Read as zero. */
            *dst = 0;
            break;

        case 12: /* VECTADDR */

            /* Read vector address at the start of an ISR.  Increases the
               current priority level to that of the current interrupt.  */
            for (i = 0; i < s->priority; i++) {
                if ((s->level | s->soft_level) & s->prio_mask[i]) {
                    break;
                }
            }

            /* Reading this value with no pending interrupts is undefined.
               We return the default address.  */
            if (i == PL190_NUM_PRIO) {
                *dst = s->vect_addr[16];
            } else {
                if (i < s->priority) {
                    s->prev_prio[i] = s->priority;
                    s->priority     = i;
                    pl190_update(s);
                }

                *dst = s->vect_addr[s->priority];
            }

            break;

        case 13: /* DEFVECTADDR */
            *dst = s->vect_addr[16];
            break;

        default:
            return VMM_ERR_FAIL;
            break;
    }

    return VMM_OK;
}

static int pl190_reg_write(struct pl190_emulator_state *s, uint32_t offset, uint32_t src_mask, uint32_t src)
{
    if (!s) {
        return VMM_ERR_FAIL;
    }

    src = src & ~src_mask;

    if (offset >= 0x100 && offset < 0x140) {
        s->vect_addr[(offset - 0x100) >> 2] = src;
        pl190_update_vectors(s);
        return VMM_OK;
    }

    if (offset >= 0x200 && offset < 0x240) {
        s->vect_control[(offset - 0x200) >> 2] = src;
        pl190_update_vectors(s);
        return VMM_OK;
    }

    switch (offset >> 2) {
        case 0: /* SELECT */
            /* This is a readonly register, but linux tries to write to it
               anyway.  Ignore the write.  */
            break;

        case 3: /* INTSELECT */
            s->fiq_select = src;
            break;

        case 4: /* INTENABLE */
            s->irq_enable |= src;
            break;

        case 5: /* INTENCLEAR */
            s->irq_enable &= ~src;
            break;

        case 6: /* SOFTINT */
            s->soft_level |= src;
            break;

        case 7: /* SOFTINTCLEAR */
            s->soft_level &= ~src;
            break;

        case 8: /* PROTECTION */
            /* TODO: Protection (supervisor only access) is not implemented.  */
            s->protected = src & 1;
            break;

        case 11:
            /* Ignore. */
            break;

        case 12: /* VECTADDR */

            /* Restore the previous priority level.  The value written is
               ignored.  */
            if (s->priority < PL190_NUM_PRIO) {
                s->priority = s->prev_prio[s->priority];
            }

            break;

        case 13: /* DEFVECTADDR */
            s->vect_addr[16] = src;
            break;

        case 0xc0: /* ITCR */
            if (src) {
                /* Test mode not implemented */
                return VMM_ERR_FAIL;
            }

            break;

        default:
            return VMM_ERR_FAIL;
            break;
    }

    pl190_update(s);

    return VMM_OK;
}

static int pl190_emulator_read8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = pl190_reg_read(edev->private, offset, &regval);

    if (!rc) {
        *dst = regval & 0xFF;
    }

    return rc;
}

static int pl190_emulator_read16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = pl190_reg_read(edev->private, offset, &regval);

    if (!rc) {
        *dst = regval & 0xFFFF;
    }

    return rc;
}

static int pl190_emulator_read32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst)
{
    return pl190_reg_read(edev->private, offset, dst);
}

static int pl190_emulator_write8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t src)
{
    return pl190_reg_write(edev->private, offset, 0xFFFFFF00, src);
}

static int pl190_emulator_write16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t src)
{
    return pl190_reg_write(edev->private, offset, 0xFFFF0000, src);
}

static int pl190_emulator_write32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src)
{
    return pl190_reg_write(edev->private, offset, 0x00000000, src);
}

static int pl190_emulator_reset(vmm_emulate_device_t *edev)
{
    int                          i;
    struct pl190_emulator_state *s = edev->private;

    for (i = 0; i < 16; i++) {
        s->vect_addr[i]    = 0;
        s->vect_control[i] = 0;
    }

    s->vect_addr[16] = 0;
    s->prio_mask[17] = 0xffffffff;
    s->priority      = PL190_NUM_PRIO;
    pl190_update_vectors(s);

    return VMM_OK;
}

static struct vmm_device_emulation_irqchip pl190_irqchip = {
    .name   = "PL190",
    .handle = pl190_irq_handle,
};

static int pl190_emulator_probe(struct vmm_guest *guest, vmm_emulate_device_t *edev, const struct vmm_device_tree_nodeid *eid)
{
    uint32_t                     i;
    int                          rc = VMM_OK;
    struct pl190_emulator_state *s;

    s = vmm_zalloc(sizeof(struct pl190_emulator_state));

    if (!s) {
        rc = VMM_ERR_NOMEM;
        goto pl190_emulator_probe_done;
    }

    rc = vmm_device_tree_read_u32(edev->node, "parent_irq", &s->parent_irq);

    if (rc) {
        goto pl190_emulator_probe_freestate_fail;
    }

    if (vmm_device_tree_getattr(edev->node, "child_pic")) {
        s->is_child_pic = TRUE;
    } else {
        s->is_child_pic = FALSE;
    }

    if (vmm_device_tree_read_u32(edev->node, "base_irq", &s->base_irq)) {
        s->base_irq = ((uint32_t *)eid->data)[1];
    }

    if (vmm_device_tree_read_u32(edev->node, "num_irq", &s->num_irq)) {
        s->num_irq = ((uint32_t *)eid->data)[0];
    }

    s->id[0] = ((uint32_t *)eid->data)[2];
    s->id[1] = ((uint32_t *)eid->data)[3];
    s->id[2] = ((uint32_t *)eid->data)[4];
    s->id[3] = ((uint32_t *)eid->data)[5];
    s->id[4] = ((uint32_t *)eid->data)[6];
    s->id[5] = ((uint32_t *)eid->data)[7];
    s->id[6] = ((uint32_t *)eid->data)[8];
    s->id[7] = ((uint32_t *)eid->data)[9];

    s->guest = guest;
    INIT_SPIN_LOCK(&s->lock);

    edev->private = s;

    for (i = s->base_irq; i < (s->base_irq + s->num_irq); i++) {
        vmm_device_emulate_register_irqchip(guest, i, &pl190_irqchip, s);
    }

    goto pl190_emulator_probe_done;

pl190_emulator_probe_freestate_fail:
    vmm_free(s);

pl190_emulator_probe_done:
    return rc;
}

static int pl190_emulator_remove(vmm_emulate_device_t *edev)
{
    uint32_t                     i;
    struct pl190_emulator_state *s = edev->private;

    if (!s) {
        return VMM_ERR_FAIL;
    }

    for (i = s->base_irq; i < (s->base_irq + s->num_irq); i++) {
        vmm_device_emulate_unregister_irqchip(s->guest, i, &pl190_irqchip, s);
    }

    vmm_free(s);
    edev->private = NULL;

    return VMM_OK;
}

static uint32_t pl190_emulator_configs[] = {
    /* === realview === */
    /* num_irq */ 32,
    /* base_irq */ 0,
    /* id0 */ 0x90,
    /* id1 */ 0x11,
    /* id2 */ 0x04,
    /* id3 */ 0x00,
    /* id4 */ 0x0d,
    /* id5 */ 0xf0,
    /* id6 */ 0x05,
    /* id7 */ 0x81,
    /* reserved */ 0,
    /* reserved */ 0,
    /* reserved */ 0,
    /* reserved */ 0,
    /* reserved */ 0,
};

static struct vmm_device_tree_nodeid pl190_emulator_emuid_table[] = {
    {
     .type       = "pic",
     .compatible = "versatilepb,pl190",
     .data       = pl190_emulator_configs,
     },
    {/* end of list */},
};

static vmm_emulator_t pl190_emulator = {
    .name        = "pl190",
    .match_table = pl190_emulator_emuid_table,
    .endian      = VMM_DEVICE_EMULATE_LITTLE_ENDIAN,
    .probe       = pl190_emulator_probe,
    .read8       = pl190_emulator_read8,
    .write8      = pl190_emulator_write8,
    .read16      = pl190_emulator_read16,
    .write16     = pl190_emulator_write16,
    .read32      = pl190_emulator_read32,
    .write32     = pl190_emulator_write32,
    .reset       = pl190_emulator_reset,
    .remove      = pl190_emulator_remove,
};

static int __init pl190_emulator_init(void)
{
    return vmm_device_emulate_register_emulator(&pl190_emulator);
}

static void __exit pl190_emulator_exit(void)
{
    vmm_device_emulate_unregister_emulator(&pl190_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
