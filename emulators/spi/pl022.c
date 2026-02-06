/**
 * Copyright (c) 2017 Chaitanya Dhere.
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
 * @file pl022.c
 * @author Chaitanya Dhere (chaitanyadhere1@gmail.comw)
 * @brief PrimeCell PL022 SSP emulator.
 * @details This source file implements the PrimeCell PL022 SSP emulator.
 *
 * The source has been largely adapted from QEMU 2.8.xx hw/pl022.c
 *
 * Arm PrimeCell PL022 SSP
 *
 * Copyright (c) 2006 CodeSourcery.
 * Written by Paul Brook
 * The original code is licensed under the GPL.
 */

#include <vio/vmm_vspi.h>
#include <vmm_device_emulate.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "PL022 Serial Emulator"
#define MODULE_AUTHOR    "Chaitanya Dhere"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (VMM_VSPI_IPRIORITY + 1)
#define MODULE_INIT      pl022_emulator_init
#define MODULE_EXIT      pl022_emulator_exit

#define PL022_CR1_LBM    0x01
#define PL022_CR1_SSE    0x02
#define PL022_CR1_MS     0x04
#define PL022_CR1_SDO    0x08

#define PL022_SR_TFE     0x01
#define PL022_SR_TNF     0x02
#define PL022_SR_RNE     0x04
#define PL022_SR_RFF     0x08
#define PL022_SR_BSY     0x10

#define PL022_INT_ROR    0x01
#define PL022_INT_RT     0x04
#define PL022_INT_RX     0x04
#define PL022_INT_TX     0x08

struct pl022_state {
    struct vmm_guest *guest;
    vmm_spinlock_t    lock;

    uint32_t cr0;
    uint32_t cr1;
    uint32_t bitmask;
    uint32_t sr;
    uint32_t cpsr;
    uint32_t is;
    uint32_t im;
    /* The FIFO head points to the next empty entry. */
    int      tx_fifo_head;
    int      rx_fifo_head;
    int      tx_fifo_len;
    int      rx_fifo_len;
    uint16_t tx_fifo[8];
    uint16_t rx_fifo[8];
    uint32_t irq;
    int      int_level;
    int      int_enabled;
    uint8_t  id[8];

    vmm_virtual_spi_host_t *vsh;
};

static uint32_t pl022_id[8] = {0x22, 0x10, 0x04, 0x00, 0x0d, 0xf0, 0x05, 0xb1};

/* Note: Must be called with s->lock held */
static void __pl022_set_irq(struct pl022_state *s, uint32_t level, uint32_t enabled)
{
    if (level & enabled) {
        vmm_device_emulate_emulate_irq(s->guest, s->irq, 1);
    } else {
        vmm_device_emulate_emulate_irq(s->guest, s->irq, 0);
    }
}

/* Note: Must be called with s->lock held */
static void __pl022_update(struct pl022_state *s)
{
    bool     set_irq = FALSE;
    uint32_t level = 0, enabled = 0;

    s->sr = 0;

    if (s->tx_fifo_len == 0) {
        s->sr |= PL022_SR_TFE;
    }

    if (s->tx_fifo_len != 8) {
        s->sr |= PL022_SR_TNF;
        set_irq = TRUE;
    }

    if (s->rx_fifo_len != 0) {
        s->sr |= PL022_SR_RNE;
        set_irq = TRUE;
    }

    if (s->rx_fifo_len == 8) {
        s->sr |= PL022_SR_RFF;
    }

    if (s->tx_fifo_len) {
        s->sr |= PL022_SR_BSY;
    }

    s->is = 0;

    if (s->rx_fifo_len >= 4) {
        s->is |= PL022_INT_RX;
        set_irq      = TRUE;
        s->int_level = PL022_INT_RX;
        level        = s->int_level;
        enabled      = s->int_enabled;
    }

    if (s->tx_fifo_len <= 4) {
        s->is |= PL022_INT_TX;
        set_irq      = TRUE;
        s->int_level = PL022_INT_TX;
        level        = s->int_level;
        enabled      = s->int_enabled;
    }

    if (set_irq) {
        __pl022_set_irq(s, level, enabled);
    }
}

static void pl022_xfer(vmm_virtual_spi_host_t *vsh, void *private)
{
    uint32_t            val;
    int                 i, o;
    irq_flags_t         flags;
    struct pl022_state *s = private;

    vmm_spin_lock_irq_save(&s->lock, flags);

    if ((s->cr1 & PL022_CR1_SSE) == 0) {
        __pl022_update(s);
        vmm_spin_unlock_irq_restore(&s->lock, flags);
        return;
    }

    i = (s->tx_fifo_head - s->tx_fifo_len) & 7;
    o = s->rx_fifo_head;

    /* ??? We do not emulate the line speed.
    This may break some applications. The are two problematic cases:
    (a) A driver feeds data into the TX FIFO until it is full,
    and only then drains the RX FIFO. On real hardware the CPU can
    feed data fast enough that the RX fifo never gets chance to overflow.
    (b) A driver transmits data, deliberately allowing the RX FIFO to
    overflow because it ignores the RX data anyway.

    We choose to support (a) by stalling the transmit engine if it would
    cause the RX FIFO to overflow. In practice much transmit-only code
    falls into (a) because it flushes the RX FIFO to determine when
    the transfer has completed. */
    while (s->tx_fifo_len && s->rx_fifo_len < 8) {
        val = s->tx_fifo[i];

        if (s->cr1 & PL022_CR1_LBM) {
            /* Loopback mode. */
        } else {
            vmm_spin_unlock_irq_restore(&s->lock, flags);
            val = vmm_vspihost_xfer_data(s->vsh, 0, val);
            vmm_spin_lock_irq_save(&s->lock, flags);
        }

        s->rx_fifo[o] = val & s->bitmask;
        i             = (i + 1) & 7;
        o             = (o + 1) & 7;
        s->tx_fifo_len--;
        s->rx_fifo_len++;
    }

    s->rx_fifo_head = o;

    __pl022_update(s);

    vmm_spin_unlock_irq_restore(&s->lock, flags);
}

static uint64_t pl022_read(struct pl022_state *s, uint32_t offset, uint32_t *dst)
{
    int val, rc = VMM_OK;

    vmm_spin_lock(&s->lock);

    switch (offset) {
        case 0x00: /* CR0 */
            *dst = s->cr0;
            break;

        case 0x04: /* CR1 */
            *dst = s->cr1;
            break;

        case 0x08: /* DR */
            if (s->rx_fifo_len) {
                val = s->rx_fifo[(s->rx_fifo_head - s->rx_fifo_len) & 7];
                s->rx_fifo_len--;
                vmm_vspihost_schedule_xfer(s->vsh);
            } else {
                val = 0;
            }

            *dst = val;
            break;

        case 0x0c: /* SR */
            *dst = s->sr;
            break;

        case 0x10: /* CPSR */
            *dst = s->cpsr;
            break;

        case 0x14: /* IMSC */
            *dst = s->im;
            break;

        case 0x18: /* RIS */
            *dst = s->is;
            break;

        case 0x1c: /* MIS */
            *dst = s->im & s->is;
            break;

        case 0x20: /* DMACR */
            /* Not implemented. */
            break;

        default:
            if (offset >= 0xfe0 && offset < 0x1000) {
                *dst = s->id[(offset - 0xfe0) >> 2];
            } else {
                rc = VMM_EFAIL;
            }

            break;
    }

    vmm_spin_unlock(&s->lock);

    return rc;
}

static int pl022_write(struct pl022_state *s, uint32_t offset, uint32_t value, uint32_t src)
{
    int rc = VMM_OK;

    vmm_spin_lock(&s->lock);

    switch (offset) {
        case 0x00: /* CR0 */
            s->cr0     = src;
            /* Clock rate and format are ignored. */
            s->bitmask = (1 << ((src & 15) + 1)) - 1;
            break;

        case 0x04: /* CR1 */
            s->cr1 = src;

            if (s->cr1 & PL022_CR1_LBM) {
                s->cr1 |= 0x01;
            }

            if ((s->cr1 & (PL022_CR1_MS | PL022_CR1_SSE)) == (PL022_CR1_MS | PL022_CR1_SSE)) {
                /* SPI Slave not implemented */
            }

            s->int_level |= PL022_INT_TX;
            vmm_vspihost_schedule_xfer(s->vsh);
            break;

        case 0x08: /* DR */
            if (s->tx_fifo_len < 8) {
                s->tx_fifo[s->tx_fifo_head] = src & s->bitmask;
                s->tx_fifo_head             = (s->tx_fifo_head + 1) & 7;
                s->tx_fifo_len++;
                vmm_vspihost_schedule_xfer(s->vsh);
            }

            break;

        case 0x10: /* CPSR */
            /* Prescaler. Ignored. */
            s->cpsr = src & 0xff;
            break;

        case 0x14: /* IMSC */
            s->im          = src;
            s->int_enabled = (s->int_enabled & src) | (value & ~src);
            __pl022_update(s);
            break;

        case 0x20: /* DMACR */
            if (src) {
                /*Not implemented */
            }

            break;

        default:
            rc = VMM_OK;
            break;
    }

    vmm_spin_unlock(&s->lock);

    return rc;
}

static int pl022_emulator_reset(vmm_emulate_device_t *edev)
{
    struct pl022_state *s = edev->private;

    vmm_spin_lock(&s->lock);

    s->rx_fifo_len = 0;
    s->tx_fifo_len = 0;
    s->im          = 0;
    s->is          = PL022_INT_TX;
    s->sr          = PL022_SR_TFE | PL022_SR_TNF;

    vmm_spin_unlock(&s->lock);

    return VMM_OK;
}

static int pl022_emulator_read8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = pl022_read(edev->private, offset, &regval);

    if (!rc) {
        *dst = regval & 0xFF;
    }

    return rc;
}

static int pl022_emulator_write8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t src)
{
    return pl022_write(edev->private, offset, 0xFFFFFF00, src);
}

static int pl022_emulator_read16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = pl022_read(edev->private, offset, &regval);

    if (!rc) {
        *dst = regval & 0xFFFF;
    }

    return rc;
}

static int pl022_emulator_read32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst)
{
    return pl022_read(edev->private, offset, dst);
}

static int pl022_emulator_write16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t src)
{
    return pl022_write(edev->private, offset, 0xFFFF0000, src);
}

static int pl022_emulator_write32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src)
{
    return pl022_write(edev->private, offset, 0x00000000, src);
}

static int pl022_emulator_probe(struct vmm_guest *guest, vmm_emulate_device_t *edev, const struct vmm_device_tree_nodeid *eid)
{
    int                 rc = VMM_OK;
    struct pl022_state *s;

    s = vmm_zalloc(sizeof(struct pl022_state));

    if (!s) {
        rc = VMM_EFAIL;
        goto pl022_emulator_probe_done;
    }

    s->guest = guest;
    INIT_SPIN_LOCK(&s->lock);

    if (eid->data) {
        s->id[0] = ((uint32_t *)eid->data)[0];
        s->id[1] = ((uint32_t *)eid->data)[1];
        s->id[2] = ((uint32_t *)eid->data)[2];
        s->id[3] = ((uint32_t *)eid->data)[3];
        s->id[4] = ((uint32_t *)eid->data)[4];
        s->id[5] = ((uint32_t *)eid->data)[5];
        s->id[6] = ((uint32_t *)eid->data)[6];
        s->id[7] = ((uint32_t *)eid->data)[7];
    }

    rc = vmm_device_tree_read_u32_atindex(edev->node, VMM_DEVICE_TREE_INTERRUPTS_ATTR_NAME, &s->irq, 0);

    if (rc) {
        goto pl022_emulator_probe_freestate_fail;
    }

    s->vsh = vmm_vspihost_create(guest->name, edev, pl022_xfer, 1, s);

    if (!s->vsh) {
        rc = VMM_EFAIL;
        goto pl022_emulator_probe_freestate_fail;
    }

    edev->private = s;

    goto pl022_emulator_probe_done;

pl022_emulator_probe_freestate_fail:
    vmm_free(s);
pl022_emulator_probe_done:
    return rc;
}

static int pl022_emulator_remove(vmm_emulate_device_t *edev)
{
    struct pl022_state *s = edev->private;

    if (s) {
        vmm_vspihost_destroy(s->vsh);
        vmm_free(s);
        edev->private = NULL;
    }

    return VMM_OK;
}

static struct vmm_device_tree_nodeid pl022_emuid_table[] = {
    {
     /*
     * ARM PL022 variant, this has a 16bit wide
     * and 8 locations deep TX/RX FIFO
     */
        .type       = "spi-host",
     .compatible = "primecell,arm,pl022",
     .data       = &pl022_id[0],
     },
    {},
};

static vmm_emulator_t pl022_emulator = {
    .name        = "pl022",
    .match_table = pl022_emuid_table,
    .endian      = VMM_DEVICE_EMULATE_LITTLE_ENDIAN,
    .probe       = pl022_emulator_probe,
    .read8       = pl022_emulator_read8,
    .write8      = pl022_emulator_write8,
    .read16      = pl022_emulator_read16,
    .write16     = pl022_emulator_write16,
    .read32      = pl022_emulator_read32,
    .write32     = pl022_emulator_write32,
    .reset       = pl022_emulator_reset,
    .remove      = pl022_emulator_remove,
};

static int __init pl022_emulator_init(void)
{
    return vmm_device_emulate_register_emulator(&pl022_emulator);
}

static void __exit pl022_emulator_exit(void)
{
    vmm_device_emulate_unregister_emulator(&pl022_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
