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
 * @file pl031.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief PrimeCell PL031 RTC emulator.
 * @details This source file implements the PrimeCell PL031 RTC emulator.
 *
 * The source has been largely adapted from QEMU 0.15.xx hw/pl031.c
 *
 * ARM AMBA PrimeCell PL031 RTC
 *
 * Copyright (c) 2007 CodeSourcery
 *
 * The original code is licensed under the GPL.
 */

#include <libs/mathlib.h>
#include <vmm_device_emulate.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_timer.h>
#include <vmm_wall_clock.h>

#define MODULE_DESC      "PL031 RTC Emulator"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      pl031_emulator_init
#define MODULE_EXIT      pl031_emulator_exit

#define RTC_DR           0x00 /* Data read register */
#define RTC_MR           0x04 /* Match register */
#define RTC_LR           0x08 /* Data load register */
#define RTC_CR           0x0c /* Control register */
#define RTC_IMSC         0x10 /* Interrupt mask and set register */
#define RTC_RIS          0x14 /* Raw interrupt status register */
#define RTC_MIS          0x18 /* Masked interrupt status register */
#define RTC_ICR          0x1c /* Interrupt clear register */

struct pl031_state {
    struct vmm_guest *guest;
    vmm_timer_event_t event;
    vmm_spinlock_t    lock;
    uint32_t          irq;
    uint32_t          tick_offset;
    uint64_t          tick_tstamp;
    uint32_t          mr;
    uint32_t          lr;
    uint32_t          cr;
    uint32_t          im;
    uint32_t          is;
};

static const unsigned char pl031_id[] = {
    0x31, 0x10, 0x14, 0x00, /* Device ID        */
    0x0d, 0xf0, 0x05, 0xb1  /* Cell ID      */
};

static void pl031_update(struct pl031_state *s)
{
    vmm_device_emulate_emulate_irq(s->guest, s->irq, s->is & s->im);
}

static void pl031_timer_event(vmm_timer_event_t *event)
{
    struct pl031_state *s = (struct pl031_state *)event->private;

    s->im                 = 1;
    pl031_update(s);
}

static uint32_t pl031_get_count(struct pl031_state *s)
{
    return s->tick_offset + (uint32_t)udiv64(vmm_timer_timestamp() - s->tick_tstamp, 1000000000);
}

static void pl031_set_alarm(struct pl031_state *s)
{
    uint32_t ticks = pl031_get_count(s);

    /* If timer wraps around then subtraction also
     * wraps in the same way, and gives correct results
     * when alarm < now_ticks.
     */
    ticks          = s->mr - ticks;

    if (ticks == 0) {
        vmm_timer_event_stop(&s->event);
        s->im = 1;
        pl031_update(s);
    } else {
        vmm_timer_event_start(&s->event, ((uint64_t)ticks) * ((uint64_t)1000000000));
    }
}

static int pl031_reg_read(struct pl031_state *s, uint32_t offset, uint32_t *dst)
{
    int rc = VMM_OK;

    vmm_spin_lock(&s->lock);

    if (offset >= 0xFE0 && offset < 0x1000) {
        *dst = (uint32_t)pl031_id[(offset - 0xFE0) >> 2];
    } else {
        switch (offset) {
            case RTC_DR:
                *dst = pl031_get_count(s);
                break;

            case RTC_MR:
                *dst = s->mr;
                break;

            case RTC_IMSC:
                *dst = s->im;
                break;

            case RTC_RIS:
                *dst = s->is;
                break;

            case RTC_LR:
                *dst = s->lr;
                break;

            case RTC_CR:
                /* RTC is permanently enabled.  */
                *dst = 1;
                break;

            case RTC_MIS:
                *dst = s->is & s->im;
                break;

            case RTC_ICR:
            default:
                rc = VMM_EFAIL;
                break;
        };
    }

    vmm_spin_unlock(&s->lock);

    return rc;
}

static int pl031_reg_write(struct pl031_state *s, uint32_t offset, uint32_t src_mask, uint32_t src)
{
    int rc = VMM_OK;

    vmm_spin_lock(&s->lock);

    switch (offset) {
        case RTC_LR:
            s->tick_offset += (src & ~src_mask) - pl031_get_count(s);
            s->tick_tstamp = vmm_timer_timestamp();
            pl031_set_alarm(s);
            break;

        case RTC_MR:
            s->mr &= src_mask;
            s->mr |= (src & ~src_mask);
            pl031_set_alarm(s);
            break;

        case RTC_IMSC:
            s->im &= src_mask;
            s->im |= (src & ~src_mask) & 1;
            pl031_update(s);
            break;

        case RTC_ICR:
            /* The PL031 documentation (DDI0224B) states that the interrupt
             * is cleared when bit 0 of the written value is set. However
             * the arm926e documentation (DDI0287B) states that the
             * interrupt is cleared when any value is written.
             */
            s->is = 0;
            pl031_update(s);
            break;

        case RTC_CR:
            /* Written value is ignored.  */
            break;

        case RTC_DR:
        case RTC_MIS:
        case RTC_RIS:
        default:
            rc = VMM_EFAIL;
            break;
    };

    vmm_spin_unlock(&s->lock);

    return rc;
}

static int pl031_emulator_read8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = pl031_reg_read(edev->private, offset, &regval);

    if (!rc) {
        *dst = regval & 0xFF;
    }

    return rc;
}

static int pl031_emulator_read16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = pl031_reg_read(edev->private, offset, &regval);

    if (!rc) {
        *dst = regval & 0xFFFF;
    }

    return rc;
}

static int pl031_emulator_read32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst)
{
    return pl031_reg_read(edev->private, offset, dst);
}

static int pl031_emulator_write8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t src)
{
    return pl031_reg_write(edev->private, offset, 0xFFFFFF00, src);
}

static int pl031_emulator_write16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t src)
{
    return pl031_reg_write(edev->private, offset, 0xFFFF0000, src);
}

static int pl031_emulator_write32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src)
{
    return pl031_reg_write(edev->private, offset, 0x00000000, src);
}

static int pl031_emulator_reset(vmm_emulate_device_t *edev)
{
    struct pl031_state *s = edev->private;
    vmm_time_value_t    tv;
    vmm_timezone_t      tz;
    int                 rc = VMM_OK;

    vmm_spin_lock(&s->lock);

    if ((rc = vmm_wall_clock_get_timeofday(&tv, &tz))) {
        goto pl031_emulator_reset_done;
    }

    s->tick_offset = (uint32_t)(tv.tv_sec - (tz.tz_minutes_greenwich * 60));
    s->tick_tstamp = vmm_timer_timestamp();

    vmm_timer_event_stop(&s->event);
    s->im = 0;
    pl031_update(s);

pl031_emulator_reset_done:

    vmm_spin_unlock(&s->lock);

    return rc;
}

static int pl031_emulator_probe(struct vmm_guest *guest, vmm_emulate_device_t *edev, const struct vmm_device_tree_nodeid *eid)
{
    int                 rc = VMM_OK;
    struct pl031_state *s;

    s = vmm_zalloc(sizeof(struct pl031_state));

    if (!s) {
        rc = VMM_EFAIL;
        goto pl031_emulator_probe_done;
    }

    s->guest = guest;
    INIT_SPIN_LOCK(&s->lock);

    rc = vmm_device_tree_read_u32_atindex(edev->node, VMM_DEVICE_TREE_INTERRUPTS_ATTR_NAME, &s->irq, 0);

    if (rc) {
        goto pl031_emulator_probe_freestate_fail;
    }

    INIT_TIMER_EVENT(&s->event, &pl031_timer_event, s);

    edev->private = s;

    goto pl031_emulator_probe_done;

pl031_emulator_probe_freestate_fail:
    vmm_free(s);
pl031_emulator_probe_done:
    return rc;
}

static int pl031_emulator_remove(vmm_emulate_device_t *edev)
{
    struct pl031_state *s = edev->private;

    if (s) {
        vmm_free(s);
        edev->private = NULL;
    }

    return VMM_OK;
}

static struct vmm_device_tree_nodeid pl031_emuid_table[] = {
    {
     .type       = "rtc",
     .compatible = "primecell,pl031",
     .data       = NULL,
     },
    {/* end of list */},
};

static vmm_emulator_t pl031_emulator = {
    .name        = "pl031",
    .match_table = pl031_emuid_table,
    .endian      = VMM_DEVICE_EMULATE_LITTLE_ENDIAN,
    .probe       = pl031_emulator_probe,
    .read8       = pl031_emulator_read8,
    .write8      = pl031_emulator_write8,
    .read16      = pl031_emulator_read16,
    .write16     = pl031_emulator_write16,
    .read32      = pl031_emulator_read32,
    .write32     = pl031_emulator_write32,
    .reset       = pl031_emulator_reset,
    .remove      = pl031_emulator_remove,
};

static int __init pl031_emulator_init(void)
{
    return vmm_device_emulate_register_emulator(&pl031_emulator);
}

static void __exit pl031_emulator_exit(void)
{
    vmm_device_emulate_unregister_emulator(&pl031_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
