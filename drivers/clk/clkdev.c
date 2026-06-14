/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file clkdev.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief helper APIs for clk lookup
 *
 * Adapted from linux/drivers/clk/clkdev.c
 *
 *  Copyright (C) 2008 Russell King.
 *
 * Helper for the clk API to assist looking up a struct clk.
 *
 * The original source is licensed under GPL.
 */

#include <drv/clk-provider.h>
#include <drv/clk.h>
#include <drv/clkdev.h>
#include <libs/list.h>
#include <libs/stringlib.h>
#include <vmm_device_driver.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>

#include <stdarg.h>
#include "clk.h"

/* asm/clkdev.h --- start --- */
#ifndef CONFIG_COMMON_CLK
static inline int __clock_get(struct clk *clk)
{
    return 1;
}

static inline void __clock_put(struct clk *clk) {}
#endif

static inline struct clock_lookup_alloc *__clockdev_alloc(size_t size)
{
    return vmm_zalloc(size);
}

/* asm/clkdev.h --- finish --- */

static LIST_HEAD(clocks);
static DEFINE_SPINLOCK(clocks_slock);

#if defined(CONFIG_OF) && defined(CONFIG_COMMON_CLK)
static struct clk *__of_clock_get(vmm_device_tree_node_t *np, int index, const char *dev_id, const char *con_id)
{
    struct vmm_device_tree_phandle_args clkspec;
    struct clk                         *clk;
    int                                 rc;

    if (index < 0) {
        return VMM_ERR_RR_PTR(VMM_ERR_INVALID);
    }

    rc = vmm_device_tree_parse_phandle_with_args(np, "clocks", "#clock-cells", index, &clkspec);

    if (rc) {
        return VMM_ERR_RR_PTR(rc);
    }

    clk = __of_clock_get_from_provider(&clkspec, dev_id, con_id);
    vmm_device_tree_dref_node(clkspec.np);

    return clk;
}

struct clk *of_clock_get(vmm_device_tree_node_t *np, int index)
{
    return __of_clock_get(np, index, np->name, NULL);
}

VMM_ERR_XPORT_SYMBOL(of_clock_get);

static struct clk *__of_clock_get_by_name(vmm_device_tree_node_t *np, const char *dev_id, const char *name)
{
    struct clk *clk = VMM_ERR_RR_PTR(VMM_ERR_NOENT);

    /* Walk up the tree of devices looking for a clock that matches */
    while (np) {
        int index = 0;

        /*
         * For named clocks, first look up the name in the
         * "clock-names" property.  If it cannot be found, then
         * index will be an error code, and of_clock_get() will fail.
         */
        if (name) {
            index = vmm_device_tree_match_string(np, "clock-names", name);
        }

        clk = __of_clock_get(np, index, dev_id, name);

        if (!VMM_IS_ERR(clk)) {
            break;
        } else if (name && index >= 0) {
            if (VMM_PTR_ERR(clk) != VMM_ERR_PROBE_DEFER) {
                vmm_lerror(__func__, "could not get clock %pOF:%s(%i)\n", np, name ? name : "", index);
            }

            return clk;
        }

        /*
         * No matching clock found on this node.  If the parent node
         * has a "clock-ranges" property, then we can try one of its
         * clocks.
         */
        np = np->parent;

        if (np && !vmm_device_tree_getattr(np, "clock-ranges")) {
            break;
        }
    }

    return clk;
}

/**
 * of_clock_get_by_name() - Parse and lookup a clock referenced by a device node
 * @np: pointer to clock consumer node
 * @name: name of consumer's clock input, or NULL for the first clock reference
 *
 * This function parses the clocks and clock-names properties,
 * and uses them to look up the struct clk from the registered list of clock
 * providers.
 */
struct clk *of_clock_get_by_name(vmm_device_tree_node_t *np, const char *name)
{
    if (!np) {
        return VMM_ERR_RR_PTR(VMM_ERR_NOENT);
    }

    return __of_clock_get_by_name(np, np->name, name);
}

VMM_ERR_XPORT_SYMBOL(of_clock_get_by_name);

#else /* defined(CONFIG_OF) && defined(CONFIG_COMMON_CLK) */

static struct clk *__of_clock_get_by_name(vmm_device_tree_node_t *np, const char *dev_id, const char *name)
{
    return VMM_ERR_RR_PTR(VMM_ERR_NOENT);
}
#endif

/*
 * Find the correct struct clk for the device and connection ID.
 * We do slightly fuzzy matching here:
 *  An entry with a NULL ID is assumed to be a wildcard.
 *  If an entry has a device ID, it must match
 *  If an entry has a connection ID, it must match
 * Then we take the most specific entry - with the following
 * order of precedence: dev+con > dev only > con only.
 */
static struct clock_lookup *clock_find(const char *dev_id, const char *con_id)
{
    struct clock_lookup *p, *cl            = NULL;
    int                  match, best_found = 0, best_possible = 0;

    if (dev_id) {
        best_possible += 2;
    }

    if (con_id) {
        best_possible += 1;
    }

    list_for_each_entry(p, &clocks, node)
    {
        match = 0;

        if (p->dev_id) {
            if (!dev_id || strcmp(p->dev_id, dev_id)) {
                continue;
            }

            match += 2;
        }

        if (p->con_id) {
            if (!con_id || strcmp(p->con_id, con_id)) {
                continue;
            }

            match += 1;
        }

        if (match > best_found) {
            cl = p;

            if (match != best_possible) {
                best_found = match;
            } else {
                break;
            }
        }
    }
    return cl;
}

struct clk *clock_get_sys(const char *dev_id, const char *con_id)
{
    struct clock_lookup *cl;
    struct clk          *clk = NULL;

    vmm_spin_lock(&clocks_slock);

    cl = clock_find(dev_id, con_id);

    if (!cl) {
        goto out;
    }

    clk = __clock_create_clock(cl->clock_hw, dev_id, con_id);

    if (VMM_IS_ERR(clk)) {
        goto out;
    }

    if (!__clock_get(clk)) {
        __clock_free_clock(clk);
        cl = NULL;
        goto out;
    }

out:
    vmm_spin_unlock(&clocks_slock);

    return cl ? clk : VMM_ERR_RR_PTR(VMM_ERR_NOENT);
}

VMM_ERR_XPORT_SYMBOL(clock_get_sys);

struct clk *clock_get(vmm_device_t *dev, const char *con_id)
{
    const char *dev_id = dev ? dev->name : NULL;
    struct clk *clk;

    if (dev) {
        clk = __of_clock_get_by_name(dev->of_node, dev_id, con_id);

        if (!VMM_IS_ERR(clk) || VMM_PTR_ERR(clk) == VMM_ERR_PROBE_DEFER) {
            return clk;
        }
    }

    return clock_get_sys(dev_id, con_id);
}

VMM_ERR_XPORT_SYMBOL(clock_get);

void clock_put(struct clk *clk)
{
    __clock_put(clk);
}

VMM_ERR_XPORT_SYMBOL(clock_put);

static void __clockdev_add(struct clock_lookup *cl)
{
    vmm_spin_lock(&clocks_slock);
    list_add_tail(&cl->node, &clocks);
    vmm_spin_unlock(&clocks_slock);
}

void clkdev_add(struct clock_lookup *cl)
{
    if (!cl->clock_hw) {
        cl->clock_hw = __clock_get_hw(cl->clk);
    }

    __clockdev_add(cl);
}

VMM_ERR_XPORT_SYMBOL(clkdev_add);

void clkdev_add_table(struct clock_lookup *cl, size_t num)
{
    vmm_spin_lock(&clocks_slock);

    while (num--) {
        cl->clock_hw = __clock_get_hw(cl->clk);
        list_add_tail(&cl->node, &clocks);
        cl++;
    }

    vmm_spin_unlock(&clocks_slock);
}

#define MAX_DEVICE_ID 20
#define MAX_CON_ID    16

struct clock_lookup_alloc {
    struct clock_lookup cl;
    char                dev_id[MAX_DEVICE_ID];
    char                con_id[MAX_CON_ID];
};

static struct clock_lookup *vclkdev_alloc(struct clock_hw *hw, const char *con_id, const char *dev_fmt, va_list ap)
{
    struct clock_lookup_alloc *cla;

    cla = __clockdev_alloc(sizeof(*cla));

    if (!cla) {
        return NULL;
    }

    cla->cl.clock_hw = hw;

    if (con_id) {
        strlcpy(cla->con_id, con_id, sizeof(cla->con_id));
        cla->cl.con_id = cla->con_id;
    }

    if (dev_fmt) {
        __vmm_snprintf(cla->dev_id, sizeof(cla->dev_id), dev_fmt, ap);
        cla->cl.dev_id = cla->dev_id;
    }

    return &cla->cl;
}

static struct clock_lookup *vclkdev_create(struct clock_hw *hw, const char *con_id, const char *dev_fmt, va_list ap)
{
    struct clock_lookup *cl;

    cl = vclkdev_alloc(hw, con_id, dev_fmt, ap);

    if (cl) {
        __clockdev_add(cl);
    }

    return cl;
}

struct clock_lookup *clkdev_alloc(struct clk *clk, const char *con_id, const char *dev_fmt, ...)
{
    struct clock_lookup *cl;
    va_list              ap;

    va_start(ap, dev_fmt);
    cl = vclkdev_alloc(__clock_get_hw(clk), con_id, dev_fmt, ap);
    va_end(ap);

    return cl;
}

VMM_ERR_XPORT_SYMBOL(clkdev_alloc);

struct clock_lookup *clkdev_hw_alloc(struct clock_hw *hw, const char *con_id, const char *dev_fmt, ...)
{
    struct clock_lookup *cl;
    va_list              ap;

    va_start(ap, dev_fmt);
    cl = vclkdev_alloc(hw, con_id, dev_fmt, ap);
    va_end(ap);

    return cl;
}

VMM_ERR_XPORT_SYMBOL(clkdev_hw_alloc);

/**
 * clkdev_create - allocate and add a clkdev lookup structure
 * @clk: struct clk to associate with all clock_lookups
 * @con_id: connection ID string on device
 * @dev_fmt: format string describing device name
 *
 * Returns a clock_lookup structure, which can be later unregistered and
 * freed.
 */
struct clock_lookup *clkdev_create(struct clk *clk, const char *con_id, const char *dev_fmt, ...)
{
    struct clock_lookup *cl;
    va_list              ap;

    va_start(ap, dev_fmt);
    cl = vclkdev_create(__clock_get_hw(clk), con_id, dev_fmt, ap);
    va_end(ap);

    return cl;
}

VMM_ERR_XPORT_SYMBOL(clkdev_create);

/**
 * clkdev_hw_create - allocate and add a clkdev lookup structure
 * @hw: struct clock_hw to associate with all clock_lookups
 * @con_id: connection ID string on device
 * @dev_fmt: format string describing device name
 *
 * Returns a clock_lookup structure, which can be later unregistered and
 * freed.
 */
struct clock_lookup *clkdev_hw_create(struct clock_hw *hw, const char *con_id, const char *dev_fmt, ...)
{
    struct clock_lookup *cl;
    va_list              ap;

    va_start(ap, dev_fmt);
    cl = vclkdev_create(hw, con_id, dev_fmt, ap);
    va_end(ap);

    return cl;
}

VMM_ERR_XPORT_SYMBOL(clkdev_hw_create);

int clock_add_alias(const char *alias, const char *alias_dev_name, const char *con_id, vmm_device_t *dev)
{
    struct clk          *r = clock_get(dev, con_id);
    struct clock_lookup *l;

    if (VMM_IS_ERR(r)) {
        return VMM_PTR_ERR(r);
    }

    l = clkdev_create(r, alias, alias_dev_name ? "%s" : NULL, alias_dev_name);
    clock_put(r);

    return l ? 0 : VMM_ERR_NODEV;
}

VMM_ERR_XPORT_SYMBOL(clock_add_alias);

/*
 * clkdev_drop - remove a clock dynamically allocated
 */
void clkdev_drop(struct clock_lookup *cl)
{
    vmm_spin_lock(&clocks_slock);
    list_del(&cl->node);
    vmm_spin_unlock(&clocks_slock);
    vmm_free(cl);
}

VMM_ERR_XPORT_SYMBOL(clkdev_drop);

static struct clock_lookup *__clock_register_clockdev(struct clock_hw *hw, const char *con_id, const char *dev_id, ...)
{
    struct clock_lookup *cl;
    va_list              ap;

    va_start(ap, dev_id);
    cl = vclkdev_create(hw, con_id, dev_id, ap);
    va_end(ap);

    return cl;
}

/**
 * clock_register_clockdev - register one clock lookup for a struct clk
 * @clk: struct clk to associate with all clock_lookups
 * @con_id: connection ID string on device
 * @dev_id: string describing device name
 *
 * con_id or dev_id may be NULL as a wildcard, just as in the rest of
 * clkdev.
 *
 * To make things easier for mass registration, we detect error clks
 * from a previous clock_register() call, and return the error code for
 * those.  This is to permit this function to be called immediately
 * after clock_register().
 */
int clock_register_clockdev(struct clk *clk, const char *con_id, const char *dev_id)
{
    struct clock_lookup *cl;

    if (VMM_IS_ERR(clk)) {
        return VMM_PTR_ERR(clk);
    }

    /*
     * Since dev_id can be NULL, and NULL is handled specially, we must
     * pass it as either a NULL format string, or with "%s".
     */
    if (dev_id) {
        cl = __clock_register_clockdev(__clock_get_hw(clk), con_id, "%s", dev_id);
    } else {
        cl = __clock_register_clockdev(__clock_get_hw(clk), con_id, NULL);
    }

    return cl ? 0 : VMM_ERR_NOMEM;
}

VMM_ERR_XPORT_SYMBOL(clock_register_clockdev);

/**
 * clock_hw_register_clockdev - register one clock lookup for a struct clock_hw
 * @hw: struct clock_hw to associate with all clock_lookups
 * @con_id: connection ID string on device
 * @dev_id: format string describing device name
 *
 * con_id or dev_id may be NULL as a wildcard, just as in the rest of
 * clkdev.
 *
 * To make things easier for mass registration, we detect error clock_hws
 * from a previous clock_hw_register_*() call, and return the error code for
 * those.  This is to permit this function to be called immediately
 * after clock_hw_register_*().
 */
int clock_hw_register_clockdev(struct clock_hw *hw, const char *con_id, const char *dev_id)
{
    struct clock_lookup *cl;

    if (VMM_IS_ERR(hw)) {
        return VMM_PTR_ERR(hw);
    }

    /*
     * Since dev_id can be NULL, and NULL is handled specially, we must
     * pass it as either a NULL format string, or with "%s".
     */
    if (dev_id) {
        cl = __clock_register_clockdev(hw, con_id, "%s", dev_id);
    } else {
        cl = __clock_register_clockdev(hw, con_id, NULL);
    }

    return cl ? 0 : VMM_ERR_NOMEM;
}

VMM_ERR_XPORT_SYMBOL(clock_hw_register_clockdev);
