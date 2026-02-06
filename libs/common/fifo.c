/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file fifo.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for generic first-in-first-out queue.
 */

#include <libs/fifo.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_heap.h>

struct fifo *fifo_alloc(uint32_t element_size, uint32_t element_count)
{
    struct fifo *f;

    if (!element_size || !element_count) {
        return NULL;
    }

    f = vmm_zalloc(sizeof(struct fifo));

    if (!f) {
        return NULL;
    }

    f->elements = vmm_zalloc(element_size * element_count);

    if (!f->elements) {
        vmm_free(f);
        return NULL;
    }

    f->element_size  = element_size;
    f->element_count = element_count;

    INIT_SPIN_LOCK(&f->lock);
    f->read_pos    = 0;
    f->write_pos   = 0;
    f->avail_count = 0;

    return f;
}

int fifo_free(struct fifo *f)
{
    vmm_free(f->elements);
    vmm_free(f);

    return VMM_OK;
}

#define __fifo_isempty(f) (((f)->avail_count) ? FALSE : TRUE)

bool fifo_isempty(struct fifo *f)
{
    bool        ret;
    irq_flags_t flags;

    if (!f) {
        return TRUE;
    }

    vmm_spin_lock_irq_save_lite(&f->lock, flags);
    ret = __fifo_isempty(f);
    vmm_spin_unlock_irq_restore_lite(&f->lock, flags);

    return ret;
}

#define __fifo_isfull(f) (((f)->avail_count >= (f)->element_count) ? TRUE : FALSE)

bool fifo_isfull(struct fifo *f)
{
    bool        ret;
    irq_flags_t flags;

    if (!f) {
        return FALSE;
    }

    vmm_spin_lock_irq_save_lite(&f->lock, flags);
    ret = __fifo_isfull(f);
    vmm_spin_unlock_irq_restore_lite(&f->lock, flags);

    return ret;
}

bool fifo_enqueue(struct fifo *f, void *src, bool overwrite)
{
    bool        ret = FALSE;
    irq_flags_t flags;

    if (!f || !src) {
        return FALSE;
    }

    vmm_spin_lock_irq_save_lite(&f->lock, flags);

    if (overwrite && __fifo_isfull(f)) {
        f->read_pos++;

        if (f->element_count <= f->read_pos) {
            f->read_pos = 0;
        }

        f->avail_count--;
    }

    if (!__fifo_isfull(f)) {
        switch (f->element_size) {
            case 1:
                *((uint8_t *)(f->elements + f->write_pos)) = *((uint8_t *)src);
                break;

            case 2:
                *((uint16_t *)(f->elements + (f->write_pos * 2))) = *((uint16_t *)src);
                break;

            case 4:
                *((uint32_t *)(f->elements + (f->write_pos * 4))) = *((uint32_t *)src);
                break;

            case 8:
                *((uint64_t *)(f->elements + (f->write_pos * 8))) = *((uint64_t *)src);
                break;

            default:
                memcpy(f->elements + (f->write_pos * f->element_size), src, f->element_size);
                break;
        };

        f->write_pos++;

        if (f->element_count <= f->write_pos) {
            f->write_pos = 0;
        }

        f->avail_count++;
        ret = TRUE;
    }

    vmm_spin_unlock_irq_restore_lite(&f->lock, flags);

    return ret;
}

bool fifo_dequeue(struct fifo *f, void *dst)
{
    bool        ret = FALSE;
    irq_flags_t flags;

    if (!f || !dst) {
        return FALSE;
    }

    vmm_spin_lock_irq_save_lite(&f->lock, flags);

    if (!__fifo_isempty(f)) {
        switch (f->element_size) {
            case 1:
                *((uint8_t *)dst) = *((uint8_t *)(f->elements + f->read_pos));
                break;

            case 2:
                *((uint16_t *)dst) = *((uint16_t *)(f->elements + (f->read_pos * 2)));
                break;

            case 4:
                *((uint32_t *)dst) = *((uint32_t *)(f->elements + (f->read_pos * 4)));
                break;

            case 8:
                *((uint64_t *)dst) = *((uint64_t *)(f->elements + (f->read_pos * 8)));
                break;

            default:
                memcpy(dst, f->elements + (f->read_pos * f->element_size), f->element_size);
                break;
        };

        f->read_pos++;

        if (f->element_count <= f->read_pos) {
            f->read_pos = 0;
        }

        f->avail_count--;
        ret = TRUE;
    }

    vmm_spin_unlock_irq_restore_lite(&f->lock, flags);

    return ret;
}

bool fifo_clear(struct fifo *f)
{
    irq_flags_t flags;

    if (!f) {
        return FALSE;
    }

    vmm_spin_lock_irq_save_lite(&f->lock, flags);

    f->read_pos    = 0;
    f->write_pos   = 0;
    f->avail_count = 0;

    vmm_spin_unlock_irq_restore_lite(&f->lock, flags);

    return TRUE;
}

bool fifo_getelement(struct fifo *f, uint32_t index, void *dst)
{
    irq_flags_t flags;

    if (!f || !dst) {
        return FALSE;
    }

    if (f->element_count <= index) {
        return FALSE;
    }

    vmm_spin_lock_irq_save_lite(&f->lock, flags);

    index = (f->read_pos + index);

    if (f->element_count <= index) {
        index -= f->element_count;
    }

    switch (f->element_size) {
        case 1:
            *((uint8_t *)dst) = *((uint8_t *)(f->elements + index));
            break;

        case 2:
            *((uint16_t *)dst) = *((uint16_t *)(f->elements + (index * 2)));
            break;

        case 4:
            *((uint32_t *)dst) = *((uint32_t *)(f->elements + (index * 4)));
            break;

        case 8:
            *((uint64_t *)dst) = *((uint64_t *)(f->elements + (index * 8)));
            break;

        default:
            memcpy(dst, f->elements + (index * f->element_size), f->element_size);
            break;
    };

    vmm_spin_unlock_irq_restore_lite(&f->lock, flags);

    return TRUE;
}

uint32_t fifo_avail(struct fifo *f)
{
    uint32_t    ret;
    irq_flags_t flags;

    if (!f) {
        return 0;
    }

    vmm_spin_lock_irq_save_lite(&f->lock, flags);
    ret = f->avail_count;
    vmm_spin_unlock_irq_restore_lite(&f->lock, flags);

    return ret;
}
