/*
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 2000, 2001, 2002 Andi Kleen, SuSE Labs
 */
#include <arch_regs.h>
#include <libs/kallsyms.h>
#include <libs/stack_trace.h>
#include <stack_trace.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_stdio.h>

extern uint8_t __x86_vmm_address(virtual_addr_t addr);

#define STACKSLOTS_PER_LINE 4

uint32_t code_bytes            = 64;
int      kstack_depth_to_print = 3 * STACKSLOTS_PER_LINE;

void print_address(uint64_t address, int reliable)
{
    vmm_printf(" [<%p>] %s%pB\n", (void *)address, reliable ? "" : "? ", (void *)address);
}

/*
 * x86-64 can have up to three kernel stacks:
 * process stack
 * interrupt stack
 * severe exception (double fault, nmi, stack fault, debug, mce) hardware stack
 *
 * FIXME: Need to add checks here based on where is called.
 */
static inline int valid_stack_ptr(void *p, uint32_t size, void *end)
{
    void *start = (void *)((uint64_t)end + 0x2000);

    if (p > end && p < start) {
        return 1;
    }

    return 0;
}

#if 0
uint64_t
print_context_stack(uint64_t *stack, uint64_t bp,
                    const struct stack_trace_ops *ops, void *data,
                    uint64_t *end)
{
    struct stack_frame *frame = (struct stack_frame *)bp;

    while (valid_stack_ptr(stack, sizeof(*stack), end)) {
        uint64_t addr;

        addr = *stack;

        if (__x86_vmm_address(addr)) {
            if ((uint64_t) stack == bp - sizeof(long)) {
                ops->address(data, addr, 1);
                frame = frame->next_frame;
                bp = (uint64_t) frame;
            } else {
                ops->address(data, addr, 0);
            }
        }

        stack++;
    }

    return bp;
}
#endif

uint64_t print_context_stack_bp(uint64_t *stack, uint64_t bp, const struct stack_trace_ops *ops, void *data, uint64_t *end)
{
    struct stack_frame *frame    = (struct stack_frame *)bp;
    uint64_t           *ret_addr = &frame->return_address;

    while (valid_stack_ptr(ret_addr, sizeof(*ret_addr), end)) {
        uint64_t addr = *ret_addr;

        if (!__x86_vmm_address(addr)) {
            break;
        }

        ops->address(data, addr, 1);
        frame    = frame->next_frame;
        ret_addr = &frame->return_address;
    }

    return (uint64_t)frame;
}

static int print_trace_stack(void *data, char *name)
{
    vmm_printf("%s <%s> ", (char *)data, name);
    return 0;
}

/*
 * Print one address/symbol entries per line.
 */
static void print_trace_address(void *data, uint64_t addr, int reliable)
{
    // vmm_printf(data);
    print_address(addr, reliable);
}

static const struct stack_trace_ops print_trace_ops = {
    .stack      = print_trace_stack,
    .address    = print_trace_address,
    .walk_stack = print_context_stack_bp,
};

void show_trace_log_lvl(struct arch_regs *regs, uint64_t *stack, uint64_t bp, char *log_lvl)
{
    vmm_printf("%sCall Trace:\n", log_lvl);
    dump_trace(regs, stack, bp, &print_trace_ops, log_lvl);
}

void show_trace(struct arch_regs *regs, uint64_t *stack, uint64_t bp)
{
    show_trace_log_lvl(regs, stack, bp, "");
}

#if 0
void show_stack(struct arch_regs *regs, uint64_t *sp)
{
    uint64_t bp = 0;

    bp = stack_frame(NULL, regs);
    show_stack_log_lvl(NULL, sp, bp, "");
}
#endif
