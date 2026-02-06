/*
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 2000, 2001, 2002 Andi Kleen, SuSE Labs
 */

#ifndef _ASM_X86_STACKTRACE_H
#define _ASM_X86_STACKTRACE_H

#include <arch_regs.h>
#include <libs/kallsyms.h>
#include <libs/stack_trace.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_stdio.h>

extern int vmm_stack_depth_to_print;

#define STACKSLOTS_PER_LINE 4
#define IRQ_STACK_SIZE      0x1000UL
#define EXEC_STACK_SIZE     0x2000UL

struct stack_trace_ops;

typedef uint64_t (*walk_stack_t)(uint64_t *stack, uint64_t bp, const struct stack_trace_ops *ops, void *data, uint64_t *end);

extern uint64_t print_context_stack(uint64_t *stack, uint64_t bp, const struct stack_trace_ops *ops, void *data, uint64_t *end);

extern uint64_t print_context_stack_bp(uint64_t *stack, uint64_t bp, const struct stack_trace_ops *ops, void *data, uint64_t *end);

/* Generic stack tracer with callbacks */

struct stack_trace_ops {
    void (*address)(void *data, uint64_t address, int reliable);
    /* On negative return stop dumping */
    int (*stack)(void *data, char *name);
    walk_stack_t walk_stack;
};

void arch_save_stack_trace_regs(struct arch_regs *regs, struct stack_trace *trace);

void dump_trace(struct arch_regs *regs, uint64_t *stack, uint64_t bp, const struct stack_trace_ops *ops, void *data);

#define STACKSLOTS_PER_LINE 4
#define get_bp(bp)          asm("movq %%rbp, %0" : "=r"(bp) :)

static inline uint64_t stack_frame(struct arch_regs *regs)
{
    if (regs) {
        return regs->rbp;
    }

    return 0;
}

extern void show_trace_log_lvl(struct arch_regs *regs, uint64_t *stack, uint64_t bp, char *log_lvl);

extern void show_stack_log_lvl(struct arch_regs *regs, uint64_t *sp, uint64_t bp, char *log_lvl);

extern uint32_t code_bytes;

/* The form of the top of the frame on the stack */
struct stack_frame {
    struct stack_frame *next_frame;
    uint64_t            return_address;
};

static inline uint64_t caller_frame_pointer(void)
{
    struct stack_frame *frame;

    get_bp(frame);

    frame = frame->next_frame;

    return (uint64_t)frame;
}

#endif /* _ASM_X86_STACKTRACE_H */
