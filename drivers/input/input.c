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
 * @file input.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Input device framework source
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/input/input.c
 *
 * Copyright (c) 1999-2002 Vojtech Pavlik
 *
 * The original code is licensed under the GPL.
 */

#include <drv/input.h>
#include <drv/input/mt.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_macros.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_NAME      input_core
#define MODULE_DESC      "Input Device Framework"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY INPUT_IPRIORITY
#define MODULE_INIT      input_init
#define MODULE_EXIT      input_exit

typedef struct input_ctrl {
    vmm_spinlock_t dev_list_lock;
    double_list_t  dev_list;
    vmm_spinlock_t hnd_list_lock;
    double_list_t  hnd_list;
    vmm_spinlock_t hnd_conn_lock[EV_CNT];
    double_list_t  hnd_conn[EV_CNT];
    uint32_t       hnd_conn_count[EV_CNT];
} input_ctrl_t;

static struct input_ctrl ictrl;

static inline int is_event_supported(uint32_t code, uint64_t *bm, uint32_t max)
{
    return code <= max && test_bit(code, bm);
}

static int input_defuzz_abs_event(int value, int old_val, int fuzz)
{
    if (fuzz) {
        if (value > old_val - fuzz / 2 && value < old_val + fuzz / 2) {
            return old_val;
        }

        if (value > old_val - fuzz && value < old_val + fuzz) {
            return (old_val * 3 + value) / 4;
        }

        if (value > old_val - fuzz * 2 && value < old_val + fuzz * 2) {
            return (old_val + value) / 2;
        }
    }

    return value;
}

#define INPUT_DO_TOGGLE(dev, type, bits, on)                                                                                                         \
    do {                                                                                                                                             \
        int  i;                                                                                                                                      \
        bool active;                                                                                                                                 \
                                                                                                                                                     \
        if (!test_bit(EV_##type, dev->evbit))                                                                                                        \
            break;                                                                                                                                   \
                                                                                                                                                     \
        for (i = 0; i < type##_MAX; i++) {                                                                                                           \
            if (!test_bit(i, dev->bits##bit))                                                                                                        \
                continue;                                                                                                                            \
                                                                                                                                                     \
            active = test_bit(i, dev->bits);                                                                                                         \
            if (!active && !on)                                                                                                                      \
                continue;                                                                                                                            \
                                                                                                                                                     \
            dev->event(dev, EV_##type, i, on ? active : 0);                                                                                          \
        }                                                                                                                                            \
    } while (0)

static void input_device_toggle(input_device_t *dev, bool activate)
{
    if (!dev->event) {
        return;
    }

    INPUT_DO_TOGGLE(dev, LED, led, activate);
    INPUT_DO_TOGGLE(dev, SND, snd, activate);

    if (activate && test_bit(EV_REP, dev->evbit)) {
        dev->event(dev, EV_REP, REP_PERIOD, dev->rep[REP_PERIOD]);
        dev->event(dev, EV_REP, REP_DELAY, dev->rep[REP_DELAY]);
    }
}

/* Pass event to all relevant input handlers. This function is called with
 * dev->event_lock held and interrupts disabled.
 */
static void input_pass_event(input_device_t *dev, uint32_t type, uint32_t code, int value)
{
    irq_flags_t           flags;
    double_list_t        *l;
    struct input_handler *handler;

    vmm_spin_lock_irq_save(&ictrl.hnd_conn_lock[type], flags);

    list_for_each(l, &ictrl.hnd_conn[type])
    {
        handler = list_entry(l, struct input_handler, conn_head[type]);
        handler->event(handler, dev, type, code, value);
    }

    vmm_spin_unlock_irq_restore(&ictrl.hnd_conn_lock[type], flags);
}

/*
 * Generate software autorepeat event. Note that we take
 * dev->event_lock here to avoid racing with input_event
 * which may cause keys get "stuck".
 */
static void input_repeat_key(vmm_timer_event_t *ev)
{
    uint64_t        duration;
    irq_flags_t     flags;
    input_device_t *dev = (void *)ev->private;

    vmm_spin_lock_irq_save(&dev->event_lock, flags);

    if (test_bit(dev->repeat_key, dev->key) && is_event_supported(dev->repeat_key, dev->keybit, KEY_MAX)) {

        input_pass_event(dev, EV_KEY, dev->repeat_key, 2);

        if (dev->sync) {
            /*
             * Only send SYN_REPORT if we are not in a middle
             * of driver parsing a new hardware packet.
             * Otherwise assume that the driver will send
             * SYN_REPORT once it's done.
             */
            input_pass_event(dev, EV_SYN, SYN_REPORT, 1);
        }

        if (dev->rep[REP_PERIOD]) {
            duration = dev->rep[REP_PERIOD];
            duration = duration * 1000000;
            vmm_timer_event_start(&dev->repeat_ev, duration);
        }
    }

    vmm_spin_unlock_irq_restore(&dev->event_lock, flags);
}

static void input_start_autorepeat(input_device_t *dev, int code)
{
    uint32_t duration;

    if (test_bit(EV_REP, dev->evbit) && dev->rep[REP_PERIOD] && dev->rep[REP_DELAY] && dev->repeat_ev.private) {
        dev->repeat_key = code;
        duration        = dev->rep[REP_DELAY];
        duration        = duration * 1000000;
        vmm_timer_event_start(&dev->repeat_ev, duration);
    }
}

static void input_stop_autorepeat(input_device_t *dev)
{
    vmm_timer_event_stop(&dev->repeat_ev);
}

#define INPUT_IGNORE_EVENT     0
#define INPUT_PASS_TO_HANDLERS 1
#define INPUT_PASS_TO_DEVICE   2
#define INPUT_PASS_TO_ALL      (INPUT_PASS_TO_HANDLERS | INPUT_PASS_TO_DEVICE)

static int input_handle_abs_event(input_device_t *dev, uint32_t code, int *pval)
{
    struct input_mt *mt = dev->mt;
    bool             is_mt_event;
    int             *pold;

    if (code == ABS_MT_SLOT) {
        /*
         * "Stage" the event; we'll flush it later, when we
         * get actual touch data.
         */
        if (mt && *pval >= 0 && *pval < mt->num_slots) {
            mt->slot = *pval;
        }

        return INPUT_IGNORE_EVENT;
    }

    is_mt_event = code >= ABS_MT_FIRST && code <= ABS_MT_LAST;

    if (!is_mt_event) {
        pold = &dev->absinfo[code].value;
    } else if (dev->mt) {
        pold = &mt->slots[mt->slot].abs[code - ABS_MT_FIRST];
    } else {
        /*
         * Bypass filtering for multi-touch events when
         * not employing slots.
         */
        pold = NULL;
    }

    if (pold) {
        *pval = input_defuzz_abs_event(*pval, *pold, dev->absinfo[code].fuzz);

        if (*pold == *pval) {
            return INPUT_IGNORE_EVENT;
        }

        *pold = *pval;
    }

    /* Flush pending "slot" event */
    if (is_mt_event && mt && mt->slot != input_abs_get_val(dev, ABS_MT_SLOT)) {
        input_abs_set_val(dev, ABS_MT_SLOT, mt->slot);
        input_pass_event(dev, EV_ABS, ABS_MT_SLOT, mt->slot);
    }

    return INPUT_PASS_TO_HANDLERS;
}

static void input_handle_event(input_device_t *dev, uint32_t type, uint32_t code, int value)
{
    int disposition = INPUT_IGNORE_EVENT;

    switch (type) {

        case EV_SYN:
            switch (code) {
                case SYN_CONFIG:
                    disposition = INPUT_PASS_TO_ALL;
                    break;

                case SYN_REPORT:
                    if (!dev->sync) {
                        dev->sync   = TRUE;
                        disposition = INPUT_PASS_TO_HANDLERS;
                    }

                    break;

                case SYN_MT_REPORT:
                    dev->sync   = FALSE;
                    disposition = INPUT_PASS_TO_HANDLERS;
                    break;
            }

            break;

        case EV_KEY:
            if (is_event_supported(code, dev->keybit, KEY_MAX) && !!test_bit(code, dev->key) != value) {

                if (value != 2) {
                    __change_bit(code, dev->key);

                    if (value) {
                        input_start_autorepeat(dev, code);
                    } else {
                        input_stop_autorepeat(dev);
                    }
                }

                disposition = INPUT_PASS_TO_HANDLERS;
            }

            break;

        case EV_SW:
            if (is_event_supported(code, dev->swbit, SW_MAX) && !!test_bit(code, dev->sw) != value) {

                disposition = INPUT_PASS_TO_HANDLERS;
            }

            break;

        case EV_ABS:
            if (is_event_supported(code, dev->absbit, ABS_MAX)) {
                disposition = input_handle_abs_event(dev, code, &value);
            }

            break;

        case EV_REL:
            if (is_event_supported(code, dev->relbit, REL_MAX) && value) {
                disposition = INPUT_PASS_TO_HANDLERS;
            }

            break;

        case EV_MSC:
            if (is_event_supported(code, dev->mscbit, MSC_MAX)) {
                disposition = INPUT_PASS_TO_ALL;
            }

            break;

        case EV_LED:
            if (is_event_supported(code, dev->ledbit, LED_MAX) && !!test_bit(code, dev->led) != value) {

                __change_bit(code, dev->led);
                disposition = INPUT_PASS_TO_ALL;
            }

            break;

        case EV_SND:
            if (is_event_supported(code, dev->sndbit, SND_MAX)) {

                if (!!test_bit(code, dev->snd) != !!value) {
                    __change_bit(code, dev->snd);
                }

                disposition = INPUT_PASS_TO_ALL;
            }

            break;

        case EV_REP:
            if (code <= REP_MAX && value >= 0 && dev->rep[code] != value) {
                dev->rep[code] = value;
                disposition    = INPUT_PASS_TO_ALL;
            }

            break;

        case EV_FF:
            if (value >= 0) {
                disposition = INPUT_PASS_TO_ALL;
            }

            break;

        case EV_PWR:
            disposition = INPUT_PASS_TO_ALL;
            break;
    };

    if (disposition != INPUT_IGNORE_EVENT && type != EV_SYN) {
        dev->sync = FALSE;
    }

    if ((disposition & INPUT_PASS_TO_DEVICE) && dev->event) {
        dev->event(dev, type, code, value);
    }

    if (disposition & INPUT_PASS_TO_HANDLERS) {
        input_pass_event(dev, type, code, value);
    }
}

void input_event(input_device_t *dev, uint32_t type, uint32_t code, int value)
{
    irq_flags_t flags;

    if (is_event_supported(type, dev->evbit, EV_MAX)) {
        vmm_spin_lock_irq_save(&dev->event_lock, flags);
        input_handle_event(dev, type, code, value);
        vmm_spin_unlock_irq_restore(&dev->event_lock, flags);
    }
}

VMM_EXPORT_SYMBOL(input_event);

void input_set_capability(input_device_t *dev, uint32_t type, uint32_t code)
{
    switch (type) {
        case EV_KEY:
            __set_bit(code, dev->keybit);
            break;

        case EV_REL:
            __set_bit(code, dev->relbit);
            break;

        case EV_ABS:
            __set_bit(code, dev->absbit);
            break;

        case EV_MSC:
            __set_bit(code, dev->mscbit);
            break;

        case EV_SW:
            __set_bit(code, dev->swbit);
            break;

        case EV_LED:
            __set_bit(code, dev->ledbit);
            break;

        case EV_SND:
            __set_bit(code, dev->sndbit);
            break;

        case EV_FF:
            __set_bit(code, dev->ffbit);
            break;

        case EV_PWR:
            /* do nothing */
            break;

        default:
            vmm_panic("%s: unknown type %u (code %u)\n", __func__, type, code);
            return;
    }

    __set_bit(type, dev->evbit);
}

VMM_EXPORT_SYMBOL(input_set_capability);

int input_scancode_to_scalar(const struct input_keymap_entry *ke, uint32_t *scancode)
{
    switch (ke->len) {
        case 1:
            *scancode = (uint32_t)(*((uint8_t *)ke->scancode));
            break;

        case 2:
            *scancode = (uint32_t)(*((uint16_t *)ke->scancode));
            break;

        case 4:
            *scancode = (uint32_t)(*((uint32_t *)ke->scancode));
            break;

        default:
            return VMM_EINVALID;
    }

    return VMM_OK;
}

VMM_EXPORT_SYMBOL(input_scancode_to_scalar);

static uint32_t input_fetch_keycode(input_device_t *dev, uint32_t index)
{
    switch (dev->keycodesize) {
        case 1:
            return ((uint8_t *)dev->keycode)[index];

        case 2:
            return ((uint16_t *)dev->keycode)[index];

        default:
            return ((uint32_t *)dev->keycode)[index];
    }
}

static int input_default_getkeycode(input_device_t *dev, struct input_keymap_entry *ke)
{
    uint32_t index;
    int      error;

    if (!dev->keycodesize) {
        return VMM_EINVALID;
    }

    if (ke->flags & INPUT_KEYMAP_BY_INDEX) {
        index = ke->index;
    } else {
        error = input_scancode_to_scalar(ke, &index);

        if (error) {
            return error;
        }
    }

    if (index >= dev->keycodemax) {
        return VMM_EINVALID;
    }

    ke->keycode = input_fetch_keycode(dev, index);
    ke->index   = index;
    ke->len     = sizeof(index);
    memcpy(ke->scancode, &index, sizeof(index));

    return 0;
}

static int input_default_setkeycode(input_device_t *dev, const struct input_keymap_entry *ke, uint32_t *old_keycode)
{
    uint32_t index;
    int      error;
    int      i;

    if (!dev->keycodesize) {
        return VMM_EINVALID;
    }

    if (ke->flags & INPUT_KEYMAP_BY_INDEX) {
        index = ke->index;
    } else {
        error = input_scancode_to_scalar(ke, &index);

        if (error) {
            return error;
        }
    }

    if (index >= dev->keycodemax) {
        return VMM_EINVALID;
    }

    if (dev->keycodesize < sizeof(ke->keycode) && (ke->keycode >> (dev->keycodesize * 8))) {
        return VMM_EINVALID;
    }

    switch (dev->keycodesize) {
        case 1:
        {
            uint8_t *k   = (uint8_t *)dev->keycode;
            *old_keycode = k[index];
            k[index]     = ke->keycode;
            break;
        }

        case 2:
        {
            uint16_t *k  = (uint16_t *)dev->keycode;
            *old_keycode = k[index];
            k[index]     = ke->keycode;
            break;
        }

        default:
        {
            uint32_t *k  = (uint32_t *)dev->keycode;
            *old_keycode = k[index];
            k[index]     = ke->keycode;
            break;
        }
    }

    __clear_bit(*old_keycode, dev->keybit);
    __set_bit(ke->keycode, dev->keybit);

    for (i = 0; i < dev->keycodemax; i++) {
        if (input_fetch_keycode(dev, i) == *old_keycode) {
            __set_bit(*old_keycode, dev->keybit);
            break; /* Setting the bit twice is useless, so break */
        }
    }

    return 0;
}

void input_alloc_absinfo(input_device_t *dev)
{
    if (!dev->absinfo) {
        dev->absinfo = vmm_malloc(ABS_CNT * sizeof(struct input_absinfo));
    }

    BUG_ON(!dev->absinfo);
}

VMM_EXPORT_SYMBOL(input_alloc_absinfo);

void input_set_abs_params(input_device_t *dev, uint32_t axis, int min, int max, int fuzz, int flat)
{
    struct input_absinfo *absinfo;

    input_alloc_absinfo(dev);

    if (!dev->absinfo) {
        return;
    }

    absinfo          = &dev->absinfo[axis];
    absinfo->minimum = min;
    absinfo->maximum = max;
    absinfo->fuzz    = fuzz;
    absinfo->flat    = flat;

    dev->absbit[BIT_WORD(axis)] |= BIT_MASK(axis);
}

VMM_EXPORT_SYMBOL(input_set_abs_params);

int input_get_keycode(input_device_t *dev, struct input_keymap_entry *ke)
{
    irq_flags_t flags;
    int         rc;

    vmm_spin_lock_irq_save(&dev->event_lock, flags);
    rc = dev->getkeycode(dev, ke);
    vmm_spin_unlock_irq_restore(&dev->event_lock, flags);

    return rc;
}

VMM_EXPORT_SYMBOL(input_get_keycode);

int input_set_keycode(input_device_t *dev, const struct input_keymap_entry *ke)
{
    int         rc;
    irq_flags_t flags;
    uint32_t    old_keycode;

    if (ke->keycode > KEY_MAX) {
        return VMM_EINVALID;
    }

    vmm_spin_lock_irq_save(&dev->event_lock, flags);

    rc = dev->setkeycode(dev, ke, &old_keycode);

    if (rc) {
        goto out;
    }

    /* Make sure KEY_RESERVED did not get enabled. */
    __clear_bit(KEY_RESERVED, dev->keybit);

    /*
     * Simulate keyup event if keycode is not present
     * in the keymap anymore
     */
    if (test_bit(EV_KEY, dev->evbit) && !is_event_supported(old_keycode, dev->keybit, KEY_MAX) && __test_and_clear_bit(old_keycode, dev->key)) {

        input_pass_event(dev, EV_KEY, old_keycode, 0);

        if (dev->sync) {
            input_pass_event(dev, EV_SYN, SYN_REPORT, 1);
        }
    }

out:
    vmm_spin_unlock_irq_restore(&dev->event_lock, flags);

    return rc;
}

VMM_EXPORT_SYMBOL(input_set_keycode);

static vmm_class_t input_class = {
    .name = INPUT_DEVICE_CLASS_NAME,
};

input_device_t *input_allocate_device(void)
{
    input_device_t *dev;

    dev = vmm_zalloc(sizeof(input_device_t));

    if (!dev) {
        return NULL;
    }

    INIT_LIST_HEAD(&dev->head);
    INIT_SPIN_LOCK(&dev->event_lock);
    INIT_SPIN_LOCK(&dev->ops_lock);
    vmm_device_driver_initialize_device(&dev->dev);
    dev->dev.class = &input_class;

    return dev;
}

VMM_EXPORT_SYMBOL(input_allocate_device);

void input_free_device(input_device_t *dev)
{
    if (!dev) {
        vmm_free(dev);
    }
}

VMM_EXPORT_SYMBOL(input_free_device);

static uint32_t input_estimate_events_per_packet(input_device_t *dev)
{
    int      mt_slots;
    int      i;
    uint32_t events;

    if (dev->mt) {
        mt_slots = dev->mt->num_slots;
    } else if (test_bit(ABS_MT_TRACKING_ID, dev->absbit)) {
        mt_slots = dev->absinfo[ABS_MT_TRACKING_ID].maximum - dev->absinfo[ABS_MT_TRACKING_ID].minimum + 1, mt_slots = clamp(mt_slots, 2, 32);
    } else if (test_bit(ABS_MT_POSITION_X, dev->absbit)) {
        mt_slots = 2;
    } else {
        mt_slots = 0;
    }

    events = mt_slots + 1; /* count SYN_MT_REPORT and SYN_REPORT */

    for (i = 0; i < ABS_CNT; i++) {
        if (test_bit(i, dev->absbit)) {
            if (input_is_mt_axis(i)) {
                events += mt_slots;
            } else {
                events++;
            }
        }
    }

    for (i = 0; i < REL_CNT; i++) {
        if (test_bit(i, dev->relbit)) {
            events++;
        }
    }

    /* Make room for KEY and MSC events */
    events += 7;

    return events;
}

#define INPUT_CLEANSE_BITMASK(dev, type, bits)                                                                                                       \
    do {                                                                                                                                             \
        if (!test_bit(EV_##type, dev->evbit))                                                                                                        \
            memset(dev->bits##bit, 0, sizeof(dev->bits##bit));                                                                                       \
    } while (0)

static void input_cleanse_bitmasks(input_device_t *dev)
{
    INPUT_CLEANSE_BITMASK(dev, KEY, key);
    INPUT_CLEANSE_BITMASK(dev, REL, rel);
    INPUT_CLEANSE_BITMASK(dev, ABS, abs);
    INPUT_CLEANSE_BITMASK(dev, MSC, msc);
    INPUT_CLEANSE_BITMASK(dev, LED, led);
    INPUT_CLEANSE_BITMASK(dev, SND, snd);
    INPUT_CLEANSE_BITMASK(dev, FF, ff);
    INPUT_CLEANSE_BITMASK(dev, SW, sw);
}

int input_register_device(input_device_t *dev)
{
    int         i, rc;
    irq_flags_t flags, flags1;

    if (!(dev && dev->phys && dev->name)) {
        return VMM_EFAIL;
    }

    if (strlcpy(dev->dev.name, dev->phys, sizeof(dev->dev.name)) >= sizeof(dev->dev.name)) {
        return VMM_EOVERFLOW;
    }

    vmm_device_driver_set_data(&dev->dev, dev);

    dev->dev.class = &input_class;
    rc             = vmm_device_driver_register_device(&dev->dev);

    if (rc) {
        return rc;
    }

    /* Every input device generates EV_SYN/SYN_REPORT events. */
    __set_bit(EV_SYN, dev->evbit);

    /* KEY_RESERVED is not supposed to be transmitted to userspace. */
    __clear_bit(KEY_RESERVED, dev->keybit);

    /* Make sure that bitmasks not mentioned in dev->evbit are clean. */
    input_cleanse_bitmasks(dev);

    if (!dev->hint_events_per_packet) {
        dev->hint_events_per_packet = input_estimate_events_per_packet(dev);
    }

    /* If delay and period are pre-set by the driver, then autorepeating
     * is handled by the driver itself and we don't do it in input.c.
     */
    INIT_TIMER_EVENT(&dev->repeat_ev, input_repeat_key, dev);

    if (!dev->rep[REP_DELAY] && !dev->rep[REP_PERIOD]) {
        dev->rep[REP_DELAY]  = 250;
        dev->rep[REP_PERIOD] = 33;
    }

    if (!dev->getkeycode) {
        dev->getkeycode = input_default_getkeycode;
    }

    if (!dev->setkeycode) {
        dev->setkeycode = input_default_setkeycode;
    }

    vmm_spin_lock_irq_save(&dev->ops_lock, flags);
    dev->users = 0;

    for (i = 0; i < EV_CNT; i++) {
        if (!test_bit(i, &dev->evbit[0])) {
            continue;
        }

        vmm_spin_lock_irq_save(&ictrl.hnd_conn_lock[i], flags1);
        dev->users += ictrl.hnd_conn_count[i];
        vmm_spin_unlock_irq_restore(&ictrl.hnd_conn_lock[i], flags1);
    }

    if (dev->users && dev->open) {
        rc = dev->open(dev);
    }

    vmm_spin_unlock_irq_restore(&dev->ops_lock, flags);

    vmm_spin_lock_irq_save(&ictrl.dev_list_lock, flags);
    list_add_tail(&dev->head, &ictrl.dev_list);
    vmm_spin_unlock_irq_restore(&ictrl.dev_list_lock, flags);

    return rc;
}

VMM_EXPORT_SYMBOL(input_register_device);

int input_unregister_device(input_device_t *dev)
{
    irq_flags_t flags;

    if (!dev) {
        return VMM_EFAIL;
    }

    vmm_spin_lock_irq_save(&ictrl.dev_list_lock, flags);
    list_del(&dev->head);
    vmm_spin_unlock_irq_restore(&ictrl.dev_list_lock, flags);

    vmm_timer_event_stop(&dev->repeat_ev);

    vmm_spin_lock_irq_save(&dev->ops_lock, flags);

    if (dev->users && dev->close) {
        dev->users = 0;
        dev->close(dev);
    }

    vmm_spin_unlock_irq_restore(&dev->ops_lock, flags);

    return vmm_device_driver_unregister_device(&dev->dev);
}

VMM_EXPORT_SYMBOL(input_unregister_device);

/*
 * Simulate keyup events for all keys that are marked as pressed.
 * The function must be called with dev->event_lock held.
 */
static void input_device_release_keys(input_device_t *dev)
{
    int code;

    if (is_event_supported(EV_KEY, dev->evbit, EV_MAX)) {
        for (code = 0; code <= KEY_MAX; code++) {
            if (is_event_supported(code, dev->keybit, KEY_MAX) && __test_and_clear_bit(code, dev->key)) {
                input_pass_event(dev, EV_KEY, code, 0);
            }
        }

        input_pass_event(dev, EV_SYN, SYN_REPORT, 1);
    }
}

void input_reset_device(input_device_t *dev)
{
    irq_flags_t flags, flags1;

    if (!dev) {
        return;
    }

    vmm_spin_lock_irq_save(&dev->ops_lock, flags);

    if (dev->users) {
        input_device_toggle(dev, TRUE);

        /* Keys that have been pressed at suspend time are unlikely
         * to be still pressed when we resume.
         */
        vmm_spin_lock_irq_save(&dev->event_lock, flags1);
        input_device_release_keys(dev);
        vmm_spin_unlock_irq_restore(&dev->event_lock, flags1);
    }

    vmm_spin_unlock_irq_restore(&dev->ops_lock, flags);
}

VMM_EXPORT_SYMBOL(input_reset_device);

int input_flush_device(input_device_t *dev)
{
    int         rc = VMM_OK;
    irq_flags_t flags;

    if (!dev) {
        return VMM_EFAIL;
    }

    if (dev->flush) {
        vmm_spin_lock_irq_save(&dev->ops_lock, flags);
        rc = dev->flush(dev);
        vmm_spin_unlock_irq_restore(&dev->ops_lock, flags);
    }

    return rc;
}

VMM_EXPORT_SYMBOL(input_flush_device);

input_device_t *input_find_device(const char *phys)
{
    vmm_device_t *dev;

    dev = vmm_device_driver_class_find_device_by_name(&input_class, phys);

    if (!dev) {
        return NULL;
    }

    return vmm_device_driver_get_data(dev);
}

VMM_EXPORT_SYMBOL(input_find_device);

struct input_iterate_device_priv {
    void *data;
    int (*fn)(input_device_t *dev, void *data);
};

static int __input_iterate_device(vmm_device_t *dev, void *data)
{
    struct input_iterate_device_priv *p    = data;
    input_device_t                   *idev = vmm_device_driver_get_data(dev);

    return p->fn(idev, p->data);
}

int input_iterate_device(input_device_t *start, void *data, int (*fn)(input_device_t *dev, void *data))
{
    vmm_device_t                    *st = (start) ? &start->dev : NULL;
    struct input_iterate_device_priv p;

    if (!fn) {
        return VMM_EINVALID;
    }

    p.data = data;
    p.fn   = fn;

    return vmm_device_driver_class_device_iterate(&input_class, st, &p, __input_iterate_device);
}

VMM_EXPORT_SYMBOL(input_iterate_device);

uint32_t input_count_device(void)
{
    return vmm_device_driver_class_device_count(&input_class);
}

VMM_EXPORT_SYMBOL(input_count_device);

int input_register_handler(struct input_handler *ihnd)
{
    int                   i;
    bool                  found;
    irq_flags_t           flags;
    double_list_t        *l;
    struct input_handler *ih;

    if (!(ihnd && ihnd->name && ihnd->event)) {
        return VMM_EFAIL;
    }

    ih    = NULL;
    found = FALSE;

    vmm_spin_lock_irq_save(&ictrl.hnd_list_lock, flags);

    list_for_each(l, &ictrl.hnd_list)
    {
        ih = list_entry(l, struct input_handler, head);

        if (strcmp(ih->name, ihnd->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_spin_unlock_irq_restore(&ictrl.hnd_list_lock, flags);
        return VMM_EFAIL;
    }

    INIT_LIST_HEAD(&ihnd->head);
    ihnd->connected = FALSE;

    for (i = 0; i < EV_CNT; i++) {
        INIT_LIST_HEAD(&ihnd->conn_head[i]);
    }

    list_add_tail(&ihnd->head, &ictrl.hnd_list);

    vmm_spin_unlock_irq_restore(&ictrl.hnd_list_lock, flags);

    return VMM_OK;
}

VMM_EXPORT_SYMBOL(input_register_handler);

int input_unregister_handler(struct input_handler *ihnd)
{
    bool                  found;
    irq_flags_t           flags;
    double_list_t        *l;
    struct input_handler *ih;

    if (!ihnd) {
        return VMM_EFAIL;
    }

    vmm_spin_lock_irq_save(&ictrl.hnd_list_lock, flags);

    if (list_empty(&ictrl.hnd_list)) {
        vmm_spin_unlock_irq_restore(&ictrl.hnd_list_lock, flags);
        return VMM_EFAIL;
    }

    ih    = NULL;
    found = FALSE;
    list_for_each(l, &ictrl.hnd_list)
    {
        ih = list_entry(l, struct input_handler, head);

        if (strcmp(ih->name, ihnd->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_spin_unlock_irq_restore(&ictrl.hnd_list_lock, flags);
        return VMM_ENOTAVAIL;
    }

    list_del(&ih->head);

    vmm_spin_unlock_irq_restore(&ictrl.hnd_list_lock, flags);

    return VMM_OK;
}

VMM_EXPORT_SYMBOL(input_unregister_handler);

int input_connect_handler(struct input_handler *ihnd)
{
    int             i, rc;
    irq_flags_t     flags, flags1;
    double_list_t  *l;
    input_device_t *dev;

    if (!ihnd || ihnd->connected) {
        return VMM_EFAIL;
    }

    for (i = 0; i < EV_CNT; i++) {
        if (!test_bit(i, &ihnd->evbit[0])) {
            continue;
        }

        vmm_spin_lock_irq_save(&ictrl.hnd_conn_lock[i], flags);
        INIT_LIST_HEAD(&ihnd->conn_head[i]);
        list_add_tail(&ihnd->conn_head[i], &ictrl.hnd_conn[i]);
        ictrl.hnd_conn_count[i]++;
        vmm_spin_unlock_irq_restore(&ictrl.hnd_conn_lock[i], flags);

        vmm_spin_lock_irq_save(&ictrl.dev_list_lock, flags);
        list_for_each(l, &ictrl.dev_list)
        {
            dev = list_entry(l, input_device_t, head);

            if (!test_bit(i, &dev->evbit[0])) {
                continue;
            }

            vmm_spin_lock_irq_save(&dev->ops_lock, flags1);

            if (!dev->users && dev->open) {
                rc = dev->open(dev);

                if (rc) {
                    vmm_printf("%s: failed to open %s", __func__, dev->phys);
                }
            }

            dev->users++;
            vmm_spin_unlock_irq_restore(&dev->ops_lock, flags1);
        }
        vmm_spin_unlock_irq_restore(&ictrl.dev_list_lock, flags);
    }

    ihnd->connected = TRUE;

    return VMM_OK;
}

VMM_EXPORT_SYMBOL(input_connect_handler);

int input_disconnect_handler(struct input_handler *ihnd)
{
    int             i;
    irq_flags_t     flags, flags1;
    double_list_t  *l;
    input_device_t *dev;

    if (!ihnd || !ihnd->connected) {
        return VMM_EFAIL;
    }

    for (i = 0; i < EV_CNT; i++) {
        if (!test_bit(i, &ihnd->evbit[0])) {
            continue;
        }

        vmm_spin_lock_irq_save(&ictrl.hnd_conn_lock[i], flags);
        list_del(&ihnd->conn_head[i]);

        if (ictrl.hnd_conn_count[i]) {
            ictrl.hnd_conn_count[i]--;
        }

        vmm_spin_unlock_irq_restore(&ictrl.hnd_conn_lock[i], flags);

        vmm_spin_lock_irq_save(&ictrl.dev_list_lock, flags);
        list_for_each(l, &ictrl.dev_list)
        {
            dev = list_entry(l, input_device_t, head);

            if (!test_bit(i, &dev->evbit[0])) {
                continue;
            }

            vmm_spin_lock_irq_save(&dev->ops_lock, flags1);

            if ((dev->users == 1) && dev->close) {
                dev->close(dev);
            }

            if (dev->users) {
                dev->users--;
            }

            vmm_spin_unlock_irq_restore(&dev->ops_lock, flags1);
        }
        vmm_spin_unlock_irq_restore(&ictrl.dev_list_lock, flags);
    }

    ihnd->connected = FALSE;

    return VMM_OK;
}

VMM_EXPORT_SYMBOL(input_disconnect_handler);

struct input_handler *input_find_handler(const char *name)
{
    bool                  found;
    irq_flags_t           flags;
    double_list_t        *l;
    struct input_handler *ihnd;

    if (!name) {
        return NULL;
    }

    found = FALSE;
    ihnd  = NULL;

    vmm_spin_lock_irq_save(&ictrl.hnd_list_lock, flags);

    list_for_each(l, &ictrl.hnd_list)
    {
        ihnd = list_entry(l, struct input_handler, head);

        if (strcmp(ihnd->name, name) == 0) {
            found = TRUE;
            break;
        }
    }

    vmm_spin_unlock_irq_restore(&ictrl.hnd_list_lock, flags);

    if (!found) {
        return NULL;
    }

    return ihnd;
}

VMM_EXPORT_SYMBOL(input_find_handler);

struct input_handler *input_get_handler(int index)
{
    bool                  found;
    irq_flags_t           flags;
    double_list_t        *l;
    struct input_handler *ret;

    if (index < 0) {
        return NULL;
    }

    ret   = NULL;
    found = FALSE;

    vmm_spin_lock_irq_save(&ictrl.hnd_list_lock, flags);

    list_for_each(l, &ictrl.hnd_list)
    {
        ret = list_entry(l, struct input_handler, head);

        if (!index) {
            found = TRUE;
            break;
        }

        index--;
    }

    vmm_spin_unlock_irq_restore(&ictrl.hnd_list_lock, flags);

    if (!found) {
        return NULL;
    }

    return ret;
}

VMM_EXPORT_SYMBOL(input_get_handler);

uint32_t input_count_handler(void)
{
    uint32_t       retval = 0;
    irq_flags_t    flags;
    double_list_t *l;

    vmm_spin_lock_irq_save(&ictrl.hnd_list_lock, flags);

    list_for_each(l, &ictrl.hnd_list)
    {
        retval++;
    }

    vmm_spin_unlock_irq_restore(&ictrl.hnd_list_lock, flags);

    return retval;
}

VMM_EXPORT_SYMBOL(input_count_handler);

static int __init input_init(void)
{
    int i;

    memset(&ictrl, 0, sizeof(input_ctrl_t));

    INIT_SPIN_LOCK(&ictrl.dev_list_lock);
    INIT_LIST_HEAD(&ictrl.dev_list);
    INIT_SPIN_LOCK(&ictrl.hnd_list_lock);
    INIT_LIST_HEAD(&ictrl.hnd_list);

    for (i = 0; i < EV_CNT; i++) {
        INIT_SPIN_LOCK(&ictrl.hnd_conn_lock[i]);
        INIT_LIST_HEAD(&ictrl.hnd_conn[i]);
        ictrl.hnd_conn_count[i] = 0;
    }

    return vmm_device_driver_register_class(&input_class);
}

static void input_exit(void)
{
    vmm_device_driver_unregister_class(&input_class);
}

VMM_DECLARE_MODULE2(MODULE_NAME, MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
