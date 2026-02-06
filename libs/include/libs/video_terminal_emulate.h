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
 * @file video_terminal_emulate.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Video terminal emulation library interface
 */

#ifndef __VIDEO_TERMINAL_EMULATE_H_
#define __VIDEO_TERMINAL_EMULATE_H_

#include <drv/frame_buffer.h>
#include <drv/input.h>
#include <libs/fifo.h>
#include <libs/video_terminal_emulate_font.h>
#include <vmm_char_device.h>
#include <vmm_completion.h>
#include <vmm_types.h>

#define VIDEO_TERMINAL_EMULATE_NAME_SIZE  VMM_CHARDEV_NAME_SIZE
#define VIDEO_TERMINAL_EMULATE_INBUF_SIZE 32
#define VIDEO_TERMINAL_EMULATE_ESCMD_SIZE (17 * 3)
#define VIDEO_TERMINAL_EMULATE_ESC_NPAR   (16)

typedef enum {
    VIDEO_TERMINAL_EMULATE_COLOR_BLACK,
    VIDEO_TERMINAL_EMULATE_COLOR_RED,
    VIDEO_TERMINAL_EMULATE_COLOR_GREEN,
    VIDEO_TERMINAL_EMULATE_COLOR_YELLOW,
    VIDEO_TERMINAL_EMULATE_COLOR_BLUE,
    VIDEO_TERMINAL_EMULATE_COLOR_MAGENTA,
    VIDEO_TERMINAL_EMULATE_COLOR_CYAN,
    VIDEO_TERMINAL_EMULATE_COLOR_WHITE,
} video_terminal_emulate_color;

#define VIDEO_TERMINAL_EMULATE_DEFAULT_FC VIDEO_TERMINAL_EMULATE_COLOR_WHITE
#define VIDEO_TERMINAL_EMULATE_DEFAULT_BC VIDEO_TERMINAL_EMULATE_COLOR_BLACK

struct video_terminal_emulate_cell {
    /* char value */
    uint8_t ch;

    /* cell location */
    uint32_t x, y;

    /* foreground color and background color */
    uint32_t fc, bc;
};

#define VIDEO_TERMINAL_EMULATE_KEYFLAG_LEFTCTRL   0x00000001
#define VIDEO_TERMINAL_EMULATE_KEYFLAG_RIGHTCTRL  0x00000002
#define VIDEO_TERMINAL_EMULATE_KEYFLAG_LEFTALT    0x00000004
#define VIDEO_TERMINAL_EMULATE_KEYFLAG_RIGHTALT   0x00000008
#define VIDEO_TERMINAL_EMULATE_KEYFLAG_LEFTSHIFT  0x00000010
#define VIDEO_TERMINAL_EMULATE_KEYFLAG_RIGHTSHIFT 0x00000020
#define VIDEO_TERMINAL_EMULATE_KEYFLAG_CAPSLOCK   0x00000040
#define VIDEO_TERMINAL_EMULATE_KEYFLAG_NUMLOCK    0x00000080
#define VIDEO_TERMINAL_EMULATE_KEYFLAG_SCROLLLOCK 0x00000100

#define VIDEO_TERMINAL_EMULATE_KEYFLAG_LOCKS \
    (VIDEO_TERMINAL_EMULATE_KEYFLAG_CAPSLOCK | VIDEO_TERMINAL_EMULATE_KEYFLAG_NUMLOCK | VIDEO_TERMINAL_EMULATE_KEYFLAG_SCROLLLOCK)

struct video_terminal_emulate {
    /* pseudo character device */
    vmm_char_device_t cdev;

    /* underlying input handler */
    struct input_handler hndl;

    /* underlying frame buffer */
    struct frame_buffer_info *info;

    /* video mode to be used */
    const struct fb_videomode *mode;

    /* variable screen info */
    struct frame_buffer_var_screeninfo var;

    /* color map to be used */
    struct frame_buffer_cmap cmap;

    /* fonts to be used */
    const struct video_terminal_emulate_font *font;
    uint32_t                                  font_img_sz;

    /* width and height */
    uint32_t w, h;

    /* current x, y */
    uint32_t x, y;
    uint32_t start_y;

    /* saved x, y */
    uint32_t saved_x, saved_y;

    /* current foreground color and background color */
    uint32_t fc, bc;

    /* saved fc, bc */
    uint32_t saved_fc, saved_bc;

    /* freeze state of video_terminal_emulate */
    bool freeze;

    /* screen data */
    struct video_terminal_emulate_cell *cell;
    uint32_t                            cell_head;
    uint32_t                            cell_tail;
    uint32_t                            cell_count;
    uint32_t                            cell_len;
    uint8_t                            *cursor_bkp;
    uint32_t                            cursor_bkp_size;
    uint8_t                             esc_cmd[VIDEO_TERMINAL_EMULATE_ESCMD_SIZE];
    uint16_t                            esc_attrib[VIDEO_TERMINAL_EMULATE_ESC_NPAR];
    uint8_t                             esc_cmd_count;
    uint8_t                             esc_attrib_count;
    bool                                esc_cmd_active;

    /* input data */
    struct fifo     *in_fifo;
    uint32_t         in_key_flags;
    vmm_completion_t in_done;
};

static inline struct frame_buffer_info *video_terminal_emulate_fbinfo(struct video_terminal_emulate *v)
{
    return (v) ? v->info : NULL;
}

static inline vmm_char_device_t *video_terminal_emulate_char_device(struct video_terminal_emulate *v)
{
    return (v) ? &v->cdev : NULL;
}

/* Get VIDEO_TERMINAL_EMULATE flags from input key code */
uint32_t video_terminal_emulate_key2flags(uint32_t code);

/* Get input characters based on input key code and VIDEO_TERMINAL_EMULATE flags */
int video_terminal_emulate_key2str(uint32_t code, uint32_t flags, char *out);

/* Create video_terminal_emulate instance */
struct video_terminal_emulate *video_terminal_emulate_create(const char *name, struct frame_buffer_info *info, const char *font_name);

/* Destroy video_terminal_emulate instance */
int video_terminal_emulate_destroy(struct video_terminal_emulate *v);

#endif /* __VIDEO_TERMINAL_EMULATE_H_ */
