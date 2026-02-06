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
 * @file vmm_virtual_display.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for virtual display subsystem
 */

/* The virtual display subsystem has two important entites namely
 * vmm_virtual_display and vmm_surface.
 *
 * GUI rendering daemons (VNC daemon or FB daemon or ...) create
 * vmm_surface instance and add/bind it to a vmm_virtual_display instance.
 * More than one GUI rendering daemons can add their vmm_surface
 * instances to a single vmm_virtual_display instance. The GUI rendering
 * daemons will also use vmm_virtual_display_one_update() API to periodically
 * update/sync vmm_surface instance with vmm_virtual_display instance.
 *
 * Display (or framebuffer) emulators create vmm_virtual_display instance
 * to emulate a virtual display. The display emulator will also use
 * vmm_virtual_display_surface_xxx() APIs to give hints to vmm_surface
 * instances about changes in virtual display.
 */

#ifndef __VMM_VDISPLAY_H_
#define __VMM_VDISPLAY_H_

#include <libs/list.h>
#include <vmm_limits.h>
#include <vmm_manager.h>
#include <vmm_notifier.h>
#include <vmm_types.h>

#define VMM_VDISPLAY_IPRIORITY     0

/* Notifier event when virtual display is created */
#define VMM_VDISPLAY_EVENT_CREATE  0x01
/* Notifier event when virtual display is destroyed */
#define VMM_VDISPLAY_EVENT_DESTROY 0x02

/** Representation of virtual input notifier event */
struct vmm_virtual_display_event {
    void *data;
};

/** Register a notifier client to receive virtual display events */
int vmm_virtual_display_register_client(vmm_notifier_block_t *nb);

/** Unregister a notifier client to not receive virtual display events */
int vmm_virtual_display_unregister_client(vmm_notifier_block_t *nb);

/** Representation of a pixel format */
struct vmm_pixelformat {
    uint8_t  bits_per_pixel;
    uint8_t  bytes_per_pixel;
    uint8_t  depth; /* color depth in bits */
    uint32_t rmask, gmask, bmask, amask;
    uint8_t  rshift, gshift, bshift, ashift;
    uint8_t  rmax, gmax, bmax, amax;
    uint8_t  rbits, gbits, bbits, abits;
};

/** Default initialization for pixel format */
void vmm_pixelformat_init_default(struct vmm_pixelformat *pf, int bpp);

/** Default initialization with different endianness for pixel format */
void vmm_pixelformat_init_different_endian(struct vmm_pixelformat *pf, int bpp);

struct vmm_surface;

/** Representation of surface operations
 *  Note: All surface operations are optional.
 *  Note: All surface operations are usually called with the
 *  'surface_list_lock' of the associated virtual display held
 *  hence, we cannot sleep in these operations.
 *  Note: Typically, all surface operations (except copyto_data
 *  and copyfrom_data) should be used to schedule a background or
 *  bottom-half work.
 */
struct vmm_surface_ops {
    void (*write8)(struct vmm_surface *s, uint8_t *dst, uint8_t val);
    uint8_t (*read8)(struct vmm_surface *s, uint8_t *src);
    void (*write16)(struct vmm_surface *s, uint16_t *dst, uint16_t val);
    uint16_t (*read16)(struct vmm_surface *s, uint16_t *src);
    void (*write32)(struct vmm_surface *s, uint32_t *dst, uint16_t val);
    uint16_t (*read32)(struct vmm_surface *s, uint32_t *src);

    void (*refresh)(struct vmm_surface *s);

    void (*gfx_clear)(struct vmm_surface *s);
    void (*gfx_update)(struct vmm_surface *s, int x, int y, int w, int h);
    void (*gfx_resize)(struct vmm_surface *s, int w, int h);
    void (*gfx_copy)(struct vmm_surface *s, int src_x, int src_y, int dst_x, int dst_y, int w, int h);

    void (*text_clear)(struct vmm_surface *s);
    void (*text_cursor)(struct vmm_surface *s, int x, int y);
    void (*text_resize)(struct vmm_surface *s, int w, int h);
    void (*text_update)(struct vmm_surface *s, int x, int y, int w, int h);
};

#define VMM_SURFACE_BIG_ENDIAN_FLAG 0x01
#define VMM_SURFACE_ALLOCED_FLAG    0x02

/** Representation of a surface */
struct vmm_surface {
    double_list_t                 head;
    char                          name[VMM_FIELD_NAME_SIZE];
    void                         *data;
    uint32_t                      data_size;
    int                           height;
    int                           width;
    uint32_t                      flags;
    struct vmm_pixelformat        pf;
    const struct vmm_surface_ops *ops;
    void *private;
};

/** Retrive private context of surface */
static inline void *vmm_surface_private(struct vmm_surface *s)
{
    return (s) ? s->private : NULL;
}

/** Write 8bit to surface data */
static inline void vmm_surface_write8(struct vmm_surface *s, uint8_t *dst, uint8_t v)
{
    if (s && s->ops && s->ops->write8) {
        s->ops->write8(s, dst, v);
    } else {
        *dst = v;
    }
}

/** Read 8bit from surface data */
static inline uint8_t vmm_surface_read8(struct vmm_surface *s, uint8_t *src)
{
    if (s && s->ops && s->ops->read8) {
        return s->ops->read8(s, src);
    } else {
        return *src;
    }
}

/** Write 16bit to surface data */
static inline void vmm_surface_write16(struct vmm_surface *s, uint16_t *dst, uint16_t v)
{
    if (s && s->ops && s->ops->write16) {
        s->ops->write16(s, dst, v);
    } else {
        *dst = v;
    }
}

/** Read 16bit from surface data */
static inline uint16_t vmm_surface_read16(struct vmm_surface *s, uint16_t *src)
{
    if (s && s->ops && s->ops->read16) {
        return s->ops->read16(s, src);
    } else {
        return *src;
    }
}

/** Write 32bit to surface data */
static inline void vmm_surface_write32(struct vmm_surface *s, uint32_t *dst, uint16_t v)
{
    if (s && s->ops && s->ops->write32) {
        s->ops->write32(s, dst, v);
    } else {
        *dst = v;
    }
}

/** Read 32bit from surface data */
static inline uint32_t vmm_surface_read32(struct vmm_surface *s, uint32_t *src)
{
    if (s && s->ops && s->ops->read32) {
        return s->ops->read32(s, src);
    } else {
        return *src;
    }
}

/** Update surface data from guest memory */
void vmm_surface_update(
    struct vmm_surface *s, struct vmm_guest *guest, physical_addr_t gphys, int cols, /* Width in pixels. */
    int rows,                                                                        /* Height in pixels. */
    int src_width,                                                                   /* Length of source line, in bytes. */
    int dest_row_pitch,                                                              /* Bytes between adjacent horizontal output pixels. */
    int dest_col_pitch,                                                              /* Bytes between adjacent vertical output pixels. */
    void (*fn)(struct vmm_surface *s, void *private, uint8_t *dst, const uint8_t *src, int width, int deststep), void *fn_private,
    int *first_row,                                                                  /* Input and output. */
    int *last_row);                                                                  /* Output only. */

/** Initialize a surface */
int vmm_surface_init(
    struct vmm_surface *s, const char *name, void *data, uint32_t data_size, int height, int width, uint32_t flags, struct vmm_pixelformat *pf,
    const struct vmm_surface_ops *ops, void *private);

/** Alloc a new surface */
struct vmm_surface *vmm_surface_alloc(
    const char *name, void *data, uint32_t data_size, int height, int width, uint32_t flags, struct vmm_pixelformat *pf,
    const struct vmm_surface_ops *ops, void *private);

/** Free an alloced surface */
void vmm_surface_free(struct vmm_surface *s);

/** Retrive row stride of given surface */
static inline int vmm_surface_stride(struct vmm_surface *s)
{
    return (s) ? s->width * s->pf.bytes_per_pixel : 0;
}

/** Retrive data pointer of given surface */
static inline void *vmm_surface_data(struct vmm_surface *s)
{
    return (s) ? s->data : NULL;
}

/** Retrive width of given surface */
static inline int vmm_surface_width(struct vmm_surface *s)
{
    return (s) ? s->width : 0;
}

/** Retrive height of given surface */
static inline int vmm_surface_height(struct vmm_surface *s)
{
    return (s) ? s->height : 0;
}

/** Retrive bits-per-pixel of given surface */
static inline int vmm_surface_bits_per_pixel(struct vmm_surface *s)
{
    return (s) ? s->pf.bits_per_pixel : 0;
}

/** Retrive bytes-per-pixel of given surface */
static inline int vmm_surface_bytes_per_pixel(struct vmm_surface *s)
{
    return (((s) ? s->pf.bits_per_pixel : 0) + 7) / 8;
}

struct vmm_virtual_display;

/** Representation of a virtual display operations */
struct vmm_virtual_display_ops {
    void (*invalidate)(struct vmm_virtual_display *vdis);
    int (*gfx_pixeldata)(struct vmm_virtual_display *vdis, struct vmm_pixelformat *pf, uint32_t *rows, uint32_t *cols, physical_addr_t *pa);
    void (*gfx_update)(struct vmm_virtual_display *vdis, struct vmm_surface *s);
    void (*text_update)(struct vmm_virtual_display *vdis, uint64_t *text);
};

/** Representation of a virtual display */
struct vmm_virtual_display {
    double_list_t                         head;
    char                                  name[VMM_FIELD_NAME_SIZE];
    vmm_spinlock_t                        surface_list_lock;
    double_list_t                         surface_list;
    const struct vmm_virtual_display_ops *ops;
    void *private;
};

/** Retreive pixel format and host physical address
 *  of given virtual display
 */
int vmm_virtual_display_get_pixeldata(
    struct vmm_virtual_display *vdis, struct vmm_pixelformat *pf, uint32_t *rows, uint32_t *cols, physical_addr_t *pa);

/** Update a particular surface for given virtual display */
void vmm_virtual_display_one_update(struct vmm_virtual_display *vdis, struct vmm_surface *s);

/** Update all surfaces for given virtual display */
void vmm_virtual_display_update(struct vmm_virtual_display *vdis);

/** Invalidate a given virtual display */
void vmm_virtual_display_invalidate(struct vmm_virtual_display *vdis);

/** Text update a given virtual display */
void vmm_virtual_display_text_update(struct vmm_virtual_display *vdis, uint64_t *chardata);

/** Refresh all surfaces for given virtual display */
void vmm_virtual_display_surface_refresh(struct vmm_virtual_display *vdis);

/** Clear all surfaces for given virtual display */
void vmm_virtual_display_surface_gfx_clear(struct vmm_virtual_display *vdis);

/** Update all surfaces for given virtual display */
void vmm_virtual_display_surface_gfx_update(struct vmm_virtual_display *vdis, int x, int y, int w, int h);

/** Resize all surfaces for given virtual display */
void vmm_virtual_display_surface_gfx_resize(struct vmm_virtual_display *vdis, int w, int h);

/** Copy data on all surfaces for given virtual display */
void vmm_virtual_display_surface_gfx_copy(struct vmm_virtual_display *vdis, int src_x, int src_y, int dst_x, int dst_y, int w, int h);

/** Clear text on all surfaces for given virtual display */
void vmm_virtual_display_surface_text_clear(struct vmm_virtual_display *vdis);

/** Set text cursor on all surfaces for given virtual display */
void vmm_virtual_display_surface_text_cursor(struct vmm_virtual_display *vdis, int x, int y);

/** Update text on all surfaces for given virtual display */
void vmm_virtual_display_surface_text_update(struct vmm_virtual_display *vdis, int x, int y, int w, int h);

/** Resize text on all surfaces for given virtual display */
void vmm_virtual_display_surface_text_resize(struct vmm_virtual_display *vdis, int w, int h);

/** Add surface to a virtual display */
int vmm_virtual_display_add_surface(struct vmm_virtual_display *vdis, struct vmm_surface *s);

/** Delete surface from a virtual display */
int vmm_virtual_display_del_surface(struct vmm_virtual_display *vdis, struct vmm_surface *s);

/** Create a virtual display */
struct vmm_virtual_display *vmm_virtual_display_create(const char *name, const struct vmm_virtual_display_ops *ops, void *private);

/** Destroy a virtual display */
int vmm_virtual_display_destroy(struct vmm_virtual_display *vdis);

/** Retrive private context of virtual display */
static inline void *vmm_virtual_display_private(struct vmm_virtual_display *vdis)
{
    return (vdis) ? vdis->private : NULL;
}

/** Find a virtual display with given name */
struct vmm_virtual_display *vmm_virtual_display_find(const char *name);

/** Iterate over each virtual display */
int vmm_virtual_display_iterate(struct vmm_virtual_display *start, void *data, int (*fn)(struct vmm_virtual_display *vdis, void *data));

/** Count of available virtual displays */
uint32_t vmm_virtual_display_count(void);

#endif /* __VMM_VDISPLAY_H_ */
