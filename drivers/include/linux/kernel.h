#ifndef _LINUX_KERNEL_H
#define _LINUX_KERNEL_H

#include <libs/mathlib.h>
#include <vmm_limits.h>
#include <vmm_macros.h>
#include <vmm_stdio.h>

#include <stdarg.h>

#include <asm/atomic.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/string.h>

#define ARRAY_SIZE  array_size
#define ALIGN(x, y) align(x, y)

#define sprintf     vmm_sprintf

#define might_sleep()                                                                                                                                \
    do {                                                                                                                                             \
    } while (0);
#define might_sleep_if(cond)                                                                                                                         \
    do {                                                                                                                                             \
    } while (0);

#define simple_strtoul strtoul
#define simple_strtol  strtol

/* FIXME: This file just a place holder for most cases */

#endif /* _LINUX_KERNEL_H */
