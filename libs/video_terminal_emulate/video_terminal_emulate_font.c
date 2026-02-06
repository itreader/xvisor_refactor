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
 * @file video_terminal_emulate_font.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Video terminal emulation font database
 */

/*
 * linux/drivers/video/fonts.c -- `Soft' font definitions
 *
 *    Created 1995 by Geert Uytterhoeven
 *    Rewritten 1998 by Martin Mares <mj@ucw.cz>
 *
 *  2001 - Documented with DocBook
 *  - Brad Douglas <brad@neruo.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <libs/stringlib.h>
#include <libs/video_terminal_emulate_font.h>
#include <vmm_macros.h>
#include <vmm_modules.h>

#define VIDEO_TERMINAL_EMULATE_NO_FONTS

static const struct video_terminal_emulate_font *fonts[] = {
#ifdef CONFIG_VIDEO_TERMINAL_EMULATE_FONT_8x8
#undef VIDEO_TERMINAL_EMULATE_NO_FONTS
    &font_vga_8x8,
#endif
#ifdef CONFIG_VIDEO_TERMINAL_EMULATE_FONT_8x16
#undef VIDEO_TERMINAL_EMULATE_NO_FONTS
    &font_vga_8x16,
#endif
#ifdef CONFIG_VIDEO_TERMINAL_EMULATE_FONT_6x11
#undef VIDEO_TERMINAL_EMULATE_NO_FONTS
    &font_vga_6x11,
#endif
#ifdef CONFIG_VIDEO_TERMINAL_EMULATE_FONT_7x14
#undef VIDEO_TERMINAL_EMULATE_NO_FONTS
    &font_7x14,
#endif
#ifdef CONFIG_VIDEO_TERMINAL_EMULATE_FONT_SUN8x16
#undef VIDEO_TERMINAL_EMULATE_NO_FONTS
    &font_sun_8x16,
#endif
#ifdef CONFIG_VIDEO_TERMINAL_EMULATE_FONT_SUN12x22
#undef VIDEO_TERMINAL_EMULATE_NO_FONTS
    &font_sun_12x22,
#endif
#ifdef CONFIG_VIDEO_TERMINAL_EMULATE_FONT_10x18
#undef VIDEO_TERMINAL_EMULATE_NO_FONTS
    &font_10x18,
#endif
#ifdef CONFIG_VIDEO_TERMINAL_EMULATE_FONT_ACORN_8x8
#undef VIDEO_TERMINAL_EMULATE_NO_FONTS
    &font_acorn_8x8,
#endif
#ifdef CONFIG_VIDEO_TERMINAL_EMULATE_FONT_PEARL_8x8
#undef VIDEO_TERMINAL_EMULATE_NO_FONTS
    &font_pearl_8x8,
#endif
#ifdef CONFIG_VIDEO_TERMINAL_EMULATE_FONT_MINI_4x6
#undef VIDEO_TERMINAL_EMULATE_NO_FONTS
    &font_mini_4x6,
#endif
};

#define num_fonts array_size(fonts)

#ifdef VIDEO_TERMINAL_EMULATE_NO_FONTS
#error No fonts configured for video_terminal_emulate.
#endif

/**
 *  Find a font
 *  @name: string name of a font
 *
 *  Find a specified font with string name @name.
 *
 *  Returns %NULL if no font found, or a pointer to the
 *  specified font.
 *
 */

const struct video_terminal_emulate_font *video_terminal_emulate_find_font(const char *name)
{
    uint32_t i;

    for (i = 0; i < num_fonts; i++) {
        if (!strcmp(fonts[i]->name, name)) {
            return fonts[i];
        }
    }

    return NULL;
}

VMM_EXPORT_SYMBOL(video_terminal_emulate_find_font);

/**
 *  Get default font
 *  @xres: screen size of X
 *  @yres: screen size of Y
 *      @font_w: bit array of supported widths (1 - 32)
 *      @font_h: bit array of supported heights (1 - 32)
 *
 *  Get the default font for a specified screen size.
 *  Dimensions are in pixels.
 *
 *  Returns %NULL if no font is found, or a pointer to the
 *  chosen font.
 *
 */

const struct video_terminal_emulate_font *video_terminal_emulate_get_default_font(int xres, int yres, uint32_t font_w, uint32_t font_h)
{
    int                                       i, c, cc;
    const struct video_terminal_emulate_font *f, *g;

    g  = NULL;
    cc = -10000;

    for (i = 0; i < num_fonts; i++) {
        f = fonts[i];
        c = f->pref;
#if defined(__mc68000__)
#ifdef CONFIG_FONT_PEARL_8x8

        if (MACH_IS_AMIGA && f->idx == PEARL8x8_IDX) {
            c = 100;
        }

#endif
#ifdef CONFIG_FONT_6x11

        if (MACH_IS_MAC && xres < 640 && f->idx == VGA6x11_IDX) {
            c = 100;
        }

#endif
#endif

        if ((yres < 400) == (f->height <= 8)) {
            c += 1000;
        }

        if ((font_w & (1 << (f->width - 1))) && (font_h & (1 << (f->height - 1)))) {
            c += 1000;
        }

        if (c > cc) {
            cc = c;
            g  = f;
        }
    }

    return g;
}

VMM_EXPORT_SYMBOL(video_terminal_emulate_get_default_font);
