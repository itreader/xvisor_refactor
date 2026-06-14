/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file goldfish_rtc.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Google Goldfish RTC emulator.
 * @details This source file implements the Google Goldfish RTC device.
 *
 * For more details on Google Goldfish virtual platform RTC device refer:
 * https://android.googlesource.com/platform/external/qemu/+/master/docs/GOLDFISH-VIRTUAL-HARDWARE.TXT
 */

#include <libs/mathlib.h>
#include <vmm_device_emulate.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_timer.h>
#include <vmm_wall_clock.h>

#define MODULE_DESC         "Goldfish RTC Emulator"
#define MODULE_AUTHOR       "Anup Patel"
#define MODULE_LICENSE      "GPL"
#define MODULE_IPRIORITY    0
#define MODULE_INIT         goldfish_rtc_emulator_init
#define MODULE_EXIT         goldfish_rtc_emulator_exit

#define RTC_TIME_LOW        0x00
#define RTC_TIME_HIGH       0x04
#define RTC_ALARM_LOW       0x08
#define RTC_ALARM_HIGH      0x0c
#define RTC_IRQ_ENABLED     0x10
#define RTC_CLEAR_ALARM     0x14
#define RTC_ALARM_STATUS    0x18
#define RTC_CLEAR_INTERRUPT 0x1c

struct goldfish_rtc_state {
    struct vmm_guest *guest;
    vmm_timer_event_t event;
    vmm_spinlock_t    lock;
    uint32_t          irq;
    uint64_t          tick_offset;
    uint64_t          tick_tstamp;
    uint64_t          alarm_next;
    uint32_t          alarm_running;
    uint32_t          irq_pending;
    uint32_t          irq_enabled;
};

/* Note: Must be called with s->lock held */
static void goldfish_rtc_update(struct goldfish_rtc_state *s)
{
    vmm_device_emulate_emulate_irq(s->guest, s->irq, (s->irq_pending & s->irq_enabled) ? 1 : 0);
}

static void goldfish_rtc_timer_event(vmm_timer_event_t *event)
{
    struct goldfish_rtc_state *s = event->private;

    vmm_spin_lock(&s->lock);

    s->alarm_running = 0;
    s->irq_pending   = 1;
    goldfish_rtc_update(s);

    vmm_spin_unlock(&s->lock);
}

/* Note: Must be called with s->lock held */
static uint64_t goldfish_rtc_get_count(struct goldfish_rtc_state *s)
{
    return s->tick_offset + (vmm_timer_timestamp() - s->tick_tstamp);
}

/* Note: Must be called with s->lock held */
static void goldfish_rtc_clear_alarm(struct goldfish_rtc_state *s)
{
    vmm_timer_event_stop(&s->event);
    s->alarm_running = 0;
}

/* Note: Must be called with s->lock held */
static void goldfish_rtc_set_alarm(struct goldfish_rtc_state *s)
{
    uint64_t now   = goldfish_rtc_get_count(s);
    uint64_t event = s->alarm_next;

    if (event <= now) {
        goldfish_rtc_clear_alarm(s);
        s->irq_pending = 1;
        goldfish_rtc_update(s);
    } else {
        s->alarm_running = 1;
        vmm_timer_event_start(&s->event, event - now);
    }
}

static int goldfish_rtc_emulator_read(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst, uint32_t size)
{
    int                        rc = VMM_OK;
    struct goldfish_rtc_state *s  = edev->private;

    vmm_spin_lock(&s->lock);

    switch (offset) {
        case RTC_TIME_LOW:
            *dst = (uint32_t)goldfish_rtc_get_count(s);
            break;

        case RTC_TIME_HIGH:
            *dst = (uint32_t)(goldfish_rtc_get_count(s) >> 32);
            break;

        case RTC_ALARM_LOW:
            *dst = (uint32_t)s->alarm_next;
            break;

        case RTC_ALARM_HIGH:
            *dst = (uint32_t)(s->alarm_next >> 32);
            break;

        case RTC_IRQ_ENABLED:
            *dst = s->irq_enabled;
            break;

        case RTC_ALARM_STATUS:
            *dst = s->alarm_running;
            break;

        default:
            rc = VMM_ERR_INVALID;
            break;
    };

    vmm_spin_unlock(&s->lock);

    return rc;
}

static int goldfish_rtc_emulator_write(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src_mask, uint32_t src, uint32_t size)
{
    uint32_t                   val;
    int                        rc = VMM_OK;
    struct goldfish_rtc_state *s  = edev->private;

    vmm_spin_lock(&s->lock);

    switch (offset) {
        case RTC_TIME_LOW:
            val = (uint32_t)s->tick_offset;
            val &= src_mask;
            val |= src;
            s->tick_offset &= (0xffffffffULL << 32);
            s->tick_offset |= ((uint64_t)val);
            s->tick_tstamp = vmm_timer_timestamp();
            break;

        case RTC_TIME_HIGH:
            val = (uint32_t)(s->tick_offset >> 32);
            val &= src_mask;
            val |= src;
            s->tick_offset &= 0xffffffffULL;
            s->tick_offset |= (((uint64_t)val) << 32);
            s->tick_tstamp = vmm_timer_timestamp();
            break;

        case RTC_ALARM_LOW:
            val = (uint32_t)s->alarm_next;
            val &= src_mask;
            val |= src;
            s->alarm_next &= (0xffffffffULL << 32);
            s->alarm_next |= ((uint64_t)val);
            goldfish_rtc_set_alarm(s);
            break;

        case RTC_ALARM_HIGH:
            val = (uint32_t)(s->alarm_next >> 32);
            val &= src_mask;
            val |= src;
            s->alarm_next &= 0xffffffffULL;
            s->alarm_next |= (((uint64_t)val) << 32);
            break;

        case RTC_IRQ_ENABLED:
            s->irq_enabled = ((s->irq_enabled & src_mask) | src) & 0x1;
            goldfish_rtc_update(s);
            break;

        case RTC_CLEAR_ALARM:
            goldfish_rtc_clear_alarm(s);
            break;

        case RTC_CLEAR_INTERRUPT:
            s->irq_pending = 0;
            goldfish_rtc_update(s);
            break;

        default:
            rc = VMM_ERR_INVALID;
            break;
    };

    vmm_spin_unlock(&s->lock);

    return rc;
}

static int goldfish_rtc_emulator_reset(vmm_emulate_device_t *edev)
{
    struct goldfish_rtc_state *s = edev->private;
    vmm_time_value_t           tv;
    vmm_timezone_t             tz;
    int                        rc = VMM_OK;

    vmm_spin_lock(&s->lock);

    if ((rc = vmm_wall_clock_get_timeofday(&tv, &tz))) {
        goto goldfish_rtc_emulator_reset_done;
    }

    s->tick_offset = (uint64_t)(tv.tv_sec - (tz.tz_minutes_greenwich * 60));
    s->tick_offset *= 1000000000ULL;
    s->tick_offset += (uint64_t)tv.tv_nsec;
    s->tick_tstamp   = vmm_timer_timestamp();

    s->alarm_next    = s->tick_offset;
    s->alarm_running = 0;
    s->irq_pending   = 0;
    s->irq_enabled   = 0;

    vmm_timer_event_stop(&s->event);

    goldfish_rtc_update(s);

goldfish_rtc_emulator_reset_done:

    vmm_spin_unlock(&s->lock);

    return rc;
}

static int goldfish_rtc_emulator_probe(struct vmm_guest *guest, vmm_emulate_device_t *edev, const struct vmm_device_tree_nodeid *eid)
{
    int                        rc = VMM_OK;
    struct goldfish_rtc_state *s;

    s = vmm_zalloc(sizeof(struct goldfish_rtc_state));

    if (!s) {
        rc = VMM_ERR_FAIL;
        goto goldfish_rtc_emulator_probe_done;
    }

    s->guest = guest;
    INIT_SPIN_LOCK(&s->lock);

    rc = vmm_device_tree_read_u32_atindex(edev->node, VMM_DEVICE_TREE_INTERRUPTS_ATTR_NAME, &s->irq, 0);

    if (rc) {
        goto goldfish_rtc_emulator_probe_freestate_fail;
    }

    INIT_TIMER_EVENT(&s->event, &goldfish_rtc_timer_event, s);

    edev->private = s;

    goto goldfish_rtc_emulator_probe_done;

goldfish_rtc_emulator_probe_freestate_fail:
    vmm_free(s);
goldfish_rtc_emulator_probe_done:
    return rc;
}

static int goldfish_rtc_emulator_remove(vmm_emulate_device_t *edev)
{
    struct goldfish_rtc_state *s = edev->private;

    if (!s) {
        return VMM_ERR_INVALID;
    }

    vmm_timer_event_stop(&s->event);
    vmm_free(s);
    edev->private = NULL;

    return VMM_OK;
}

static struct vmm_device_tree_nodeid goldfish_rtc_emuid_table[] = {
    {
     .type       = "rtc",
     .compatible = "google,goldfish-rtc",
     .data       = NULL,
     },
    {/* end of list */},
};

VMM_DECLARE_EMULATOR_SIMPLE(
    goldfish_rtc_emulator, "goldfish_rtc_emulator", goldfish_rtc_emuid_table, VMM_DEVICE_EMULATE_LITTLE_ENDIAN, goldfish_rtc_emulator_probe,
    goldfish_rtc_emulator_remove, goldfish_rtc_emulator_reset, NULL, goldfish_rtc_emulator_read, goldfish_rtc_emulator_write);

static int __init goldfish_rtc_emulator_init(void)
{
    return vmm_device_emulate_register_emulator(&goldfish_rtc_emulator);
}

static void __exit goldfish_rtc_emulator_exit(void)
{
    vmm_device_emulate_unregister_emulator(&goldfish_rtc_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
