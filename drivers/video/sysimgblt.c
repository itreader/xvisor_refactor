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
 * @file sysimgblt.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 1-bit/8-bit to 1-32 bit color expansion (sys-to-sys)
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/video/sysimgblt.c
 *
 *  Generic 1-bit or 8-bit source to 1-32 bit destination expansion
 *  for frame buffer located in system RAM with packed pixels of any depth.
 *
 *  Based almost entirely on cfbimgblt.c
 *
 *      Copyright (C)  April 2007 Antonino Daplas <adaplas@pol.net>
 *
 * The original code is licensed under the GPL.
 */

#include <drv/frame_buffer.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define DEBUG

#ifdef DEBUG
#define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __func__, ##args)
#else
#define DPRINTK(fmt, args...)
#endif

static const uint32_t cfb_tab8_be[]  = {0x00000000, 0x000000ff, 0x0000ff00, 0x0000ffff, 0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff,
                                        0xff000000, 0xff0000ff, 0xff00ff00, 0xff00ffff, 0xffff0000, 0xffff00ff, 0xffffff00, 0xffffffff};

static const uint32_t cfb_tab8_le[]  = {0x00000000, 0xff000000, 0x00ff0000, 0xffff0000, 0x0000ff00, 0xff00ff00, 0x00ffff00, 0xffffff00,
                                        0x000000ff, 0xff0000ff, 0x00ff00ff, 0xffff00ff, 0x0000ffff, 0xff00ffff, 0x00ffffff, 0xffffffff};

static const uint32_t cfb_tab16_be[] = {0x00000000, 0x0000ffff, 0xffff0000, 0xffffffff};

static const uint32_t cfb_tab16_le[] = {0x00000000, 0xffff0000, 0x0000ffff, 0xffffffff};

static const uint32_t cfb_tab32[]    = {0x00000000, 0xffffffff};

static void color_imageblit(
    const struct frame_buffer_image *image, struct frame_buffer_info *p, void *dst1, uint32_t start_index, uint32_t pitch_index)
{
    /* Draw the penguin */
    uint32_t      *dst, *dst2;
    uint32_t       color = 0, val, shift;
    int            i, n, bpp = p->var.bits_per_pixel;
    uint32_t       null_bits = 32 - bpp;
    uint32_t      *palette   = (uint32_t *)p->pseudo_palette;
    const uint8_t *src       = (const uint8_t *)image->data;

    dst2                     = dst1;

    for (i = image->height; i--;) {
        n     = image->width;
        dst   = dst1;
        shift = 0;
        val   = 0;

        if (start_index) {
            uint32_t start_mask = ~(FB_SHIFT_HIGH(p, ~(uint32_t)0, start_index));
            val                 = *dst & start_mask;
            shift               = start_index;
        }

        while (n--) {
            if (p->fix.visual == FB_VISUAL_TRUECOLOR || p->fix.visual == FB_VISUAL_DIRECTCOLOR) {
                color = palette[*src];
            } else {
                color = *src;
            }

            color <<= FB_LEFT_POS(p, bpp);
            val |= FB_SHIFT_HIGH(p, color, shift);

            if (shift >= null_bits) {
                *dst++ = val;

                val    = (shift == null_bits) ? 0 : FB_SHIFT_LOW(p, color, 32 - shift);
            }

            shift += bpp;
            shift &= (32 - 1);
            src++;
        }

        if (shift) {
            uint32_t end_mask = FB_SHIFT_HIGH(p, ~(uint32_t)0, shift);

            *dst &= end_mask;
            *dst |= val;
        }

        dst1 += p->fix.line_length;

        if (pitch_index) {
            dst2 += p->fix.line_length;
            dst1 = (uint8_t *)((long)dst2 & ~(sizeof(uint32_t) - 1));

            start_index += pitch_index;
            start_index &= 32 - 1;
        }
    }
}

static void slow_imageblit(
    const struct frame_buffer_image *image, struct frame_buffer_info *p, void *dst1, uint32_t fgcolor, uint32_t bgcolor, uint32_t start_index,
    uint32_t pitch_index)
{
    uint32_t       shift, color = 0, bpp = p->var.bits_per_pixel;
    uint32_t      *dst, *dst2;
    uint32_t       val, pitch = p->fix.line_length;
    uint32_t       null_bits = 32 - bpp;
    uint32_t       spitch    = (image->width + 7) / 8;
    const uint8_t *src       = (const uint8_t *)image->data, *s;
    uint32_t       i, j, l;

    dst2 = dst1;
    fgcolor <<= FB_LEFT_POS(p, bpp);
    bgcolor <<= FB_LEFT_POS(p, bpp);

    for (i = image->height; i--;) {
        shift = val = 0;
        l           = 8;
        j           = image->width;
        dst         = dst1;
        s           = src;

        /* write leading bits */
        if (start_index) {
            uint32_t start_mask = ~(FB_SHIFT_HIGH(p, ~(uint32_t)0, start_index));
            val                 = *dst & start_mask;
            shift               = start_index;
        }

        while (j--) {
            l--;
            color = (*s & (1 << l)) ? fgcolor : bgcolor;
            val |= FB_SHIFT_HIGH(p, color, shift);

            /* Did the bitshift spill bits to the next long? */
            if (shift >= null_bits) {
                *dst++ = val;
                val    = (shift == null_bits) ? 0 : FB_SHIFT_LOW(p, color, 32 - shift);
            }

            shift += bpp;
            shift &= (32 - 1);

            if (!l) {
                l = 8;
                s++;
            };
        }

        /* write trailing bits */
        if (shift) {
            uint32_t end_mask = FB_SHIFT_HIGH(p, ~(uint32_t)0, shift);

            *dst &= end_mask;
            *dst |= val;
        }

        dst1 += pitch;
        src += spitch;

        if (pitch_index) {
            dst2 += pitch;
            dst1 = (uint8_t *)((long)dst2 & ~(sizeof(uint32_t) - 1));
            start_index += pitch_index;
            start_index &= 32 - 1;
        }
    }
}

/*
 * fast_imageblit - optimized monochrome color expansion
 *
 * Only if:  bits_per_pixel == 8, 16, or 32
 *           image->width is divisible by pixel/dword (ppw);
 *           fix->line_legth is divisible by 4;
 *           beginning and end of a scanline is dword aligned
 */
static void fast_imageblit(const struct frame_buffer_image *image, struct frame_buffer_info *p, void *dst1, uint32_t fgcolor, uint32_t bgcolor)
{
    uint32_t        fgx = fgcolor, bgx = bgcolor, bpp = p->var.bits_per_pixel;
    uint32_t        ppw = udiv32(32, bpp), spitch = (image->width + 7) / 8;
    uint32_t        bit_mask, end_mask, eorx, shift;
    const char     *s = image->data, *src;
    uint32_t       *dst;
    const uint32_t *tab = NULL;
    int             i, j, k;

    switch (bpp) {
        case 8:
            tab = fb_be_math(p) ? cfb_tab8_be : cfb_tab8_le;
            break;

        case 16:
            tab = fb_be_math(p) ? cfb_tab16_be : cfb_tab16_le;
            break;

        case 32:
        default:
            tab = cfb_tab32;
            break;
    }

    for (i = ppw - 1; i--;) {
        fgx <<= bpp;
        bgx <<= bpp;
        fgx |= fgcolor;
        bgx |= bgcolor;
    }

    bit_mask = (1 << ppw) - 1;
    eorx     = fgx ^ bgx;
    k        = udiv32(image->width, ppw);

    for (i = image->height; i--;) {
        dst   = dst1;
        shift = 8;
        src   = s;

        for (j = k; j--;) {
            shift -= ppw;
            end_mask = tab[(*src >> shift) & bit_mask];
            *dst++   = (end_mask & eorx) ^ bgx;

            if (!shift) {
                shift = 8;
                src++;
            }
        }

        dst1 += p->fix.line_length;
        s += spitch;
    }
}

void sys_imageblit(struct frame_buffer_info *p, const struct frame_buffer_image *image)
{
    uint32_t fgcolor, bgcolor, start_index, bitstart, pitch_index = 0;
    uint32_t bpl = sizeof(uint32_t), bpp = p->var.bits_per_pixel;
    uint32_t width = image->width;
    uint32_t dx = image->dx, dy = image->dy;
    void    *dst1;

    if (p->state != FBINFO_STATE_RUNNING) {
        return;
    }

    bitstart    = (dy * p->fix.line_length * 8) + (dx * bpp);
    start_index = bitstart & (32 - 1);
    pitch_index = (p->fix.line_length & (bpl - 1)) * 8;

    bitstart /= 8;
    bitstart &= ~(bpl - 1);
    dst1 = (void *)p->screen_base + bitstart;

    if (p->fbops->fb_sync) {
        p->fbops->fb_sync(p);
    }

    if (image->depth == 1) {
        if (p->fix.visual == FB_VISUAL_TRUECOLOR || p->fix.visual == FB_VISUAL_DIRECTCOLOR) {
            fgcolor = ((uint32_t *)(p->pseudo_palette))[image->fg_color];
            bgcolor = ((uint32_t *)(p->pseudo_palette))[image->bg_color];
        } else {
            fgcolor = image->fg_color;
            bgcolor = image->bg_color;
        }

        if (umod32(32, bpp) == 0 && !start_index && !pitch_index && ((width & (udiv32(32, bpp) - 1)) == 0) && bpp >= 8 && bpp <= 32) {
            fast_imageblit(image, p, dst1, fgcolor, bgcolor);
        } else {
            slow_imageblit(image, p, dst1, fgcolor, bgcolor, start_index, pitch_index);
        }
    } else {
        color_imageblit(image, p, dst1, start_index, pitch_index);
    }
}

VMM_EXPORT_SYMBOL(sys_imageblit);
