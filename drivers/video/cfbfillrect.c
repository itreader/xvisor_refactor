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
 * @file cfbfillrect.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic software accelerated fill rectangle
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/video/cfbfillrect.c
 *
 *  Generic fillrect for frame buffers with packed pixels of any depth.
 *
 *      Copyright (C)  2000 James Simmons (jsimmons@linux-fbdev.org)
 *
 * NOTES:
 *
 *  Also need to add code to deal with cards endians that are different than
 *  the native cpu endians. I also need to deal with MSB position in the word.
 *
 * The original code is licensed under the GPL.
 */

#include <drv/frame_buffer.h>
#include <libs/bitops.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#include "fb_draw.h"

#if BITS_PER_LONG == 32
#define FB_WRITEL fb_writel
#define FB_READL  fb_readl
#else
#define FB_WRITEL fb_writeq
#define FB_READL  fb_readq
#endif

/*
 *  Aligned pattern fill using 32/64-bit memory accesses
 */

static void bitfill_aligned(struct frame_buffer_info *p, uint64_t *dst, int dst_idx, uint64_t pat, unsigned n, int bits, uint32_t bswapmask)
{
    uint64_t first, last;

    if (!n) {
        return;
    }

    first = fb_shifted_pixels_mask_long(p, dst_idx, bswapmask);
    last  = ~fb_shifted_pixels_mask_long(p, umod32(dst_idx + n, bits), bswapmask);

    if (dst_idx + n <= bits) {
        // Single word
        if (last) {
            first &= last;
        }

        FB_WRITEL(comp(pat, FB_READL(dst), first), dst);
    } else {
        // Multiple destination words

        // Leading bits
        if (first != ~0UL) {
            FB_WRITEL(comp(pat, FB_READL(dst), first), dst);
            dst++;
            n -= bits - dst_idx;
        }

        // Main chunk
        n = udiv32(n, bits);

        while (n >= 8) {
            FB_WRITEL(pat, dst++);
            FB_WRITEL(pat, dst++);
            FB_WRITEL(pat, dst++);
            FB_WRITEL(pat, dst++);
            FB_WRITEL(pat, dst++);
            FB_WRITEL(pat, dst++);
            FB_WRITEL(pat, dst++);
            FB_WRITEL(pat, dst++);
            n -= 8;
        }

        while (n--) {
            FB_WRITEL(pat, dst++);
        }

        // Trailing bits
        if (last) {
            FB_WRITEL(comp(pat, FB_READL(dst), last), dst);
        }
    }
}

/*
 *  Unaligned generic pattern fill using 32/64-bit memory accesses
 *  The pattern must have been expanded to a full 32/64-bit value
 *  Left/right are the appropriate shifts to convert to the pattern to be
 *  used for the next 32/64-bit word
 */

static void bitfill_unaligned(struct frame_buffer_info *p, uint64_t *dst, int dst_idx, uint64_t pat, int left, int right, unsigned n, int bits)
{
    uint64_t first, last;

    if (!n) {
        return;
    }

    first = FB_SHIFT_HIGH(p, ~0UL, dst_idx);
    last  = ~(FB_SHIFT_HIGH(p, ~0UL, umod32(dst_idx + n, bits)));

    if (dst_idx + n <= bits) {
        // Single word
        if (last) {
            first &= last;
        }

        FB_WRITEL(comp(pat, FB_READL(dst), first), dst);
    } else {
        // Multiple destination words
        // Leading bits
        if (first) {
            FB_WRITEL(comp(pat, FB_READL(dst), first), dst);
            dst++;
            pat = pat << left | pat >> right;
            n -= bits - dst_idx;
        }

        // Main chunk
        n = udiv32(n, bits);

        while (n >= 4) {
            FB_WRITEL(pat, dst++);
            pat = pat << left | pat >> right;
            FB_WRITEL(pat, dst++);
            pat = pat << left | pat >> right;
            FB_WRITEL(pat, dst++);
            pat = pat << left | pat >> right;
            FB_WRITEL(pat, dst++);
            pat = pat << left | pat >> right;
            n -= 4;
        }

        while (n--) {
            FB_WRITEL(pat, dst++);
            pat = pat << left | pat >> right;
        }

        // Trailing bits
        if (last) {
            FB_WRITEL(comp(pat, FB_READL(dst), last), dst);
        }
    }
}

/*
 *  Aligned pattern invert using 32/64-bit memory accesses
 */
static void bitfill_aligned_rev(struct frame_buffer_info *p, uint64_t *dst, int dst_idx, uint64_t pat, unsigned n, int bits, uint32_t bswapmask)
{
    uint64_t val = pat, dat;
    uint64_t first, last;

    if (!n) {
        return;
    }

    first = fb_shifted_pixels_mask_long(p, dst_idx, bswapmask);
    last  = ~fb_shifted_pixels_mask_long(p, umod32(dst_idx + n, bits), bswapmask);

    if (dst_idx + n <= bits) {
        // Single word
        if (last) {
            first &= last;
        }

        dat = FB_READL(dst);
        FB_WRITEL(comp(dat ^ val, dat, first), dst);
    } else {
        // Multiple destination words
        // Leading bits
        if (first != 0UL) {
            dat = FB_READL(dst);
            FB_WRITEL(comp(dat ^ val, dat, first), dst);
            dst++;
            n -= bits - dst_idx;
        }

        // Main chunk
        n = udiv32(n, bits);

        while (n >= 8) {
            FB_WRITEL(FB_READL(dst) ^ val, dst);
            dst++;
            FB_WRITEL(FB_READL(dst) ^ val, dst);
            dst++;
            FB_WRITEL(FB_READL(dst) ^ val, dst);
            dst++;
            FB_WRITEL(FB_READL(dst) ^ val, dst);
            dst++;
            FB_WRITEL(FB_READL(dst) ^ val, dst);
            dst++;
            FB_WRITEL(FB_READL(dst) ^ val, dst);
            dst++;
            FB_WRITEL(FB_READL(dst) ^ val, dst);
            dst++;
            FB_WRITEL(FB_READL(dst) ^ val, dst);
            dst++;
            n -= 8;
        }

        while (n--) {
            FB_WRITEL(FB_READL(dst) ^ val, dst);
            dst++;
        }

        // Trailing bits
        if (last) {
            dat = FB_READL(dst);
            FB_WRITEL(comp(dat ^ val, dat, last), dst);
        }
    }
}

/*
 *  Unaligned generic pattern invert using 32/64-bit memory accesses
 *  The pattern must have been expanded to a full 32/64-bit value
 *  Left/right are the appropriate shifts to convert to the pattern to be
 *  used for the next 32/64-bit word
 */

static void bitfill_unaligned_rev(struct frame_buffer_info *p, uint64_t *dst, int dst_idx, uint64_t pat, int left, int right, unsigned n, int bits)
{
    uint64_t first, last, dat;

    if (!n) {
        return;
    }

    first = FB_SHIFT_HIGH(p, ~0UL, dst_idx);
    last  = ~(FB_SHIFT_HIGH(p, ~0UL, umod32(dst_idx + n, bits)));

    if (dst_idx + n <= bits) {
        // Single word
        if (last) {
            first &= last;
        }

        dat = FB_READL(dst);
        FB_WRITEL(comp(dat ^ pat, dat, first), dst);
    } else {
        // Multiple destination words

        // Leading bits
        if (first != 0UL) {
            dat = FB_READL(dst);
            FB_WRITEL(comp(dat ^ pat, dat, first), dst);
            dst++;
            pat = pat << left | pat >> right;
            n -= bits - dst_idx;
        }

        // Main chunk
        n = udiv32(n, bits);

        while (n >= 4) {
            FB_WRITEL(FB_READL(dst) ^ pat, dst);
            dst++;
            pat = pat << left | pat >> right;
            FB_WRITEL(FB_READL(dst) ^ pat, dst);
            dst++;
            pat = pat << left | pat >> right;
            FB_WRITEL(FB_READL(dst) ^ pat, dst);
            dst++;
            pat = pat << left | pat >> right;
            FB_WRITEL(FB_READL(dst) ^ pat, dst);
            dst++;
            pat = pat << left | pat >> right;
            n -= 4;
        }

        while (n--) {
            FB_WRITEL(FB_READL(dst) ^ pat, dst);
            dst++;
            pat = pat << left | pat >> right;
        }

        // Trailing bits
        if (last) {
            dat = FB_READL(dst);
            FB_WRITEL(comp(dat ^ pat, dat, last), dst);
        }
    }
}

void cframe_buffer_fillrect(struct frame_buffer_info *p, const struct frame_buffer_fillrect *rect)
{
    uint64_t  pat, pat2, fg;
    uint64_t  width = rect->width, height = rect->height;
    int       bits = BITS_PER_LONG, bytes = bits >> 3;
    uint32_t  bpp = p->var.bits_per_pixel;
    uint64_t *dst;
    int       dst_idx, left;

    if (p->state != FBINFO_STATE_RUNNING) {
        return;
    }

    if (p->fix.visual == FB_VISUAL_TRUECOLOR || p->fix.visual == FB_VISUAL_DIRECTCOLOR) {
        fg = ((uint32_t *)(p->pseudo_palette))[rect->color];
    } else {
        fg = rect->color;
    }

    pat     = pixel_to_pat(bpp, fg);

    dst     = (uint64_t *)((uint64_t)p->screen_base & ~(bytes - 1));
    dst_idx = ((uint64_t)p->screen_base & (bytes - 1)) * 8;
    dst_idx += rect->dy * p->fix.line_length * 8 + rect->dx * bpp;
    /* FIXME For now we support 1-32 bpp only */
    left = umod32(bits, bpp);

    if (p->fbops->fb_sync) {
        p->fbops->fb_sync(p);
    }

    if (!left) {
        uint32_t bswapmask = fb_compute_bswapmask(p);
        void (*fill_op32)(struct frame_buffer_info *p, uint64_t *dst, int dst_idx, uint64_t pat, unsigned n, int bits, uint32_t bswapmask) = NULL;

        switch (rect->rop) {
            case ROP_XOR:
                fill_op32 = bitfill_aligned_rev;
                break;

            case ROP_COPY:
                fill_op32 = bitfill_aligned;
                break;

            default:
                vmm_printf("cframe_buffer_fillrect(): unknown rop, defaulting to ROP_COPY\n");
                fill_op32 = bitfill_aligned;
                break;
        }

        while (height--) {
            dst += dst_idx >> (ffs(bits) - 1);
            dst_idx &= (bits - 1);
            fill_op32(p, dst, dst_idx, pat, width * bpp, bits, bswapmask);
            dst_idx += p->fix.line_length * 8;
        }
    } else {
        int right, r;
        void (*fill_op)(struct frame_buffer_info *p, uint64_t *dst, int dst_idx, uint64_t pat, int left, int right, unsigned n, int bits) = NULL;
#ifdef CONFIG_CPU_LE
        right = left;
        left  = bpp - right;
#else
        right = bpp - left;
#endif

        switch (rect->rop) {
            case ROP_XOR:
                fill_op = bitfill_unaligned_rev;
                break;

            case ROP_COPY:
                fill_op = bitfill_unaligned;
                break;

            default:
                vmm_printf("cframe_buffer_fillrect(): unknown rop, defaulting to ROP_COPY\n");
                fill_op = bitfill_unaligned;
                break;
        }

        while (height--) {
            dst += udiv32(dst_idx, bits);
            dst_idx &= (bits - 1);
            r    = umod32(dst_idx, bpp);
            /* rotate pattern to the correct start position */
            pat2 = le_long_to_cpu(rolx(cpu_to_le_long(pat), r, bpp));
            fill_op(p, dst, dst_idx, pat2, left, right, width * bpp, bits);
            dst_idx += p->fix.line_length * 8;
        }
    }
}

VMM_ERR_XPORT_SYMBOL(cframe_buffer_fillrect);
