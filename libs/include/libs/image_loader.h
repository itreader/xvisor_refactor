/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
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
 * @file image_loader.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Simple image loader.
 */

#ifndef _IMAGE_LOADER_H
#define _IMAGE_LOADER_H

#include <drv/frame_buffer.h>
#include <vmm_error.h>

#define IMAGE_LOADER_IPRIORITY 1

struct image_format {
    int                          bits_per_pixel;
    struct frame_buffer_bitfield red;
    struct frame_buffer_bitfield green;
    struct frame_buffer_bitfield blue;
    struct frame_buffer_bitfield transp;
};

#if IS_ENABLED(CONFIG_IMAGE_LOADER)

int image_load(const char *path, struct image_format *fmt, struct frame_buffer_image *image);

void image_release(struct frame_buffer_image *image);

int image_draw(struct frame_buffer_info *info, const struct frame_buffer_image *image, uint32_t x, uint32_t y, uint32_t w, uint32_t h);

#else

static inline int image_load(const char *path, struct image_format *fmt, struct frame_buffer_image *image)
{
    return VMM_ERR_NOTSUPP;
}

static inline void image_release(struct frame_buffer_image *image) {}

static inline int image_draw(struct frame_buffer_info *info, const struct frame_buffer_image *image, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    return VMM_ERR_NOTSUPP;
}

#endif

#endif /* !_IMAGE_LOADER_H */
