/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file arm11mpcore.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM11 MPCore Private Memory Emulator.
 * @details This source file implements the private memory region present
 * in ARM11 multiprocessor system
 *
 * The source has been adapted from QEMU hw/arm11mpcore.c
 *
 * ARM11MPCore internal peripheral emulation.
 *
 * Copyright (c) 2006-2007 CodeSourcery.
 * Written by Paul Brook
 *
 * The original code is licensed under the GPL.
 */

#include <emu/arm_mptimer_emulator.h>
#include <emu/gic_emulator.h>
#include <vmm_device_emulate.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>

#define MODULE_DESC      "ARM11MPCore Private Region Emulator"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      arm11mpcore_emulator_init
#define MODULE_EXIT      arm11mpcore_emulator_exit

/* Memory map (addresses are offsets from PERIPHBASE):
 *  0x0000-0x00ff -- Snoop Control Unit
 *  0x0100-0x01ff -- GIC CPU interface
 *  0x0200-0x02ff -- Global Timer
 *  0x0300-0x05ff -- nothing
 *  0x0600-0x06ff -- private timers and watchdogs
 *  0x0700-0x0fff -- nothing
 *  0x1000-0x1fff -- GIC Distributor
 *
 * We currently implement only the SCU and GIC portions.
 */

struct arm11mpcore_priv_state {
    struct vmm_guest *guest;
    vmm_spinlock_t    lock;

    /* Configuration */
    uint32_t num_cpu;

    /* Snoop Control Unit */
    uint32_t scu_control;

    /* Private & Watchdog Timer Block */
    struct mptimer_state *mpt;

    /* GIC-state */
    struct gic_state *gic;
};

static int arm11mpcore_scu_read(struct arm11mpcore_priv_state *s, uint32_t offset, uint32_t *dst)
{
    int rc = VMM_OK;

    if (!s || !dst) {
        return VMM_ERR_FAIL;
    }

    vmm_spin_lock(&s->lock);

    switch (offset) {
        case 0x00: /* Control */
            *dst = s->scu_control;
            break;

        case 0x04: /* Configuration */
            *dst = (((1 << s->num_cpu) - 1) << 4) | (s->num_cpu - 1);
            break;

        case 0x08: /* CPU Status */
            *dst = 0;
            break;

        case 0x0c: /* Invalidate all. */
            *dst = 0;
            break;

        default:
            rc = VMM_ERR_FAIL;
            break;
    }

    vmm_spin_unlock(&s->lock);

    return rc;
}

static int arm11mpcore_scu_write(struct arm11mpcore_priv_state *s, uint32_t offset, uint32_t src_mask, uint32_t src)
{
    int rc = VMM_OK;

    if (!s) {
        return VMM_ERR_FAIL;
    }

    src = src & ~src_mask;

    vmm_spin_lock(&s->lock);

    switch (offset) {
        case 0x00: /* Control */
            s->scu_control = src & 1;
            break;

        case 0x0c: /* Invalidate All. */
            break;

        default:
            rc = VMM_ERR_FAIL;
            break;
    }

    vmm_spin_unlock(&s->lock);

    return rc;
}

static int arm11mpcore_reg_read(struct arm11mpcore_priv_state *s, uint32_t offset, uint32_t *dst)
{
    int rc;

    if (offset < 0x100) {
        /* Read SCU block */
        rc = arm11mpcore_scu_read(s, offset & 0xFC, dst);
    } else if (0x600 <= offset && offset < 0x700) {
        /* Read Private & Watchdog Timer blocks */
        rc = mptimer_reg_read(s->mpt, offset & 0xFC, dst);
    } else {
        /* Read GIC */
        rc = gic_reg_read(s->gic, offset, dst);
    }

    return rc;
}

static int arm11mpcore_reg_write(struct arm11mpcore_priv_state *s, uint32_t offset, uint32_t src_mask, uint32_t src)
{
    int rc;

    if (offset < 0x100) {
        /* Write SCU */
        rc = arm11mpcore_scu_write(s, offset & 0xFC, src_mask, src);
    } else if (0x600 <= offset && offset < 0x700) {
        /* Write Private & Watchdog Timer blocks */
        rc = mptimer_reg_write(s->mpt, offset & 0xFC, src_mask, src);
    } else {
        /* Write GIC */
        rc = gic_reg_write(s->gic, offset, src_mask, src);
    }

    return rc;
}

static int arm11mpcore_emulator_read8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = arm11mpcore_reg_read(edev->private, offset, &regval);

    if (!rc) {
        *dst = regval & 0xFF;
    }

    return rc;
}

static int arm11mpcore_emulator_read16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = arm11mpcore_reg_read(edev->private, offset, &regval);

    if (!rc) {
        *dst = regval & 0xFFFF;
    }

    return rc;
}

static int arm11mpcore_emulator_read32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst)
{
    return arm11mpcore_reg_read(edev->private, offset, dst);
}

static int arm11mpcore_emulator_write8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t src)
{
    return arm11mpcore_reg_write(edev->private, offset, 0xFFFFFF00, src);
}

static int arm11mpcore_emulator_write16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t src)
{
    return arm11mpcore_reg_write(edev->private, offset, 0xFFFF0000, src);
}

static int arm11mpcore_emulator_write32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src)
{
    return arm11mpcore_reg_write(edev->private, offset, 0x00000000, src);
}

static int arm11mpcore_emulator_reset(vmm_emulate_device_t *edev)
{
    struct arm11mpcore_priv_state *s = edev->private;

    /* Reset SCU state */
    s->scu_control                   = 0;

    /* Reset GIC state */
    gic_state_reset(s->gic);

    /* Reset Private & Watchdog Timer state */
    mptimer_state_reset(s->mpt);

    return VMM_OK;
}

static int arm11mpcore_emulator_probe(struct vmm_guest *guest, vmm_emulate_device_t *edev, const struct vmm_device_tree_nodeid *eid)
{
    int                            rc = VMM_OK;
    struct arm11mpcore_priv_state *s;
    uint32_t                       parent_irq, timer_irq[2];

    s = vmm_zalloc(sizeof(struct arm11mpcore_priv_state));

    if (!s) {
        rc = VMM_ERR_NOMEM;
        goto arm11mp_probe_done;
    }

    s->num_cpu = guest->vcpu_count;

    rc         = vmm_device_tree_read_u32(edev->node, "parent_irq", &parent_irq);

    if (rc) {
        goto arm11mp_probe_failed;
    }

    rc = vmm_device_tree_read_u32_array(edev->node, "timer_irq", timer_irq, array_size(timer_irq));

    if (rc) {
        goto arm11mp_probe_failed;
    }

    /* Allocate and init MPT state */
    if (!(s->mpt = mptimer_state_alloc(guest, edev, s->num_cpu, 1000000, timer_irq[0], timer_irq[1]))) {
        rc = VMM_ERR_NOMEM;
        goto arm11mp_probe_failed;
    }

    /* Allocate and init GIC state */
    if (!(s->gic = gic_state_alloc(edev->node->name, guest, GIC_TYPE_ARM11MPCORE, s->num_cpu, FALSE, 0, 96, parent_irq))) {
        rc = VMM_ERR_NOMEM;
        goto arm11mp_gic_alloc_failed;
    }

    s->guest = guest;
    INIT_SPIN_LOCK(&s->lock);

    edev->private = s;

    goto arm11mp_probe_done;

arm11mp_gic_alloc_failed:
    mptimer_state_free(s->mpt);

arm11mp_probe_failed:
    vmm_free(s);

arm11mp_probe_done:
    return rc;
}

static int arm11mpcore_emulator_remove(vmm_emulate_device_t *edev)
{
    struct arm11mpcore_priv_state *s = edev->private;

    if (s) {
        /* Remove GIC state */
        gic_state_free(s->gic);

        /* Remove MPtimer state */
        mptimer_state_free(s->mpt);

        vmm_free(s);
    }

    return VMM_OK;
}

static struct vmm_device_tree_nodeid arm11mpcore_emuid_table[] = {
    {
     .type       = "misc",
     .compatible = "arm,arm11mpcore",
     .data       = NULL,
     },
    {/* end of list */},
};

static vmm_emulator_t arm11mpcore_emulator = {
    .name        = "arm11mpcore",
    .match_table = arm11mpcore_emuid_table,
    .endian      = VMM_DEVICE_EMULATE_LITTLE_ENDIAN,
    .probe       = arm11mpcore_emulator_probe,
    .read8       = arm11mpcore_emulator_read8,
    .write8      = arm11mpcore_emulator_write8,
    .read16      = arm11mpcore_emulator_read16,
    .write16     = arm11mpcore_emulator_write16,
    .read32      = arm11mpcore_emulator_read32,
    .write32     = arm11mpcore_emulator_write32,
    .reset       = arm11mpcore_emulator_reset,
    .remove      = arm11mpcore_emulator_remove,
};

static int __init arm11mpcore_emulator_init(void)
{
    return vmm_device_emulate_register_emulator(&arm11mpcore_emulator);
}

static void __exit arm11mpcore_emulator_exit(void)
{
    vmm_device_emulate_unregister_emulator(&arm11mpcore_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
