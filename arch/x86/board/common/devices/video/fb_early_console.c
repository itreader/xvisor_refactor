/*
 * Copyright (c) 2021 Himanshu Chauhan.
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
 */
/*!
 * @file fb_early_console.c
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief Early framebuffer console with only writing capabilities
 */

#include <brd_default_terminal.h>
#include <cpu_mmu.h>
#include <libs/stringlib.h>
#include <multiboot.h>
#include <video/fb_console.h>
#include <video/svga.h>
#include <video/ter-i16b.h>
#include <video/ter-i16n.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_host_io.h>
#include <vmm_params.h>
#include <vmm_types.h>

/* Pointers to fonts */
static uint8_t *font_reg, *font_bold;
/* Video mode info */
static uint16_t width, height, depth, bytesPerLine;
static void    *video_base;  //, video_size;
/* Cursor location (in text cells) */
static uint16_t col, row;
/* Set by escape sequences */
static uint8_t  fg_colour, bg_colour;
/* Used to parse escape codes */
static bool     next_char_is_escape_seq, is_bold;

extern struct multiboot_info boot_info;
extern int                   __create_bootstrap_page_table_entry(uint64_t va, uint64_t pa, uint32_t page_size, uint8_t wt, uint8_t cd);

#define VMM_ROUNDUP2_SIZE(_address, _size) ((_address & ~(_size - 1)) + _size)

/* Colour code -> 16bpp */
static uint32_t early_fb_console_col_map[16] = {0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,

                                                0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF};

static void early_memclr(void *area, uint32_t size)
{
    memset(area, 0, size);
}

static void early_fb_console_set_font(void *reg, void *bold)
{
    if (reg) {
        font_reg = reg;
    }

    if (bold) {
        font_bold = bold;
    }
}

static void early_fb_console_scroll_up(uint32_t num_rows)
{
    /* Copy rows upwards */
    uint8_t *read_ptr  = (uint8_t *)video_base + ((num_rows * CHAR_HEIGHT) * bytesPerLine);
    uint8_t *write_ptr = (uint8_t *)video_base;
    uint32_t num_bytes = (bytesPerLine * height) - (bytesPerLine * (num_rows * CHAR_HEIGHT));
    memcpy(write_ptr, read_ptr, num_bytes);

    /* Clear the rows at the end */
    read_ptr = (uint8_t *)(video_base + (bytesPerLine * height) - (bytesPerLine * (num_rows * CHAR_HEIGHT)));
    early_memclr(read_ptr, bytesPerLine * (num_rows * CHAR_HEIGHT));
}

static void early_fb_console_control(unsigned char c)
{
    switch (c) {
        case '\n':
            col = 0;

            /* We're on the last row, a newline should only scroll the viewport up */
            if (row == (height / CHAR_HEIGHT) - 1) {
                early_fb_console_scroll_up(1);
            } else {
                row++;
            }

            break;

        default:
            break;
    }
}

int early_fb_default_terminal_putc(unsigned char c)
{
    if (c == '\n') {
        early_fb_console_control('\n');
        return VMM_OK;
    }

    if (c == 0x01) {
        next_char_is_escape_seq = true;
        return VMM_OK;
    }

    /* Check if the following character is part of an escape sequence */
    if (next_char_is_escape_seq) {
        /* Codes 0x00 to 0x0F are colours */
        if (c >= 0x00 && c <= 0x0F) {
            fg_colour               = c;
            next_char_is_escape_seq = false;
        } else if (c == 0x10 || c == 0x11) {
            is_bold                 = (c == 0x11) ? true : false;

            next_char_is_escape_seq = false;
        } else {
            next_char_is_escape_seq = false;
        }
    } else { /* Handle printing of a regular character */
        /* Characters are 16 px tall, i.e. 0x10 bytes in stored rep */
        uint8_t  *read_ptr = (uint8_t *)((is_bold) ? font_bold : font_reg) + (c * CHAR_HEIGHT);
        uint32_t *write_base;

        const uint8_t x_to_bitmap[CHAR_WIDTH] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
        uint8_t       fontChar                = 0;
        uint32_t      out                     = 0;

        for (uint8_t y = 0; y < CHAR_HEIGHT; y++) {
            fontChar = read_ptr[y];

            /* Process one column at a time */
            if (depth == 4) {
                write_base = (uint32_t *)(video_base) + (((bytesPerLine / 4) * CHAR_HEIGHT * row)) + (CHAR_WIDTH * col);

                for (uint8_t x = 0; x < CHAR_WIDTH; x++) {
                    if (x_to_bitmap[x] & fontChar) {
                        write_base[x + (y * (bytesPerLine / 4))] = early_fb_console_col_map[fg_colour];
                    }
                }
            } else if (depth == 2) {
                write_base = (uint32_t *)(video_base) + (((bytesPerLine / 4) * CHAR_HEIGHT * row)) + ((CHAR_WIDTH * (col)) >> 1);

                /* In 16bpp, process two pixels at once */
                for (uint8_t x = 0; x < CHAR_WIDTH; x += 2) {
                    out = 0;

                    if (x_to_bitmap[x] & fontChar) {
                        out = ((SVGA_24TO16BPP(early_fb_console_col_map[fg_colour])) & 0xFFFF) << 16;
                    }

                    if (x_to_bitmap[x + 1] & fontChar) {
                        out |= ((SVGA_24TO16BPP(early_fb_console_col_map[fg_colour])) & 0xFFFF);
                    }

                    write_base[(x >> 1) + (y * (bytesPerLine / 4))] = out;
                }
            }
        }

        /* Increment column and check row */
        col++;

        if (col > (width / CHAR_WIDTH)) {
            early_fb_console_control('\n');
        }
    }

    return VMM_OK;
}

int init_early_fb_console(void)
{
    virtual_size_t fb_length;
    uint32_t       nr_pages = 0;

    bytesPerLine            = boot_info.framebuffer_pitch;
    width                   = boot_info.framebuffer_width;
    height                  = boot_info.framebuffer_height;
    depth                   = boot_info.framebuffer_bpp / 8;

    fb_length               = (bytesPerLine * height);
    fb_length               = VMM_ROUNDUP2_SIZE(fb_length, PAGE_SIZE_2M);
    video_base              = (void *)boot_info.framebuffer_addr;
    nr_pages                = fb_length / PAGE_SIZE_2M;
    fb_length               = (bytesPerLine * height);

    while (nr_pages) {
        if (__create_bootstrap_page_table_entry((uint64_t)video_base, (uint64_t)video_base, PAGE_SIZE_2M, 0, 1) != VMM_OK) {
            return VMM_EFAIL;
        }

        video_base += PAGE_SIZE_2M;
        nr_pages--;
    }

    video_base = (void *)boot_info.framebuffer_addr;

    early_fb_console_set_font(&ter_i16n_raw, &ter_i16b_raw);

    /* Clear screen */
    early_memclr((void *)video_base, bytesPerLine * height);

    is_bold                 = false;
    next_char_is_escape_seq = false;
    fg_colour               = 0x0F;
    bg_colour               = 0x00;

    col = row  = 0;

    early_putc = early_fb_default_terminal_putc;

    return VMM_OK;
}
