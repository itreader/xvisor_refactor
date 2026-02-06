/**
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
 *
 * @file vga.c
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief text-mode VGA console as default terminal.
 */

#include <brd_default_terminal.h>
#include <drv/input.h>
#include <libs/fifo.h>
#include <libs/video_terminal_emulate.h>
#include <video/vga.h>
#include <vmm_compiler.h>
#include <vmm_completion.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_host_io.h>
#include <vmm_params.h>
#include <vmm_types.h>

/*
 * These define our textpointer, our background and foreground
 * colors (attributes), and x and y cursor coordinates
 */
static uint16_t *textmemptr;
static uint32_t  attrib = 0x0E;
static uint32_t  csr_x = 0, csr_y = 0;
static char      esc_seq[16]   = {0};
static uint32_t  esc_seq_count = 0;

#if defined(CONFIG_VIDEO_TERMINAL_EMULATE)
static struct fifo         *default_terminal_fifo;
static vmm_completion_t     default_terminal_fifo_cmpl;
static uint32_t             default_terminal_key_flags;
static struct input_handler default_terminal_hndl;
static bool                 default_terminal_key_handler_registered;
#endif

static uint16_t *memsetw(uint16_t *dest, uint16_t val, size_t count)
{
    uint16_t *temp = (uint16_t *)dest;

    for (; count != 0; count--) {
        *temp++ = val;
    }

    return dest;
}

static void update_cursor(void)
{
    uint16_t pos = (csr_y * 80) + csr_x;

    /* cursor LOW port to vga INDEX register */
    vmm_outb(0x0F, 0x3D4);
    vmm_outb((uint8_t)(pos & 0xFF), 0x3D5);

    /* cursor HIGH port to vga INDEX register */
    vmm_outb(0x0E, 0x3D4);
    vmm_outb((uint8_t)((pos >> 8) & 0xFF), 0x3D5);
}

static void scroll_up_byline(void)
{
    uint8_t *dest, *src;
    uint32_t copylen;
    uint32_t blank, temp;

    /*
     * A blank is defined as a space... we need to give it
     * backcolor too
     */
    blank   = (0x20 | (attrib << 8));

    /*
     * Move the current text chunk that makes up the screen
     * back in the buffer by a line.
     */
    temp    = csr_y - 25 + 1;

    dest    = (uint8_t *)textmemptr;
    src     = (uint8_t *)(textmemptr + temp * 80);
    copylen = ((25 - temp) * 80 * 2);

    while (copylen) {
        *dest = *src;
        copylen--;
        dest++;
        src++;
    }

    /*
     * Finally, we set the chunk of memory that occupies
     *  the last line of text to our 'blank' character
     */
    memsetw(textmemptr + (25 - temp) * 80, blank, 80);
    csr_y = 25 - 1;
}

/* Scrolls the screen */
void scroll(void)
{
    /* Row 25 is the end, this means we need to scroll up */
    if (csr_y >= 25) {
        scroll_up_byline();
    }
}

/* Clears the screen */
static void cls()
{
    uint32_t i, blank;

    /*
     * Again, we need the 'short' that will be used to
     *  represent a space with color
     */
    blank = 0x20 | (attrib << 8);

    /* Sets the entire screen to spaces in our current color */
    for (i = 0; i < 25; i++) {
        memsetw(textmemptr + i * 80, blank, 80);
    }

    /*
     * Update out virtual cursor, and then move the
     *  hardware cursor
     */
    csr_x = 0;
    csr_y = 0;
}

/* Puts a single character on the screen */
int vga_putc(unsigned char c)
{
    uint16_t *where;
    uint32_t  att = attrib << 8;

    if (esc_seq_count) {
        esc_seq[esc_seq_count] = c;
        esc_seq_count++;

        if ((esc_seq_count == 2) && (esc_seq[1] == '[')) {
            /* Do nothing */
        } else if ((esc_seq_count == 3) && (esc_seq[1] == '[') && (esc_seq[2] == 'D')) {
            /* Move left */
            if (csr_x != 0) {
                csr_x--;
            }

            esc_seq_count = 0;
        } else if ((esc_seq_count == 3) && (esc_seq[1] == '[') && (esc_seq[2] == 'C')) {
            /* Move right */
            if (csr_x != 0) {
                csr_x++;
            }

            esc_seq_count = 0;
        } else {
            /* Ignore unknown escape sequences */
            esc_seq_count = 0;
        }

        goto done;
    }

    /* Handle a backspace, by moving the cursor back one space */
    if (c == '\e') {
        esc_seq_count = 1;
        esc_seq[0]    = '\e';
        goto done;
    } else if (c == '\b') {
        if (csr_x != 0) {
            csr_x--;
        }
    } else if (c == '\t') {
        /*
         * Handles a tab by incrementing the cursor's x, but only
         * to a point that will make it divisible by 8
         */
        csr_x = (csr_x + 8) & ~(8 - 1);
    } else if (c == '\r') {
        /*
         * Handles a 'Carriage Return', which simply brings the
         * cursor back to the margin
         */
        csr_x = 0;
    } else if (c == '\n') {
        /*
         * We handle our newlines the way DOS and the BIOS do: we
         *  treat it as if a 'CR' was also there, so we bring the
         *  cursor to the margin and we increment the 'y' value
         */
        csr_x = 0;
        csr_y++;
    } else if (c >= ' ') {
        /*
         * Any character greater than and including a space, is a
         *  printable character. The equation for finding the index
         *  in a linear chunk of memory can be represented by:
         *  Index = [(y * width) + x]
         */
        where  = textmemptr + (csr_y * 80 + csr_x);
        *where = c | att; /* Character AND attributes: color */
        csr_x++;
    }

    /*
     * If the cursor has reached the edge of the screen's width, we
     * insert a new line in there
     */
    if (csr_x >= 80) {
        csr_x = 0;
        csr_y++;
    }

done:
    /* Scroll the screen if needed, and finally move the cursor */
    scroll();

    /* Update cursor location */
    update_cursor();

    return 0;
}

/* Sets the forecolor and backcolor that we will use */
void vga_settextcolor(uint8_t forecolor, uint8_t backcolor)
{
    /*
     * Top 4 bytes are the background, bottom 4 bytes
     * are the foreground color
     */
    attrib = (backcolor << 4) | (forecolor & 0x0F);
}

/* Sets our text-mode VGA pointer, then clears the screen for us */
void init_vga_console(void)
{
    vga_settextcolor(15 /* White foreground */, 0 /* Black background */);
    textmemptr = (uint16_t *)vmm_host_iomap(0xB8000, 0x4000);
    cls();
}

int init_early_vga_console(void)
{
    vga_settextcolor(15 /* White foreground */, 0 /* Black background */);
    textmemptr = (uint16_t *)(0xB8000UL);
    cls();

    early_putc = vga_putc;

    return 0;
}

int arch_std_default_terminal_putc(uint8_t ch)
{
    vga_putc(ch);

    return VMM_OK;
}

#if defined(CONFIG_VIDEO_TERMINAL_EMULATE)
static int default_terminal_key_event(struct input_handler *ihnd, input_device_t *idev, uint32_t type, uint32_t code, int value)
{
    int      rc, i, len;
    char     str[16];
    uint32_t key_flags;

    if (value) { /* value=1 (key-up) or value=2 (auto-repeat) */
        /* Update input key flags */
        key_flags = video_terminal_emulate_key2flags(code);

        if ((key_flags & VIDEO_TERMINAL_EMULATE_KEYFLAG_LOCKS) && (default_terminal_key_flags & key_flags)) {
            default_terminal_key_flags &= ~key_flags;
        } else {
            default_terminal_key_flags |= key_flags;
        }

        /* Retrive input key string */
        rc = video_terminal_emulate_key2str(code, default_terminal_key_flags, str);

        if (rc) {
            return VMM_OK;
        }

        /* Add input key string to input buffer */
        len = strlen(str);

        for (i = 0; i < len; i++) {
            fifo_enqueue(default_terminal_fifo, &str[i], TRUE);
            vmm_completion_complete(&default_terminal_fifo_cmpl);
        }
    } else { /* value=0 (key-down) */
        /* Update input key flags */
        key_flags = video_terminal_emulate_key2flags(code);

        if (!(key_flags & VIDEO_TERMINAL_EMULATE_KEYFLAG_LOCKS)) {
            default_terminal_key_flags &= ~key_flags;
        }
    }

    return VMM_OK;
}

int arch_std_default_terminal_getc(uint8_t *ch)
{
    int rc;

    if (!default_terminal_key_handler_registered) {
        memset(&default_terminal_hndl, 0, sizeof(default_terminal_hndl));
        default_terminal_hndl.name = "default_terminal";
        default_terminal_hndl.evbit[0] |= BIT_MASK(EV_KEY);
        default_terminal_hndl.event   = default_terminal_key_event;
        default_terminal_hndl.private = NULL;

        rc                            = input_register_handler(&default_terminal_hndl);

        if (rc) {
            return rc;
        }

        rc = input_connect_handler(&default_terminal_hndl);

        if (rc) {
            return rc;
        }

        default_terminal_key_handler_registered = TRUE;
    }

    if (default_terminal_fifo) {
        /* Assume that we are always called from
         * Orphan (or Thread) context hence we can
         * sleep waiting for input characters.
         */
        vmm_completion_wait(&default_terminal_fifo_cmpl);

        /* Try to dequeue from default_terminal fifo */
        if (!fifo_dequeue(default_terminal_fifo, ch)) {
            return VMM_ENOTAVAIL;
        }

        return VMM_OK;
    }

    return VMM_EFAIL;
}

int __init arch_std_default_terminal_init(void)
{
    init_vga_console();

#if defined(CONFIG_VIDEO_TERMINAL_EMULATE)
    default_terminal_fifo = fifo_alloc(sizeof(uint8_t), 128);

    if (!default_terminal_fifo) {
        return VMM_ENOMEM;
    }

    INIT_COMPLETION(&default_terminal_fifo_cmpl);

    default_terminal_key_flags              = 0;
    default_terminal_key_handler_registered = FALSE;
#endif

    return VMM_OK;
}

static struct default_terminal_ops vga_ops = {
    .putc = arch_std_default_terminal_putc, .getc = arch_std_default_terminal_getc, .init = arch_std_default_terminal_init};

struct default_terminal_ops *get_vga_default_terminal_ops(void *data)
{
    return &vga_ops;
}
#else
static struct default_terminal_ops vga_ops = {.putc = arch_std_default_terminal_putc, .getc = NULL;
.init                                      = arch_std_default_terminal_init
}
;

struct default_terminal_ops *get_vga_default_terminal_ops(void)
{
    return &vga_ops
}
#endif
