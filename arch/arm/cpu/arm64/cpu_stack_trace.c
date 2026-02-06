/**
 * Copyright (c) 2013 Sukanto Ghosh.
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
 * @file cpu_stack_trace.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief ARM64 specific function stack_trace.
 *
 * Portions of this file are derived from arch/arm/kernel/stack_trace.c
 * in linux source
 *
 */

#include <libs/kallsyms.h>
#include <libs/stack_trace.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_stdio.h>

struct stackframe {
    uint64_t fp;
    uint64_t sp;
    uint64_t lr;
    uint64_t pc;
};

/*
 * AArch64 PCS assigns the frame pointer to x29.
 *
 * A simple function prologue looks like this:
 *      sub     sp, sp, #0x10
 *      stp     x29, x30, [sp]
 *      mov     x29, sp
 *
 * A simple function epilogue looks like this:
 *      mov     sp, x29
 *      ldp     x29, x30, [sp]
 *      add     sp, sp, #0x10
 */
int unwind_frame(struct stackframe *frame)
{
    uint64_t low;
    uint64_t fp = frame->fp;

    low         = frame->sp;

    // if (fp < low ||  fp & 0xf)
    if (fp < low) {
        return -1;
    }

    frame->sp = fp + 0x10;
    frame->fp = *(uint64_t *)(fp);
    frame->pc = *(uint64_t *)(fp + 8);

    return 0;
}

void walk_stackframe(struct stackframe *frame, int (*fn)(struct stackframe *, void *), void *data)
{
    while (1) {
        int ret;

        if (fn(frame, data)) {
            break;
        }

        ret = unwind_frame(frame);

        if (ret < 0) {
            break;
        }
    }
}

struct stack_trace_data {
    struct stack_trace *trace;
    uint32_t            skip;
};

static int save_trace(struct stackframe *frame, void *d)
{
    struct stack_trace_data *data  = d;
    struct stack_trace      *trace = data->trace;
    uint64_t                 addr  = frame->pc;

    if (data->skip) {
        data->skip--;
        return 0;
    }

    trace->entries[trace->nr_entries++] = addr;

    return trace->nr_entries >= trace->max_entries;
}

void arch_save_stack_trace(struct stack_trace *trace)
{
    struct stack_trace_data data;
    struct stackframe       frame;

    data.trace = trace;
    data.skip  = trace->skip;

    register uint64_t current_sp asm("sp");

    frame.fp = (uint64_t)__builtin_frame_address(0);
    frame.sp = current_sp;
    frame.lr = (uint64_t)__builtin_return_address(0);
    frame.pc = (uint64_t)arch_save_stack_trace;

    walk_stackframe(&frame, save_trace, &data);
}
