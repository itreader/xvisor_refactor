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
 * @file vmm_virtual_input.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for virtual input subsystem
 */

#ifndef __VMM_VINPUT_H_
#define __VMM_VINPUT_H_

#include <libs/list.h>
#include <vmm_limits.h>
#include <vmm_notifier.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#define VMM_VINPUT_IPRIORITY              0

/* Notifier event when virtual keyboard is created */
#define VMM_VINPUT_EVENT_CREATE_KEYBOARD  0x01
/* Notifier event when virtual keyboard is destroyed */
#define VMM_VINPUT_EVENT_DESTROY_KEYBOARD 0x02
/* Notifier event when virtual mouse is created */
#define VMM_VINPUT_EVENT_CREATE_MOUSE     0x03
/* Notifier event when virtual mouse is destroyed */
#define VMM_VINPUT_EVENT_DESTROY_MOUSE    0x04

/** Representation of virtual input notifier event */
struct vmm_virtual_input_event {
    void *data;
};

/** Register a notifier client to receive virtual input events */
int vmm_virtual_input_register_client(vmm_notifier_block_t *nb);

/** Unregister a notifier client to not receive virtual input events */
int vmm_virtual_input_unregister_client(vmm_notifier_block_t *nb);

/* Keyboard LED bits */
#define VMM_SCROLL_LOCK_LED (1 << 0)
#define VMM_NUM_LOCK_LED    (1 << 1)
#define VMM_CAPS_LOCK_LED   (1 << 2)

struct vmm_vkeyboard;

/** Representation of a virtual keyboard */
struct vmm_vkeyboard_led_handler {
    double_list_t head;
    void (*led_change)(struct vmm_vkeyboard *vkbd, int ledstate, void *private);
    void *private;
};

/** Representation of a virtual keyboard */
struct vmm_vkeyboard {
    double_list_t  head;
    char           name[VMM_FIELD_NAME_SIZE];
    vmm_spinlock_t ledstate_lock;
    int            ledstate;
    double_list_t  led_handler_list;
    void (*kbd_event)(struct vmm_vkeyboard *vkbd, int vkeycode, int vkey);
    void *private;
};

/** Create a virtual keyboard */
struct vmm_vkeyboard *vmm_vkeyboard_create(const char *name, void (*kbd_event)(struct vmm_vkeyboard *, int, int), void *private);

/** Destroy a virtual keyboard */
int vmm_vkeyboard_destroy(struct vmm_vkeyboard *vkbd);

/** Retrive private context of virtual keyboard */
static inline void *vmm_vkeyboard_private(struct vmm_vkeyboard *vkbd)
{
    return (vkbd) ? vkbd->private : NULL;
}

/**
 * Trigger virtual keyboard event
 * @param vkbd virtual keyboad instance
 * @param vkeycode virtual keycode (Linux-style key code with additional bits)
 * @param vkey virtual key number (Sequenial Xvisor specific key number)
 * @return VMM_OK (on success) and VMM_Exxx (on failure)
 */
int vmm_vkeyboard_event(struct vmm_vkeyboard *vkbd, int vkeycode, int vkey);

/** Add led handler to a virtual keyboard */
int vmm_vkeyboard_add_led_handler(struct vmm_vkeyboard *vkbd, void (*led_change)(struct vmm_vkeyboard *, int, void *), void *private);

/** Delete led handler from a virtual keyboard */
int vmm_vkeyboard_del_led_handler(struct vmm_vkeyboard *vkbd, void (*led_change)(struct vmm_vkeyboard *, int, void *), void *private);

/** Set ledstate of virtual keyboard */
void vmm_vkeyboard_set_ledstate(struct vmm_vkeyboard *vkbd, int ledstate);

/** Get ledstate of virtual keyboard */
int vmm_vkeyboard_get_ledstate(struct vmm_vkeyboard *vkbd);

/** Find a virtual keyboard with given name */
struct vmm_vkeyboard *vmm_vkeyboard_find(const char *name);

/** Iterate over each virtual keyboard */
int vmm_vkeyboard_iterate(struct vmm_vkeyboard *start, void *data, int (*fn)(struct vmm_vkeyboard *vkbd, void *data));

/** Count of available virtual keyboards */
uint32_t vmm_vkeyboard_count(void);

/* Mouse buttons */
#define VMM_MOUSE_LBUTTON 0x01
#define VMM_MOUSE_RBUTTON 0x02
#define VMM_MOUSE_MBUTTON 0x04

/** Representation of a virtual mouse */
struct vmm_vmouse {
    double_list_t head;
    char          name[VMM_FIELD_NAME_SIZE];
    bool          absolute;
    uint32_t      graphics_width;
    uint32_t      graphics_height;
    uint32_t      graphics_rotation;
    int           abs_x;
    int           abs_y;
    int           abs_z;
    void (*mouse_event)(struct vmm_vmouse *vmou, int dx, int dy, int dz, int buttons_state);
    void *private;
};

/** Create a virtual mouse */
struct vmm_vmouse *vmm_vmouse_create(
    const char *name, bool absolute, void (*mouse_event)(struct vmm_vmouse *vmou, int dx, int dy, int dz, int buttons_state), void *private);

/** Destroy a virtual mouse */
int vmm_vmouse_destroy(struct vmm_vmouse *vmou);

/** Retrive private context of virtual mouse */
static inline void *vmm_vmouse_private(struct vmm_vmouse *vmou)
{
    return (vmou) ? vmou->private : NULL;
}

/** Trigger virtual mouse event */
int vmm_vmouse_event(struct vmm_vmouse *vmou, int dx, int dy, int dz, int buttons_state);

/** Reset virtual mouse */
void vmm_vmouse_reset(struct vmm_vmouse *vmou);

/** Get absolute X position of virtual mouse */
int vmm_vmouse_absolute_x(struct vmm_vmouse *vmou);

/** Get absolute Y position of virtual mouse */
int vmm_vmouse_absolute_y(struct vmm_vmouse *vmou);

/** Get absolute Z position of virtual mouse */
int vmm_vmouse_absolute_z(struct vmm_vmouse *vmou);

/** Check whether virtual mouse uses absolute positioning */
bool vmm_vmouse_is_absolute(struct vmm_vmouse *vmou);

/** Set graphics width for virtual mouse
 *  Note: This is required for relative virtual mouse
 */
void vmm_vmouse_set_graphics_width(struct vmm_vmouse *vmou, uint32_t width);

/** Get graphics width for virtual mouse
 *  Note: This is required for relative virtual mouse
 */
uint32_t vmm_vmouse_get_graphics_width(struct vmm_vmouse *vmou);

/** Set graphics height for virtual mouse
 *  Note: This is required for relative virtual mouse
 */
void vmm_vmouse_set_graphics_height(struct vmm_vmouse *vmou, uint32_t height);

/** Get graphics height for virtual mouse
 *  Note: This is required for relative virtual mouse
 */
uint32_t vmm_vmouse_get_graphics_height(struct vmm_vmouse *vmou);

/** Set graphics rotation angle for virtual mouse  */
void vmm_vmouse_set_graphics_rotation(struct vmm_vmouse *vmou, uint32_t rotation);

/** Get graphics rotation angle for virtual mouse */
uint32_t vmm_vmouse_get_graphics_rotation(struct vmm_vmouse *vmou);

/** Find a virtual mouse with given name */
struct vmm_vmouse *vmm_vmouse_find(const char *name);

/** Iterate over each virtual mouse */
int vmm_vmouse_iterate(struct vmm_vmouse *start, void *data, int (*fn)(struct vmm_vmouse *vmou, void *data));

/** Count of available virtual mouses */
uint32_t vmm_vmouse_count(void);

#endif /* __VMM_VINPUT_H_ */
