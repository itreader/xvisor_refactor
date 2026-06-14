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
 * @file fbcvt.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief VESA(TM) Coordinated Video Timings
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/video/fbcvt.c
 *
 * Copyright (C) 2005 Antonino Daplas <adaplas@pol.net>
 *
 *      Based from the VESA(TM) Coordinated Video Timing Generator by
 *      Graham Loveridge April 9, 2003 available at
 *      http://www.elo.utfsm.cl/~elo212/docs/CVTd6r1.xls
 *
 * The original code is licensed under the GPL.
 */

#include <drv/frame_buffer.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define FB_CVT_CELLSIZE           8
#define FB_CVT_GTF_C              40
#define FB_CVT_GTF_J              20
#define FB_CVT_GTF_K              128
#define FB_CVT_GTF_M              600
#define FB_CVT_MIN_VSYNC_BP       550
#define FB_CVT_MIN_VPORCH         3
#define FB_CVT_MIN_BPORCH         6

#define FB_CVT_RB_MIN_VBLANK      460
#define FB_CVT_RB_HBLANK          160
#define FB_CVT_RB_V_FPORCH        3

#define FB_CVT_FLAG_REDUCED_BLANK 1
#define FB_CVT_FLAG_MARGINS       2
#define FB_CVT_FLAG_INTERLACED    4

struct fb_cvt_data {
    uint32_t xres;
    uint32_t yres;
    uint32_t refresh;
    uint32_t f_refresh;
    uint32_t pixclock;
    uint32_t hperiod;
    uint32_t hblank;
    uint32_t hfreq;
    uint32_t htotal;
    uint32_t vtotal;
    uint32_t vsync;
    uint32_t hsync;
    uint32_t h_front_porch;
    uint32_t h_back_porch;
    uint32_t v_front_porch;
    uint32_t v_back_porch;
    uint32_t h_margin;
    uint32_t v_margin;
    uint32_t interlace;
    uint32_t aspect_ratio;
    uint32_t active_pixels;
    uint32_t flags;
    uint32_t status;
};

static const unsigned char fb_cvt_vbi_tab[] = {
    4, /* 4:3      */
    5, /* 16:9     */
    6, /* 16:10    */
    7, /* 5:4      */
    7, /* 15:9     */
    8, /* reserved */
    9, /* reserved */
    10 /* custom   */
};

/* returns hperiod * 1000 */
static uint32_t fb_cvt_hperiod(struct fb_cvt_data *cvt)
{
    uint32_t num = udiv32(1000000000, cvt->f_refresh);
    uint32_t den;

    if (cvt->flags & FB_CVT_FLAG_REDUCED_BLANK) {
        num -= FB_CVT_RB_MIN_VBLANK * 1000;
        den = 2 * (udiv32(cvt->yres, cvt->interlace) + 2 * cvt->v_margin);
    } else {
        num -= FB_CVT_MIN_VSYNC_BP * 1000;
        den = 2 * (udiv32(cvt->yres, cvt->interlace) + cvt->v_margin * 2 + FB_CVT_MIN_VPORCH + cvt->interlace / 2);
    }

    return 2 * udiv32(num, den);
}

/* returns ideal duty cycle * 1000 */
static uint32_t fb_cvt_ideal_duty_cycle(struct fb_cvt_data *cvt)
{
    uint32_t c_prime      = (FB_CVT_GTF_C - FB_CVT_GTF_J) * (FB_CVT_GTF_K) + 256 * FB_CVT_GTF_J;
    uint32_t m_prime      = (FB_CVT_GTF_K * FB_CVT_GTF_M);
    uint32_t h_period_est = cvt->hperiod;

    return (1000 * c_prime - ((m_prime * h_period_est) / 1000)) / 256;
}

static uint32_t fb_cvt_hblank(struct fb_cvt_data *cvt)
{
    uint32_t hblank = 0;

    if (cvt->flags & FB_CVT_FLAG_REDUCED_BLANK) {
        hblank = FB_CVT_RB_HBLANK;
    } else {
        uint32_t ideal_duty_cycle = fb_cvt_ideal_duty_cycle(cvt);
        uint32_t active_pixels    = cvt->active_pixels;

        if (ideal_duty_cycle < 20000) {
            hblank = (active_pixels * 20000) / (100000 - 20000);
        } else {
            hblank = udiv32((active_pixels * ideal_duty_cycle), (100000 - ideal_duty_cycle));
        }
    }

    hblank &= ~((2 * FB_CVT_CELLSIZE) - 1);

    return hblank;
}

static uint32_t fb_cvt_hsync(struct fb_cvt_data *cvt)
{
    uint32_t hsync;

    if (cvt->flags & FB_CVT_FLAG_REDUCED_BLANK) {
        hsync = 32;
    } else {
        hsync = (FB_CVT_CELLSIZE * cvt->htotal) / 100;
    }

    hsync &= ~(FB_CVT_CELLSIZE - 1);
    return hsync;
}

static uint32_t fb_cvt_vbi_lines(struct fb_cvt_data *cvt)
{
    uint32_t vbi_lines, min_vbi_lines, act_vbi_lines;

    if (cvt->flags & FB_CVT_FLAG_REDUCED_BLANK) {
        vbi_lines     = udiv32((1000 * FB_CVT_RB_MIN_VBLANK), cvt->hperiod) + 1;
        min_vbi_lines = FB_CVT_RB_V_FPORCH + cvt->vsync + FB_CVT_MIN_BPORCH;

    } else {
        vbi_lines     = udiv32((FB_CVT_MIN_VSYNC_BP * 1000), cvt->hperiod) + 1 + FB_CVT_MIN_VPORCH;
        min_vbi_lines = cvt->vsync + FB_CVT_MIN_BPORCH + FB_CVT_MIN_VPORCH;
    }

    if (vbi_lines < min_vbi_lines) {
        act_vbi_lines = min_vbi_lines;
    } else {
        act_vbi_lines = vbi_lines;
    }

    return act_vbi_lines;
}

static uint32_t fb_cvt_vtotal(struct fb_cvt_data *cvt)
{
    uint32_t vtotal = udiv32(cvt->yres, cvt->interlace);

    vtotal += 2 * cvt->v_margin + cvt->interlace / 2 + fb_cvt_vbi_lines(cvt);
    vtotal |= cvt->interlace / 2;

    return vtotal;
}

static uint32_t fb_cvt_pixclock(struct fb_cvt_data *cvt)
{
    uint32_t pixclock;

    if (cvt->flags & FB_CVT_FLAG_REDUCED_BLANK) {
        pixclock = (cvt->f_refresh * cvt->vtotal * cvt->htotal) / 1000;
    } else {
        pixclock = udiv32((cvt->htotal * 1000000), cvt->hperiod);
    }

    pixclock /= 250;
    pixclock *= 250;
    pixclock *= 1000;

    return pixclock;
}

static uint32_t fb_cvt_aspect_ratio(struct fb_cvt_data *cvt)
{
    uint32_t xres   = cvt->xres;
    uint32_t yres   = cvt->yres;
    uint32_t aspect = -1;

    if (xres == (yres * 4) / 3 && !((yres * 4) % 3)) {
        aspect = 0;
    } else if (xres == (yres * 16) / 9 && !((yres * 16) % 9)) {
        aspect = 1;
    } else if (xres == (yres * 16) / 10 && !((yres * 16) % 10)) {
        aspect = 2;
    } else if (xres == (yres * 5) / 4 && !((yres * 5) % 4)) {
        aspect = 3;
    } else if (xres == (yres * 15) / 9 && !((yres * 15) % 9)) {
        aspect = 4;
    } else {
        vmm_printf("fbcvt: Aspect ratio not CVT standard\n");
        aspect      = 7;
        cvt->status = 1;
    }

    return aspect;
}

static void fb_cvt_print_name(struct fb_cvt_data *cvt)
{
    uint32_t pixcount, pixcount_mod;
    int      cnt = 255, offset = 0, read = 0;
    char    *buf = vmm_zalloc(256);

    if (!buf) {
        return;
    }

    pixcount     = (cvt->xres * udiv32(cvt->yres, cvt->interlace)) / 1000000;
    pixcount_mod = (cvt->xres * udiv32(cvt->yres, cvt->interlace)) % 1000000;
    pixcount_mod /= 1000;

    read = vmm_snprintf(buf + offset, cnt, "fbcvt: %dx%d@%d: CVT Name - ", cvt->xres, cvt->yres, cvt->refresh);
    offset += read;
    cnt -= read;

    if (cvt->status) {
        vmm_snprintf(
            buf + offset, cnt,
            "Not a CVT standard - %d.%03d Mega "
            "Pixel Image\n",
            pixcount, pixcount_mod);
    } else {
        if (pixcount) {
            read = vmm_snprintf(buf + offset, cnt, "%d", pixcount);
            cnt -= read;
            offset += read;
        }

        read = vmm_snprintf(buf + offset, cnt, ".%03dM", pixcount_mod);
        cnt -= read;
        offset += read;

        if (cvt->aspect_ratio == 0) {
            read = vmm_snprintf(buf + offset, cnt, "3");
        } else if (cvt->aspect_ratio == 3) {
            read = vmm_snprintf(buf + offset, cnt, "4");
        } else if (cvt->aspect_ratio == 1 || cvt->aspect_ratio == 4) {
            read = vmm_snprintf(buf + offset, cnt, "9");
        } else if (cvt->aspect_ratio == 2) {
            read = vmm_snprintf(buf + offset, cnt, "A");
        } else {
            read = 0;
        }

        cnt -= read;
        offset += read;

        if (cvt->flags & FB_CVT_FLAG_REDUCED_BLANK) {
            read = vmm_snprintf(buf + offset, cnt, "-R");
            cnt -= read;
            offset += read;
        }
    }

    vmm_printf("%s\n", buf);
    vmm_free(buf);
}

static void fb_cvt_convert_to_mode(struct fb_cvt_data *cvt, struct fb_videomode *mode)
{
    mode->refresh      = cvt->f_refresh;
    mode->pixclock     = KHZ2PICOS(cvt->pixclock / 1000);
    mode->left_margin  = cvt->h_back_porch;
    mode->right_margin = cvt->h_front_porch;
    mode->hsync_len    = cvt->hsync;
    mode->upper_margin = cvt->v_back_porch;
    mode->lower_margin = cvt->v_front_porch;
    mode->vsync_len    = cvt->vsync;

    mode->sync &= ~(FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT);

    if (cvt->flags & FB_CVT_FLAG_REDUCED_BLANK) {
        mode->sync |= FB_SYNC_HOR_HIGH_ACT;
    } else {
        mode->sync |= FB_SYNC_VERT_HIGH_ACT;
    }
}

/*
 * Calculate mode using VESA(TM) CVT
 * @mode: pointer to fb_videomode; xres, yres, refresh and vmode must be
 *        pre-filled with the desired values
 * @margins: add margin to calculation (1.8% of xres and yres)
 * @rb: compute with reduced blanking (for flatpanels)
 *
 * RETURNS:
 * 0 for success
 * @mode is filled with computed values.  If interlaced, the refresh field
 * will be filled with the field rate (2x the frame rate)
 *
 * DESCRIPTION:
 * Computes video timings using VESA(TM) Coordinated Video Timings
 */
int fb_find_mode_cvt(struct fb_videomode *mode, int margins, int rb)
{
    struct fb_cvt_data cvt;

    memset(&cvt, 0, sizeof(cvt));

    if (margins) {
        cvt.flags |= FB_CVT_FLAG_MARGINS;
    }

    if (rb) {
        cvt.flags |= FB_CVT_FLAG_REDUCED_BLANK;
    }

    if (mode->vmode & FB_VMODE_INTERLACED) {
        cvt.flags |= FB_CVT_FLAG_INTERLACED;
    }

    cvt.xres      = mode->xres;
    cvt.yres      = mode->yres;
    cvt.refresh   = mode->refresh;
    cvt.f_refresh = cvt.refresh;
    cvt.interlace = 1;

    if (!cvt.xres || !cvt.yres || !cvt.refresh) {
        vmm_printf("fbcvt: Invalid input parameters\n");
        return 1;
    }

    if (!(cvt.refresh == 50 || cvt.refresh == 60 || cvt.refresh == 70 || cvt.refresh == 85)) {
        vmm_printf("fbcvt: Refresh rate not CVT standard\n");
        cvt.status = 1;
    }

    cvt.xres &= ~(FB_CVT_CELLSIZE - 1);

    if (cvt.flags & FB_CVT_FLAG_INTERLACED) {
        cvt.interlace = 2;
        cvt.f_refresh *= 2;
    }

    if (cvt.flags & FB_CVT_FLAG_REDUCED_BLANK) {
        if (cvt.refresh != 60) {
            vmm_printf("fbcvt: 60Hz refresh rate "
                       "advised for reduced blanking\n");
            cvt.status = 1;
        }
    }

    if (cvt.flags & FB_CVT_FLAG_MARGINS) {
        cvt.h_margin = (cvt.xres * 18) / 1000;
        cvt.h_margin &= ~(FB_CVT_CELLSIZE - 1);
        cvt.v_margin = (udiv32(cvt.yres, cvt.interlace) * 18) / 1000;
    }

    cvt.aspect_ratio  = fb_cvt_aspect_ratio(&cvt);
    cvt.active_pixels = cvt.xres + 2 * cvt.h_margin;
    cvt.hperiod       = fb_cvt_hperiod(&cvt);
    cvt.vsync         = fb_cvt_vbi_tab[cvt.aspect_ratio];
    cvt.vtotal        = fb_cvt_vtotal(&cvt);
    cvt.hblank        = fb_cvt_hblank(&cvt);
    cvt.htotal        = cvt.active_pixels + cvt.hblank;
    cvt.hsync         = fb_cvt_hsync(&cvt);
    cvt.pixclock      = fb_cvt_pixclock(&cvt);
    cvt.hfreq         = udiv32(cvt.pixclock, cvt.htotal);
    cvt.h_back_porch  = cvt.hblank / 2 + cvt.h_margin;
    cvt.h_front_porch = cvt.hblank - cvt.hsync - cvt.h_back_porch + 2 * cvt.h_margin;
    cvt.v_back_porch  = 3 + cvt.v_margin;
    cvt.v_front_porch = cvt.vtotal - udiv32(cvt.yres, cvt.interlace) - cvt.v_back_porch - cvt.vsync;
    fb_cvt_print_name(&cvt);
    fb_cvt_convert_to_mode(&cvt, mode);

    return 0;
}

VMM_ERR_XPORT_SYMBOL(fb_find_mode_cvt);
