#ifndef _FB_DRAW_H
#define _FB_DRAW_H

#include <drv/frame_buffer.h>

/*
 *  Compose two values, using a bitmask as decision value
 *  This is equivalent to (a & mask) | (b & ~mask)
 */

static inline uint64_t comp(uint64_t a, uint64_t b, uint64_t mask)
{
    return ((a ^ b) & mask) ^ b;
}

/*
 *  Create a pattern with the given pixel's color
 */

#if BITS_PER_LONG == 64
static inline uint64_t pixel_to_pat(uint32_t bpp, uint32_t pixel)
{
    switch (bpp) {
        case 1:
            return 0xfffffffffffffffful * pixel;

        case 2:
            return 0x5555555555555555ul * pixel;

        case 4:
            return 0x1111111111111111ul * pixel;

        case 8:
            return 0x0101010101010101ul * pixel;

        case 12:
            return 0x1001001001001001ul * pixel;

        case 16:
            return 0x0001000100010001ul * pixel;

        case 24:
            return 0x0001000001000001ul * pixel;

        case 32:
            return 0x0000000100000001ul * pixel;

        default:
            vmm_panic("pixel_to_pat(): unsupported pixelformat\n");
    }
}
#else
static inline uint64_t pixel_to_pat(uint32_t bpp, uint32_t pixel)
{
    switch (bpp) {
        case 1:
            return 0xfffffffful * pixel;

        case 2:
            return 0x55555555ul * pixel;

        case 4:
            return 0x11111111ul * pixel;

        case 8:
            return 0x01010101ul * pixel;

        case 12:
            return 0x01001001ul * pixel;

        case 16:
            return 0x00010001ul * pixel;

        case 24:
            return 0x01000001ul * pixel;

        case 32:
            return 0x00000001ul * pixel;

        default:
            vmm_panic("pixel_to_pat(): unsupported pixelformat\n");
    }
}
#endif

#ifdef CONFIG_FB_CFB_REV_PIXELS_IN_BYTE
#if BITS_PER_LONG == 64
#define REV_PIXELS_MASK1 0x5555555555555555ul
#define REV_PIXELS_MASK2 0x3333333333333333ul
#define REV_PIXELS_MASK4 0x0f0f0f0f0f0f0f0ful
#else
#define REV_PIXELS_MASK1 0x55555555ul
#define REV_PIXELS_MASK2 0x33333333ul
#define REV_PIXELS_MASK4 0x0f0f0f0ful
#endif

static inline uint64_t fb_rev_pixels_in_long(uint64_t val, uint32_t bswapmask)
{
    if (bswapmask & 1) {
        val = comp(val >> 1, val << 1, REV_PIXELS_MASK1);
    }

    if (bswapmask & 2) {
        val = comp(val >> 2, val << 2, REV_PIXELS_MASK2);
    }

    if (bswapmask & 3) {
        val = comp(val >> 4, val << 4, REV_PIXELS_MASK4);
    }

    return val;
}

static inline uint32_t fb_shifted_pixels_mask_u32(struct frame_buffer_info *p, uint32_t index, uint32_t bswapmask)
{
    uint32_t mask;

    if (!bswapmask) {
        mask = FB_SHIFT_HIGH(p, ~(uint32_t)0, index);
    } else {
        mask = 0xff << FB_LEFT_POS(p, 8);
        mask = FB_SHIFT_LOW(p, mask, index & (bswapmask)) & mask;
        mask = FB_SHIFT_HIGH(p, mask, index & ~(bswapmask));
#if defined(__i386__) || defined(__x86_64__)

        /* Shift argument is limited to 0 - 31 on x86 based CPU's */
        if (index + bswapmask < 32)
#endif
            mask |= FB_SHIFT_HIGH(p, ~(uint32_t)0, (index + bswapmask) & ~(bswapmask));
    }

    return mask;
}

static inline uint64_t fb_shifted_pixels_mask_long(struct frame_buffer_info *p, uint32_t index, uint32_t bswapmask)
{
    uint64_t mask;

    if (!bswapmask) {
        mask = FB_SHIFT_HIGH(p, ~0UL, index);
    } else {
        mask = 0xff << FB_LEFT_POS(p, 8);
        mask = FB_SHIFT_LOW(p, mask, index & (bswapmask)) & mask;
        mask = FB_SHIFT_HIGH(p, mask, index & ~(bswapmask));
#if defined(__i386__) || defined(__x86_64__)

        /* Shift argument is limited to 0 - 31 on x86 based CPU's */
        if (index + bswapmask < BITS_PER_LONG)
#endif
            mask |= FB_SHIFT_HIGH(p, ~0UL, (index + bswapmask) & ~(bswapmask));
    }

    return mask;
}

static inline uint32_t fb_compute_bswapmask(struct frame_buffer_info *info)
{
    uint32_t bswapmask = 0;
    unsigned bpp       = info->var.bits_per_pixel;

    if ((bpp < 8) && (info->var.nonstd & FB_NONSTD_REV_PIX_IN_B)) {
        /*
         * Reversed order of pixel layout in bytes
         * works only for 1, 2 and 4 bpp
         */
        bswapmask = 7 - bpp + 1;
    }

    return bswapmask;
}

#else /* CONFIG_FB_CFB_REV_PIXELS_IN_BYTE */

static inline uint64_t fb_rev_pixels_in_long(uint64_t val, uint32_t bswapmask)
{
    return val;
}

#define fb_shifted_pixels_mask_u32(p, i, b)  FB_SHIFT_HIGH((p), ~(uint32_t)0, (i))
#define fb_shifted_pixels_mask_long(p, i, b) FB_SHIFT_HIGH((p), ~0UL, (i))
#define fb_compute_bswapmask(...)            0

#endif /* CONFIG_FB_CFB_REV_PIXELS_IN_BYTE */

#if BITS_PER_LONG == 64
#define le_long_to_cpu vmm_le64_to_cpu
#define cpu_to_le_long vmm_cpu_to_le64
#else
#define le_long_to_cpu vmm_le32_to_cpu
#define cpu_to_le_long vmm_cpu_to_le32
#endif

static inline uint64_t rolx(uint64_t word, uint32_t shift, uint32_t x)
{
    return (word << shift) | (word >> (x - shift));
}

#endif /* FB_DRAW_H */
