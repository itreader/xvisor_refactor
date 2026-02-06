/*
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file hpet.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief High Precision Event Timer Emulator
 *
 * This work is derived from Qemu/hw/timer/hpet.c
 *
 *  Copyright (c) 2007 Alexander Graf
 *  Copyright (c) 2008 IBM Corporation
 *
 *  Authors: Beth Kon <bkon@us.ibm.com>
 *
 * This driver attempts to emulate an HPET device in software.
 */

#include <vmm_device_emulate.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_vcpu_irq.h>

#include <libs/mathlib.h>

#include <emu/hpet.h>
#include <emu/i8254.h>
#include <emu/rtc/mc146818rtc.h>

// #define HPET_DEBUG
#ifdef HPET_DEBUG
#define DPRINTF vmm_printf
#else
#define DPRINTF(...)
#endif

#define HPET_MSI_SUPPORT 0

#define MODULE_DESC      "High Precision Event Timer Emulator"
#define MODULE_AUTHOR    "Himanshu Chauhan"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY 0
#define MODULE_INIT      hpet_emulator_init
#define MODULE_EXIT      hpet_emulator_exit

struct hpet_state;

typedef struct hpet_timer { /* timers */
    uint8_t            tn;  /*timer number*/
    vmm_timer_event_t  timer;
    struct hpet_state *state;
    /* Memory-mapped, software visible timer registers */
    uint64_t           config; /* configuration/cap */
    uint64_t           cmp;    /* comparator */
    uint64_t           fsb;    /* FSB route */
    /* Hidden register state */
    uint64_t           period;    /* Last value written to comparator */
    uint8_t            wrap_flag; /* timer pop will indicate wrap for one-shot 32-bit
                                   * mode. Next pop will be actual timer expiration.
                                   */
} hpet_timer_t;

typedef struct hpet_state {
    struct vmm_guest *guest;

    uint64_t          hpet_offset;
    uint32_t          irqs[HPET_NUM_IRQ_ROUTES];
    uint32_t          flags;
    uint8_t           rtc_irq_level;
    uint32_t          pit_enabled;
    uint32_t          num_timers;
    uint32_t          intcap;
    struct hpet_timer timer[HPET_MAX_TIMERS];

    /* Memory-mapped, software visible registers */
    uint64_t capability;   /* capabilities */
    uint64_t config;       /* configuration */
    uint64_t isr;          /* interrupt status reg */
    uint64_t hpet_counter; /* main counter */
    uint32_t hpet_id;      /* instance id */
} hpet_state_t;

static uint32_t hpet_in_legacy_mode(struct hpet_state *s)
{
    return s->config & HPET_CFG_LEGACY;
}

static uint32_t timer_int_route(struct hpet_timer *timer)
{
    return (timer->config & HPET_TN_INT_ROUTE_MASK) >> HPET_TN_INT_ROUTE_SHIFT;
}

static uint32_t timer_fsb_route(struct hpet_timer *t)
{
    return t->config & HPET_TN_FSB_ENABLE;
}

static uint32_t hpet_enabled(struct hpet_state *s)
{
    return s->config & HPET_CFG_ENABLE;
}

static uint32_t timer_is_periodic(struct hpet_timer *t)
{
    return t->config & HPET_TN_PERIODIC;
}

static uint32_t timer_enabled(struct hpet_timer *t)
{
    return t->config & HPET_TN_ENABLE;
}

static uint32_t hpet_time_after(uint64_t a, uint64_t b)
{
    return ((int32_t)(b) - (int32_t)(a) < 0);
}

static uint32_t hpet_time_after64(uint64_t a, uint64_t b)
{
    return ((int64_t)(b) - (int64_t)(a) < 0);
}

static uint64_t ticks_to_ns(uint64_t value)
{
    return (muldiv64(value, HPET_CLK_PERIOD, FS_PER_NS));
}

static uint64_t ns_to_ticks(uint64_t value)
{
    return (muldiv64(value, FS_PER_NS, HPET_CLK_PERIOD));
}

static uint64_t hpet_fixup_reg(uint64_t new, uint64_t old, uint64_t mask)
{
    new &= mask;
    new |= old & ~mask;
    return new;
}

static int activating_bit(uint64_t old, uint64_t new, uint64_t mask)
{
    return (!(old & mask) && (new &mask));
}

static int deactivating_bit(uint64_t old, uint64_t new, uint64_t mask)
{
    return ((old & mask) && !(new &mask));
}

static uint64_t hpet_get_ticks(struct hpet_state *s)
{
    return ns_to_ticks(vmm_timer_timestamp() + s->hpet_offset);
}

/*
 * calculate diff between comparator value and current ticks
 */
static inline uint64_t hpet_calculate_diff(struct hpet_timer *t, uint64_t current)
{
    if (t->config & HPET_TN_32BIT) {
        uint32_t diff, cmp;

        cmp  = (uint32_t)t->cmp;
        diff = cmp - (uint32_t)current;
        diff = (int32_t)diff > 0 ? diff : (uint32_t)1;
        return (uint64_t)diff;
    } else {
        uint64_t diff, cmp;

        cmp  = t->cmp;
        diff = cmp - current;
        diff = (int64_t)diff > 0 ? diff : (uint64_t)1;
        return diff;
    }
}

static void update_irq(struct hpet_timer *timer, int set)
{
    uint64_t           mask;
    struct hpet_state *s;
    int                route;

    if (timer->tn <= 1 && hpet_in_legacy_mode(timer->state)) {
        /* if LegacyReplacementRoute bit is set, HPET specification requires
         * timer0 be routed to IRQ0 in NON-APIC or IRQ2 in the I/O APIC,
         * timer1 be routed to IRQ8 in NON-APIC or IRQ8 in the I/O APIC.
         */
        route = (timer->tn == 0) ? 0 : RTC_ISA_IRQ;
    } else {
        route = timer_int_route(timer);
    }

    s    = timer->state;
    mask = 1 << timer->tn;

    if (!set || !timer_enabled(timer) || !hpet_enabled(timer->state)) {
        s->isr &= ~mask;

        if (!timer_fsb_route(timer)) {
#define ISA_NUM_IRQS 16

            /* fold the ICH PIRQ# pin's internal inversion logic into hpet */
            if (route >= ISA_NUM_IRQS) {
                vmm_device_emulate_emulate_irq(s->guest, s->irqs[route], 1);
            } else {
                vmm_device_emulate_emulate_irq(s->guest, s->irqs[route], 0);
            }
        }
    } else if (timer_fsb_route(timer)) {
#if 0
        stl_le_phys(&address_space_memory,
                    timer->fsb >> 32, timer->fsb & 0xffffffff);
#endif
        vmm_panic("Write to fsb route\n");
    } else if (timer->config & HPET_TN_TYPE_LEVEL) {
        s->isr |= mask;

        /* fold the ICH PIRQ# pin's internal inversion logic into hpet */
        if (route >= ISA_NUM_IRQS) {
            vmm_device_emulate_emulate_irq(s->guest, s->irqs[route], 0);
        } else {
            vmm_device_emulate_emulate_irq(s->guest, s->irqs[route], 1);
        }
    } else {
        s->isr &= ~mask;
        vmm_device_emulate_emulate_irq(s->guest, s->irqs[route], 1);
        vmm_device_emulate_emulate_irq(s->guest, s->irqs[route], 0);
    }
}

/*
 * timer expiration callback
 */
static void hpet_timer(vmm_timer_event_t *event)
{
    struct hpet_timer *t = event->private;
    uint64_t           diff;

    uint64_t period   = t->period;
    uint64_t cur_tick = hpet_get_ticks(t->state);

    if (timer_is_periodic(t) && period != 0) {
        if (t->config & HPET_TN_32BIT) {
            while (hpet_time_after(cur_tick, t->cmp)) {
                t->cmp = (uint32_t)(t->cmp + t->period);
            }
        } else {
            while (hpet_time_after64(cur_tick, t->cmp)) {
                t->cmp += period;
            }
        }

        diff = hpet_calculate_diff(t, cur_tick);
        vmm_timer_event_stop(&t->timer);
        vmm_timer_event_start(&t->timer, vmm_timer_timestamp() + (int64_t)ticks_to_ns(diff));
    } else if (t->config & HPET_TN_32BIT && !timer_is_periodic(t)) {
        if (t->wrap_flag) {
            diff = hpet_calculate_diff(t, cur_tick);
            vmm_timer_event_stop(&t->timer);
            vmm_timer_event_start(&t->timer, vmm_timer_timestamp() + (int64_t)ticks_to_ns(diff));
            t->wrap_flag = 0;
        }
    }

    update_irq(t, 1);
}

static void hpet_set_timer(struct hpet_timer *t)
{
    uint64_t diff;
    uint32_t wrap_diff; /* how many ticks until we wrap? */
    uint64_t cur_tick = hpet_get_ticks(t->state);

    /* whenever new timer is being set up, make sure wrap_flag is 0 */
    t->wrap_flag      = 0;
    diff              = hpet_calculate_diff(t, cur_tick);

    /* hpet spec says in one-shot 32-bit mode, generate an interrupt when
     * counter wraps in addition to an interrupt with comparator match.
     */
    if (t->config & HPET_TN_32BIT && !timer_is_periodic(t)) {
        wrap_diff = 0xffffffff - (uint32_t)cur_tick;

        if (wrap_diff < (uint32_t)diff) {
            diff         = wrap_diff;
            t->wrap_flag = 1;
        }
    }

    vmm_timer_event_stop(&t->timer);
    vmm_timer_event_start(&t->timer, vmm_timer_timestamp() + (int64_t)ticks_to_ns(diff));
}

static void hpet_del_timer(struct hpet_timer *t)
{
    vmm_timer_event_stop(&t->timer);
    update_irq(t, 0);
}

static int hpet_ram_read(struct hpet_state *s, physical_addr_t addr, uint64_t *dst)
{
    uint64_t cur_tick, index, retval;

    DPRINTF("qemu: Enter hpet_ram_readl at 0x%lx\n", addr);
    index = addr;

    /*address range of all TN regs*/
    if (index >= 0x100 && index <= 0x3ff) {
        uint8_t            timer_id = (addr - 0x100) / 0x20;
        struct hpet_timer *timer    = &s->timer[timer_id];

        if (timer_id > s->num_timers) {
            DPRINTF("qemu: timer id out of range\n");
            return VMM_ERANGE;
        }

        switch ((addr - 0x100) % 0x20) {
            case HPET_TN_CFG:
                retval = timer->config;
                break;

            case HPET_TN_CFG + 4:  // Interrupt capabilities
                retval = timer->config >> 32;
                break;

            case HPET_TN_CMP:  // comparator register
                retval = timer->cmp;
                break;

            case HPET_TN_CMP + 4:
                retval = timer->cmp >> 32;
                break;

            case HPET_TN_ROUTE:
                retval = timer->fsb;
                break;

            case HPET_TN_ROUTE + 4:
                retval = timer->fsb >> 32;
                break;

            default:
                DPRINTF("qemu: invalid hpet_ram_readl\n");
                return VMM_EINVALID;
        }
    } else {
        switch (index) {
            case HPET_ID:
                retval = s->capability;
                break;

            case HPET_PERIOD:
                retval = s->capability >> 32;
                break;

            case HPET_CFG:
                retval = s->config;
                break;

            case HPET_CFG + 4:
                DPRINTF("qemu: invalid HPET_CFG + 4 hpet_ram_readl\n");
                return VMM_EINVALID;

            case HPET_COUNTER:
                if (hpet_enabled(s)) {
                    cur_tick = hpet_get_ticks(s);
                } else {
                    cur_tick = s->hpet_counter;
                }

                DPRINTF("qemu: reading counter  = 0x%llx", cur_tick);
                retval = cur_tick;
                break;

            case HPET_COUNTER + 4:
                if (hpet_enabled(s)) {
                    cur_tick = hpet_get_ticks(s);
                } else {
                    cur_tick = s->hpet_counter;
                }

                DPRINTF("qemu: reading counter + 4  = 0x%llx\n", cur_tick);
                retval = (cur_tick >> 32);
                break;

            case HPET_STATUS:
                retval = s->isr;
                break;

            default:
                DPRINTF("qemu: invalid hpet_ram_readl\n");
                return VMM_EINVALID;
        }
    }

    if (dst) {
        *dst = retval;
    }

    return VMM_OK;
}

static int hpet_ram_write(struct hpet_state *s, physical_addr_t addr, uint64_t mask, uint64_t value)
{
    int      i;
    uint64_t old_val, new_val, val, index;

    DPRINTF("qemu: Enter hpet_ram_writel at 0x%llx = 0x%llx\n", addr, value);
    index = addr;
    hpet_ram_read(s, addr, &old_val);
    new_val = value;

    /*address range of all TN regs*/
    if (index >= 0x100 && index <= 0x3ff) {
        uint8_t            timer_id = (addr - 0x100) / 0x20;
        struct hpet_timer *timer    = &s->timer[timer_id];

        DPRINTF("qemu: hpet_ram_writel timer_id = %lx\n", timer_id);

        if (timer_id > s->num_timers) {
            DPRINTF("qemu: timer id out of range\n");
            return VMM_ERANGE;
        }

        switch ((addr - 0x100) % 0x20) {
            case HPET_TN_CFG:
                DPRINTF("qemu: hpet_ram_writel HPET_TN_CFG\n");

                if (activating_bit(old_val, new_val, HPET_TN_FSB_ENABLE)) {
                    update_irq(timer, 0);
                }

                val           = hpet_fixup_reg(new_val, old_val, HPET_TN_CFG_WRITE_MASK);
                timer->config = (timer->config & 0xffffffff00000000ULL) | val;

                if (new_val & HPET_TN_32BIT) {
                    timer->cmp    = (uint32_t)timer->cmp;
                    timer->period = (uint32_t)timer->period;
                }

                if (activating_bit(old_val, new_val, HPET_TN_ENABLE) && hpet_enabled(s)) {
                    hpet_set_timer(timer);
                } else if (deactivating_bit(old_val, new_val, HPET_TN_ENABLE)) {
                    hpet_del_timer(timer);
                }

                break;

            case HPET_TN_CFG + 4:  // Interrupt capabilities
                DPRINTF("qemu: invalid HPET_TN_CFG+4 write\n");
                break;

            case HPET_TN_CMP:  // comparator register
                DPRINTF("qemu: hpet_ram_writel HPET_TN_CMP\n");

                if (timer->config & HPET_TN_32BIT) {
                    new_val = (uint32_t)new_val;
                }

                if (!timer_is_periodic(timer) || (timer->config & HPET_TN_SETVAL)) {
                    timer->cmp = (timer->cmp & 0xffffffff00000000ULL) | new_val;
                }

                if (timer_is_periodic(timer)) {
                    /*
                     * FIXME: Clamp period to reasonable min value?
                     * Clamp period to reasonable max value
                     */
                    new_val &= (timer->config & HPET_TN_32BIT ? ~0u : ~0ull) >> 1;
                    timer->period = (timer->period & 0xffffffff00000000ULL) | new_val;
                }

                timer->config &= ~HPET_TN_SETVAL;

                if (hpet_enabled(s)) {
                    hpet_set_timer(timer);
                }

                break;

            case HPET_TN_CMP + 4:  // comparator register high order
                DPRINTF("qemu: hpet_ram_writel HPET_TN_CMP + 4\n");

                if (!timer_is_periodic(timer) || (timer->config & HPET_TN_SETVAL)) {
                    timer->cmp = (timer->cmp & 0xffffffffULL) | new_val << 32;
                } else {
                    /*
                     * FIXME: Clamp period to reasonable min value?
                     * Clamp period to reasonable max value
                     */
                    new_val &= (timer->config & HPET_TN_32BIT ? ~0u : ~0ull) >> 1;
                    timer->period = (timer->period & 0xffffffffULL) | new_val << 32;
                }

                timer->config &= ~HPET_TN_SETVAL;

                if (hpet_enabled(s)) {
                    hpet_set_timer(timer);
                }

                break;

            case HPET_TN_ROUTE:
                timer->fsb = (timer->fsb & 0xffffffff00000000ULL) | new_val;
                break;

            case HPET_TN_ROUTE + 4:
                timer->fsb = (new_val << 32) | (timer->fsb & 0xffffffff);
                break;

            default:
                DPRINTF("qemu: invalid hpet_ram_writel\n");
                return VMM_EINVALID;
        }

        return VMM_OK;
        ;
    } else {
        switch (index) {
            case HPET_ID:
                return VMM_OK;

            case HPET_CFG:
                val       = hpet_fixup_reg(new_val, old_val, HPET_CFG_WRITE_MASK);
                s->config = (s->config & 0xffffffff00000000ULL) | val;

                if (activating_bit(old_val, new_val, HPET_CFG_ENABLE)) {
                    /* Enable main counter and interrupt generation. */
                    s->hpet_offset = ticks_to_ns(s->hpet_counter) - vmm_timer_timestamp();

                    for (i = 0; i < s->num_timers; i++) {
                        if ((&s->timer[i])->cmp != ~0ULL) {
                            hpet_set_timer(&s->timer[i]);
                        }
                    }
                } else if (deactivating_bit(old_val, new_val, HPET_CFG_ENABLE)) {
                    /* Halt main counter and disable interrupt generation. */
                    s->hpet_counter = hpet_get_ticks(s);

                    for (i = 0; i < s->num_timers; i++) {
                        hpet_del_timer(&s->timer[i]);
                    }
                }

                /* i8254 and RTC output pins are disabled
                 * when HPET is in legacy mode */
                if (activating_bit(old_val, new_val, HPET_CFG_LEGACY)) {
                    vmm_device_emulate_emulate_irq(s->guest, s->pit_enabled, 1);
                    vmm_device_emulate_emulate_irq(s->guest, s->irqs[0], 0);
                    vmm_device_emulate_emulate_irq(s->guest, s->irqs[RTC_ISA_IRQ], 0);
                } else if (deactivating_bit(old_val, new_val, HPET_CFG_LEGACY)) {
                    vmm_device_emulate_emulate_irq(s->guest, s->irqs[0], 0);
                    vmm_device_emulate_emulate_irq(s->guest, s->pit_enabled, 1);
                    vmm_device_emulate_emulate_irq(s->guest, s->irqs[RTC_ISA_IRQ], s->rtc_irq_level);
                }

                break;

            case HPET_CFG + 4:
                DPRINTF("qemu: invalid HPET_CFG+4 write\n");
                break;

            case HPET_STATUS:
                val = new_val & s->isr;

                for (i = 0; i < s->num_timers; i++) {
                    if (val & (1 << i)) {
                        update_irq(&s->timer[i], 0);
                    }
                }

                break;

            case HPET_COUNTER:
                if (hpet_enabled(s)) {
                    DPRINTF("qemu: Writing counter while HPET enabled!\n");
                }

                s->hpet_counter = (s->hpet_counter & 0xffffffff00000000ULL) | value;
                DPRINTF("qemu: HPET counter written. ctr = %#x -> %" PRIx64 "\n", value, s->hpet_counter);
                break;

            case HPET_COUNTER + 4:
                if (hpet_enabled(s)) {
                    DPRINTF("qemu: Writing counter while HPET enabled!\n");
                }

                s->hpet_counter = (s->hpet_counter & 0xffffffffULL) | (((uint64_t)value) << 32);
                DPRINTF("qemu: HPET counter + 4 written. ctr = %#x -> %" PRIx64 "\n", value, s->hpet_counter);
                break;

            default:
                DPRINTF("qemu: invalid hpet_ram_writel\n");
                return VMM_EINVALID;
        }
    }

    return VMM_OK;
}

static void hpet_reset(struct hpet_state *s)
{
    int i;

    for (i = 0; i < s->num_timers; i++) {
        struct hpet_timer *timer = &s->timer[i];

        hpet_del_timer(timer);
        timer->cmp    = ~0ULL;
        timer->config = HPET_TN_PERIODIC_CAP | HPET_TN_SIZE_CAP;

        if (s->flags & (1 << HPET_MSI_SUPPORT)) {
            timer->config |= HPET_TN_FSB_CAP;
        }

        /* advertise availability of ioapic int */
        timer->config |= (uint64_t)s->intcap << 32;
        timer->period    = 0ULL;
        timer->wrap_flag = 0;
    }

    vmm_device_emulate_emulate_irq(s->guest, s->pit_enabled, 1);
    s->hpet_counter  = 0ULL;
    s->hpet_offset   = 0ULL;
    s->config        = 0ULL;

    /* to document that the RTC lowers its output on reset as well */
    s->rtc_irq_level = 0;
}

static int hpet_emulator_reset(vmm_emulate_device_t *edev)
{
    struct hpet_state *s = edev->private;

    hpet_reset(s);

    return VMM_OK;
}

static int hpet_emulator_read8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t *dst)
{
    int      rc;
    uint64_t regval = 0x0;

    rc              = hpet_ram_read(edev->private, offset, &regval);

    if (!rc) {
        *dst = regval & 0xFF;
    }

    return rc;
}

static int hpet_emulator_read16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t *dst)
{
    int      rc;
    uint64_t regval = 0x0;

    rc              = hpet_ram_read(edev->private, offset, &regval);

    if (!rc) {
        *dst = regval & 0xFFFF;
    }

    return rc;
}

static int hpet_emulator_read32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst)
{
    int      rc;
    uint64_t regval = 0x0;

    rc              = hpet_ram_read(edev->private, offset, &regval);

    if (!rc) {
        *dst = regval & 0xFFFFFFFF;
    }

    return rc;
}

static int hpet_emulator_read64(vmm_emulate_device_t *edev, physical_addr_t offset, uint64_t *dst)
{
    return hpet_ram_read(edev->private, offset, dst);
}

static int hpet_emulator_write8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t src)
{
    return hpet_ram_write(edev->private, offset, 0xFFFFFFFFFFFFFF00ULL, src);
}

static int hpet_emulator_write16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t src)
{
    return hpet_ram_write(edev->private, offset, 0xFFFFFFFFFFFF0000ULL, src);
}

static int hpet_emulator_write32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src)
{
    return hpet_ram_write(edev->private, offset, 0xFFFFFFFF00000000ULL, src);
}

static int hpet_emulator_write64(vmm_emulate_device_t *edev, physical_addr_t offset, uint64_t src)
{
    return hpet_ram_write(edev->private, offset, 0x0000000000000000ULL, src);
}

static int hpet_emulator_remove(vmm_emulate_device_t *edev)
{
    struct hpet_state *s = edev->private;

    vmm_free(s);

    return VMM_OK;
}

static int hpet_emulator_probe(struct vmm_guest *guest, vmm_emulate_device_t *edev, const struct vmm_device_tree_nodeid *eid)
{
    int                rc    = VMM_OK, i;
    struct hpet_state *s     = NULL;
    struct hpet_timer *timer = NULL;

    s                        = vmm_zalloc(sizeof(struct hpet_state));

    if (!s) {
        rc = VMM_ENOMEM;
        goto hpet_emulator_probe_done;
    }

    s->guest = guest;

    rc       = vmm_device_tree_read_u32(edev->node, "id", &s->hpet_id);

    if (rc != VMM_OK) {
        vmm_printf("HPET ID not specified in guest device tree.\n");
        goto hpet_emulator_probe_freestate_fail;
    }

    rc = vmm_device_tree_read_u32(edev->node, "num_timers", &s->num_timers);

    if (rc != VMM_OK) {
        vmm_printf("Number of timers not specified in guest device tree.\n");
        goto hpet_emulator_probe_freestate_fail;
    }

    if (s->num_timers < HPET_MIN_TIMERS) {
        s->num_timers = HPET_MIN_TIMERS;
    } else if (s->num_timers > HPET_MAX_TIMERS) {
        s->num_timers = HPET_MAX_TIMERS;
    }

    for (i = 0; i < HPET_MAX_TIMERS; i++) {
        timer = &s->timer[i];
        INIT_TIMER_EVENT(&timer->timer, hpet_timer, timer);
        timer->tn    = i;
        timer->state = s;
    }

    edev->private = s;

    goto hpet_emulator_probe_done;

hpet_emulator_probe_freestate_fail:
    vmm_free(s);

hpet_emulator_probe_done:
    return rc;
}

static struct vmm_device_tree_nodeid hpet_emulator_emuid_table[] = {
    {
     .type       = "hpet",
     .compatible = "hpet",
     },
    {/* end of list */                   },
};

static vmm_emulator_t hpet_emulator = {
    .name        = "hpet",
    .match_table = hpet_emulator_emuid_table,
    .endian      = VMM_DEVICE_EMULATE_LITTLE_ENDIAN,
    .probe       = hpet_emulator_probe,
    .read8       = hpet_emulator_read8,
    .write8      = hpet_emulator_write8,
    .read16      = hpet_emulator_read16,
    .write16     = hpet_emulator_write16,
    .read32      = hpet_emulator_read32,
    .write32     = hpet_emulator_write32,
    .read64      = hpet_emulator_read64,
    .write64     = hpet_emulator_write64,
    .reset       = hpet_emulator_reset,
    .remove      = hpet_emulator_remove,
};

static int __init hpet_emulator_init(void)
{
    return vmm_device_emulate_register_emulator(&hpet_emulator);
}

static void __exit hpet_emulator_exit(void)
{
    vmm_device_emulate_unregister_emulator(&hpet_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
