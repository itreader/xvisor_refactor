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
 * @file clk.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief common clocking framework implementation
 *
 * Adapted from linux/drivers/clk/clk.c
 *
 * Copyright (C) 2010-2011 Canonical Ltd <jeremy.kerr@canonical.com>
 * Copyright (C) 2011-2012 Linaro Ltd <mturquette@linaro.org>
 *
 * The original source is licensed under GPL.
 */

#include <drv/clk-provider.h>
#include <drv/clk.h>
#include <drv/clk/clk-conf.h>
#include <libs/list.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <libs/xref.h>
#include <vmm_device_driver.h>
#include <vmm_device_resource.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_macros.h>
#include <vmm_modules.h>
#include <vmm_notifier.h>
#include <vmm_params.h>
#include <vmm_scheduler.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

#include <linux/clkdev.h>

#include "clk.h"

static DEFINE_SPINLOCK(enable_lock);
static DEFINE_SPINLOCK(prepare_lock);

static vmm_vcpu_t *prepare_owner;
static vmm_vcpu_t *enable_owner;

static int prepare_refcnt;
static int enable_refcnt;

static HLIST_HEAD(clock_root_list);
static HLIST_HEAD(clock_orphan_list);
static LIST_HEAD(clock_notifier_list);

/***    private data structures    ***/

struct clock_core {
    const char             *name;
    const struct clock_ops *ops;
    struct clock_hw        *hw;
    vmm_device_t           *dev;
    struct clock_core      *parent;
    const char            **parent_names;
    struct clock_core     **parents;
    uint8_t                 num_parents;
    uint8_t                 new_parent_index;
    uint64_t                rate;
    uint64_t                req_rate;
    uint64_t                new_rate;
    struct clock_core      *new_parent;
    struct clock_core      *new_child;
    uint64_t                flags;
    bool                    orphan;
    uint32_t                enable_count;
    uint32_t                prepare_count;
    uint64_t                min_rate;
    uint64_t                max_rate;
    uint64_t                accuracy;
    int                     phase;
    struct hlist_head       children;
    struct hlist_node       child_node;
    struct hlist_head       clks;
    uint32_t                notifier_count;
    struct xref             ref;
};

struct clk {
    struct clock_core *core;
    const char        *dev_id;
    const char        *con_id;
    uint64_t           min_rate;
    uint64_t           max_rate;
    struct hlist_node  clks_node;
};

#define lockdep_assert_held(x) BUG_ON(!vmm_spin_lock_check(x))

/***           runtime pm          ***/
static int pm_runtime_enabled(vmm_device_t *dev)
{
    return 1;
}

static inline int pm_runtime_get_sync(vmm_device_t *dev)
{
    return 0;
}

static inline int pm_runtime_put_sync(vmm_device_t *dev)
{
    return 0;
}

static inline int pm_runtime_put(vmm_device_t *dev)
{
    return 0;
}

static inline int pm_runtime_get_noresume(vmm_device_t *dev)
{
    return 0;
}

static inline int pm_runtime_active(vmm_device_t *dev)
{
    return 1;
}

static int clock_pm_runtime_get(struct clock_core *core)
{
    int ret = 0;

    if (!core->dev) {
        return 0;
    }

    ret = pm_runtime_get_sync(core->dev);
    return ret < 0 ? ret : 0;
}

static void clock_pm_runtime_put(struct clock_core *core)
{
    if (!core->dev) {
        return;
    }

    pm_runtime_put_sync(core->dev);
}

/***           locking             ***/
static void clock_prepare_lock(void)
{
    if (!vmm_spin_trylock(&prepare_lock)) {
        if (prepare_owner == vmm_scheduler_current_vcpu()) {
            prepare_refcnt++;
            return;
        }

        vmm_spin_lock(&prepare_lock);
    }

    WARN_ON(prepare_owner != NULL);
    WARN_ON(prepare_refcnt != 0);
    prepare_owner  = vmm_scheduler_current_vcpu();
    prepare_refcnt = 1;
}

static void clock_prepare_unlock(void)
{
    WARN_ON(prepare_owner != vmm_scheduler_current_vcpu());
    WARN_ON(prepare_refcnt == 0);

    if (--prepare_refcnt) {
        return;
    }

    prepare_owner = NULL;
    vmm_spin_unlock(&prepare_lock);
}

static uint64_t clock_enable_lock(void)
{
    uint64_t flags;

    if (!vmm_spin_trylock_irq_save(&enable_lock, flags)) {
        if (enable_owner == vmm_scheduler_current_vcpu()) {
            enable_refcnt++;
            return flags;
        }

        vmm_spin_lock_irq_save(&enable_lock, flags);
    }

    WARN_ON(enable_owner != NULL);
    WARN_ON(enable_refcnt != 0);
    enable_owner  = vmm_scheduler_current_vcpu();
    enable_refcnt = 1;
    return flags;
}

static void clock_enable_unlock(uint64_t flags)
{
    WARN_ON(enable_owner != vmm_scheduler_current_vcpu());
    WARN_ON(enable_refcnt == 0);

    if (--enable_refcnt) {
        return;
    }

    enable_owner = NULL;
    vmm_spin_unlock_irq_restore(&enable_lock, flags);
}

static bool clock_core_is_prepared(struct clock_core *core)
{
    bool ret = false;

    /*
     * .is_prepared is optional for clocks that can prepare
     * fall back to software usage counter if it is missing
     */
    if (!core->ops->is_prepared) {
        return core->prepare_count;
    }

    if (!clock_pm_runtime_get(core)) {
        ret = core->ops->is_prepared(core->hw);
        clock_pm_runtime_put(core);
    }

    return ret;
}

static bool clock_core_is_enabled(struct clock_core *core)
{
    bool ret = false;

    /*
     * .is_enabled is only mandatory for clocks that gate
     * fall back to software usage counter if .is_enabled is missing
     */
    if (!core->ops->is_enabled) {
        return core->enable_count;
    }

    /*
     * Check if clock controller's device is runtime active before
     * calling .is_enabled callback. If not, assume that clock is
     * disabled, because we might be called from atomic context, from
     * which pm_runtime_get() is not allowed.
     * This function is called mainly from clock_disable_unused_subtree,
     * which ensures proper runtime pm activation of controller before
     * taking enable spinlock, but the below check is needed if one tries
     * to call it from other places.
     */
    if (core->dev) {
        pm_runtime_get_noresume(core->dev);

        if (!pm_runtime_active(core->dev)) {
            ret = false;
            goto done;
        }
    }

    ret = core->ops->is_enabled(core->hw);
done:

    if (core->dev) {
        pm_runtime_put(core->dev);
    }

    return ret;
}

/***    helper functions   ***/

const char *__clock_get_name(const struct clk *clk)
{
    return !clk ? NULL : clk->core->name;
}

VMM_EXPORT_SYMBOL(__clock_get_name);

const char *clock_hw_get_name(const struct clock_hw *hw)
{
    return hw->core->name;
}

VMM_EXPORT_SYMBOL(clock_hw_get_name);

struct clock_hw *__clock_get_hw(struct clk *clk)
{
    return !clk ? NULL : clk->core->hw;
}

VMM_EXPORT_SYMBOL(__clock_get_hw);

uint32_t clock_hw_get_num_parents(const struct clock_hw *hw)
{
    return hw->core->num_parents;
}

VMM_EXPORT_SYMBOL(clock_hw_get_num_parents);

struct clock_hw *clock_hw_get_parent(const struct clock_hw *hw)
{
    return hw->core->parent ? hw->core->parent->hw : NULL;
}

VMM_EXPORT_SYMBOL(clock_hw_get_parent);

static struct clock_core *__clock_lookup_subtree(const char *name, struct clock_core *core)
{
    struct clock_core *child;
    struct clock_core *ret;

    if (!strcmp(core->name, name)) {
        return core;
    }

    hlist_for_each_entry(child, &core->children, child_node)
    {
        ret = __clock_lookup_subtree(name, child);

        if (ret) {
            return ret;
        }
    }

    return NULL;
}

static struct clock_core *clock_core_lookup(const char *name)
{
    struct clock_core *root_clock;
    struct clock_core *ret;

    if (!name) {
        return NULL;
    }

    /* search the 'proper' clk tree first */
    hlist_for_each_entry(root_clock, &clock_root_list, child_node)
    {
        ret = __clock_lookup_subtree(name, root_clock);

        if (ret) {
            return ret;
        }
    }

    /* if not found, then search the orphan tree */
    hlist_for_each_entry(root_clock, &clock_orphan_list, child_node)
    {
        ret = __clock_lookup_subtree(name, root_clock);

        if (ret) {
            return ret;
        }
    }

    return NULL;
}

static struct clock_core *clock_core_get_parent_by_index(struct clock_core *core, uint8_t index)
{
    if (!core || index >= core->num_parents) {
        return NULL;
    }

    if (!core->parents[index]) {
        core->parents[index] = clock_core_lookup(core->parent_names[index]);
    }

    return core->parents[index];
}

struct clock_hw *clock_hw_get_parent_by_index(const struct clock_hw *hw, uint32_t index)
{
    struct clock_core *parent;

    parent = clock_core_get_parent_by_index(hw->core, index);

    return !parent ? NULL : parent->hw;
}

VMM_EXPORT_SYMBOL(clock_hw_get_parent_by_index);

uint32_t __clock_get_enable_count(struct clk *clk)
{
    return !clk ? 0 : clk->core->enable_count;
}

static uint64_t clock_core_get_rate_nolock(struct clock_core *core)
{
    uint64_t ret;

    if (!core) {
        ret = 0;
        goto out;
    }

    ret = core->rate;

    if (!core->num_parents) {
        goto out;
    }

    if (!core->parent) {
        ret = 0;
    }

out:
    return ret;
}

uint64_t clock_hw_get_rate(const struct clock_hw *hw)
{
    return clock_core_get_rate_nolock(hw->core);
}

VMM_EXPORT_SYMBOL(clock_hw_get_rate);

static uint64_t __clock_get_accuracy(struct clock_core *core)
{
    if (!core) {
        return 0;
    }

    return core->accuracy;
}

uint64_t __clock_get_flags(struct clk *clk)
{
    return !clk ? 0 : clk->core->flags;
}

VMM_EXPORT_SYMBOL(__clock_get_flags);

uint64_t clock_hw_get_flags(const struct clock_hw *hw)
{
    return hw->core->flags;
}

VMM_EXPORT_SYMBOL(clock_hw_get_flags);

bool clock_hw_is_prepared(const struct clock_hw *hw)
{
    return clock_core_is_prepared(hw->core);
}

bool clock_hw_is_enabled(const struct clock_hw *hw)
{
    return clock_core_is_enabled(hw->core);
}

bool __clock_is_enabled(struct clk *clk)
{
    if (!clk) {
        return false;
    }

    return clock_core_is_enabled(clk->core);
}

VMM_EXPORT_SYMBOL(__clock_is_enabled);

static bool mux_is_better_rate(uint64_t rate, uint64_t now, uint64_t best, uint64_t flags)
{
    if (flags & CLK_MUX_ROUND_CLOSEST) {
        return abs(now - rate) < abs(best - rate);
    }

    return now <= rate && now > best;
}

static int clock_mux_determine_rate_flags(struct clock_hw *hw, struct clock_rate_request *req, uint64_t flags)
{
    struct clock_core        *core = hw->core, *parent, *best_parent = NULL;
    int                       i, num_parents, ret;
    uint64_t                  best       = 0;
    struct clock_rate_request parent_req = *req;

    /* if NO_REPARENT flag set, pass through to current parent */
    if (core->flags & CLK_SET_RATE_NO_REPARENT) {
        parent = core->parent;

        if (core->flags & CLK_SET_RATE_PARENT) {
            ret = __clock_determine_rate(parent ? parent->hw : NULL, &parent_req);

            if (ret) {
                return ret;
            }

            best = parent_req.rate;
        } else if (parent) {
            best = clock_core_get_rate_nolock(parent);
        } else {
            best = clock_core_get_rate_nolock(core);
        }

        goto out;
    }

    /* find the parent that can provide the fastest rate <= rate */
    num_parents = core->num_parents;

    for (i = 0; i < num_parents; i++) {
        parent = clock_core_get_parent_by_index(core, i);

        if (!parent) {
            continue;
        }

        if (core->flags & CLK_SET_RATE_PARENT) {
            parent_req = *req;
            ret        = __clock_determine_rate(parent->hw, &parent_req);

            if (ret) {
                continue;
            }
        } else {
            parent_req.rate = clock_core_get_rate_nolock(parent);
        }

        if (mux_is_better_rate(req->rate, parent_req.rate, best, flags)) {
            best_parent = parent;
            best        = parent_req.rate;
        }
    }

    if (!best_parent) {
        return VMM_EINVALID;
    }

out:

    if (best_parent) {
        req->best_parent_hw = best_parent->hw;
    }

    req->best_parent_rate = best;
    req->rate             = best;

    return 0;
}

struct clk *__clock_lookup(const char *name)
{
    struct clock_core *core = clock_core_lookup(name);

    return !core ? NULL : core->hw->clk;
}

static void clock_core_get_boundaries(struct clock_core *core, uint64_t *min_rate, uint64_t *max_rate)
{
    struct clk *clock_user;

    *min_rate                                                          = core->min_rate;
    *max_rate                                                          = core->max_rate;

    hlist_for_each_entry(clock_user, &core->clks, clks_node) *min_rate = max(*min_rate, clock_user->min_rate);

    hlist_for_each_entry(clock_user, &core->clks, clks_node) *max_rate = min(*max_rate, clock_user->max_rate);
}

void clock_hw_set_rate_range(struct clock_hw *hw, uint64_t min_rate, uint64_t max_rate)
{
    hw->core->min_rate = min_rate;
    hw->core->max_rate = max_rate;
}

VMM_EXPORT_SYMBOL(clock_hw_set_rate_range);

/*
 * Helper for finding best parent to provide a given frequency. This can be used
 * directly as a determine_rate callback (e.g. for a mux), or from a more
 * complex clock that may combine a mux with other operations.
 */
int __clock_mux_determine_rate(struct clock_hw *hw, struct clock_rate_request *req)
{
    return clock_mux_determine_rate_flags(hw, req, 0);
}

VMM_EXPORT_SYMBOL(__clock_mux_determine_rate);

int __clock_mux_determine_rate_closest(struct clock_hw *hw, struct clock_rate_request *req)
{
    return clock_mux_determine_rate_flags(hw, req, CLK_MUX_ROUND_CLOSEST);
}

VMM_EXPORT_SYMBOL(__clock_mux_determine_rate_closest);

/***        clk api        ***/

static void clock_core_unprepare(struct clock_core *core)
{
    lockdep_assert_held(&prepare_lock);

    if (!core) {
        return;
    }

    if (WARN_ON(core->prepare_count == 0)) {
        return;
    }

    if (WARN_ON(core->prepare_count == 1 && core->flags & CLK_IS_CRITICAL)) {
        return;
    }

    if (--core->prepare_count > 0) {
        return;
    }

    WARN_ON(core->enable_count > 0);

    if (core->ops->unprepare) {
        core->ops->unprepare(core->hw);
    }

    clock_pm_runtime_put(core);

    clock_core_unprepare(core->parent);
}

static void clock_core_unprepare_lock(struct clock_core *core)
{
    clock_prepare_lock();
    clock_core_unprepare(core);
    clock_prepare_unlock();
}

/**
 * clock_unprepare - undo preparation of a clock source
 * @clk: the clk being unprepared
 *
 * clock_unprepare may sleep, which differentiates it from clock_disable.  In a
 * simple case, clock_unprepare can be used instead of clock_disable to gate a clk
 * if the operation may sleep.  One example is a clk which is accessed over
 * I2c.  In the complex case a clk gate operation may require a fast and a slow
 * part.  It is this reason that clock_unprepare and clock_disable are not mutually
 * exclusive.  In fact clock_disable must be called before clock_unprepare.
 */
void clock_unprepare(struct clk *clk)
{
    if (VMM_IS_ERR_OR_NULL(clk)) {
        return;
    }

    clock_core_unprepare_lock(clk->core);
}

VMM_EXPORT_SYMBOL(clock_unprepare);

static int clock_core_prepare(struct clock_core *core)
{
    int ret = 0;

    lockdep_assert_held(&prepare_lock);

    if (!core) {
        return 0;
    }

    if (core->prepare_count == 0) {
        ret = clock_pm_runtime_get(core);

        if (ret) {
            return ret;
        }

        ret = clock_core_prepare(core->parent);

        if (ret) {
            goto runtime_put;
        }

        if (core->ops->prepare) {
            ret = core->ops->prepare(core->hw);
        }

        if (ret) {
            goto unprepare;
        }
    }

    core->prepare_count++;

    return 0;
unprepare:
    clock_core_unprepare(core->parent);
runtime_put:
    clock_pm_runtime_put(core);
    return ret;
}

static int clock_core_prepare_lock(struct clock_core *core)
{
    int ret;

    clock_prepare_lock();
    ret = clock_core_prepare(core);
    clock_prepare_unlock();

    return ret;
}

/**
 * clock_prepare - prepare a clock source
 * @clk: the clk being prepared
 *
 * clock_prepare may sleep, which differentiates it from clock_enable.  In a simple
 * case, clock_prepare can be used instead of clock_enable to ungate a clk if the
 * operation may sleep.  One example is a clk which is accessed over I2c.  In
 * the complex case a clk ungate operation may require a fast and a slow part.
 * It is this reason that clock_prepare and clock_enable are not mutually
 * exclusive.  In fact clock_prepare must be called before clock_enable.
 * Returns 0 on success, -EERROR otherwise.
 */
int clock_prepare(struct clk *clk)
{
    if (!clk) {
        return 0;
    }

    return clock_core_prepare_lock(clk->core);
}

VMM_EXPORT_SYMBOL(clock_prepare);

static void clock_core_disable(struct clock_core *core)
{
    lockdep_assert_held(&enable_lock);

    if (!core) {
        return;
    }

    if (WARN_ON(core->enable_count == 0)) {
        return;
    }

    if (WARN_ON(core->enable_count == 1 && core->flags & CLK_IS_CRITICAL)) {
        return;
    }

    if (--core->enable_count > 0) {
        return;
    }

    if (core->ops->disable) {
        core->ops->disable(core->hw);
    }

    clock_core_disable(core->parent);
}

static void clock_core_disable_lock(struct clock_core *core)
{
    uint64_t flags;

    flags = clock_enable_lock();
    clock_core_disable(core);
    clock_enable_unlock(flags);
}

/**
 * clock_disable - gate a clock
 * @clk: the clk being gated
 *
 * clock_disable must not sleep, which differentiates it from clock_unprepare.  In
 * a simple case, clock_disable can be used instead of clock_unprepare to gate a
 * clk if the operation is fast and will never sleep.  One example is a
 * SoC-internal clk which is controlled via simple register writes.  In the
 * complex case a clk gate operation may require a fast and a slow part.  It is
 * this reason that clock_unprepare and clock_disable are not mutually exclusive.
 * In fact clock_disable must be called before clock_unprepare.
 */
void clock_disable(struct clk *clk)
{
    if (VMM_IS_ERR_OR_NULL(clk)) {
        return;
    }

    clock_core_disable_lock(clk->core);
}

VMM_EXPORT_SYMBOL(clock_disable);

static int clock_core_enable(struct clock_core *core)
{
    int ret = 0;

    lockdep_assert_held(&enable_lock);

    if (!core) {
        return 0;
    }

    if (WARN_ON(core->prepare_count == 0)) {
        return VMM_ESHUTDOWN;
    }

    if (core->enable_count == 0) {
        ret = clock_core_enable(core->parent);

        if (ret) {
            return ret;
        }

        if (core->ops->enable) {
            ret = core->ops->enable(core->hw);
        }

        if (ret) {
            clock_core_disable(core->parent);
            return ret;
        }
    }

    core->enable_count++;
    return 0;
}

static int clock_core_enable_lock(struct clock_core *core)
{
    uint64_t flags;
    int      ret;

    flags = clock_enable_lock();
    ret   = clock_core_enable(core);
    clock_enable_unlock(flags);

    return ret;
}

/**
 * clock_enable - ungate a clock
 * @clk: the clk being ungated
 *
 * clock_enable must not sleep, which differentiates it from clock_prepare.  In a
 * simple case, clock_enable can be used instead of clock_prepare to ungate a clk
 * if the operation will never sleep.  One example is a SoC-internal clk which
 * is controlled via simple register writes.  In the complex case a clk ungate
 * operation may require a fast and a slow part.  It is this reason that
 * clock_enable and clock_prepare are not mutually exclusive.  In fact clock_prepare
 * must be called before clock_enable.  Returns 0 on success, -EERROR
 * otherwise.
 */
int clock_enable(struct clk *clk)
{
    if (!clk) {
        return 0;
    }

    return clock_core_enable_lock(clk->core);
}

VMM_EXPORT_SYMBOL(clock_enable);

static int clock_core_prepare_enable(struct clock_core *core)
{
    int ret;

    ret = clock_core_prepare_lock(core);

    if (ret) {
        return ret;
    }

    ret = clock_core_enable_lock(core);

    if (ret) {
        clock_core_unprepare_lock(core);
    }

    return ret;
}

static void clock_core_disable_unprepare(struct clock_core *core)
{
    clock_core_disable_lock(core);
    clock_core_unprepare_lock(core);
}

static void clock_unprepare_unused_subtree(struct clock_core *core)
{
    struct clock_core *child;

    lockdep_assert_held(&prepare_lock);

    hlist_for_each_entry(child, &core->children, child_node) clock_unprepare_unused_subtree(child);

    if (core->prepare_count) {
        return;
    }

    if (core->flags & CLK_IGNORE_UNUSED) {
        return;
    }

    if (clock_pm_runtime_get(core)) {
        return;
    }

    if (clock_core_is_prepared(core)) {
        if (core->ops->unprepare_unused) {
            core->ops->unprepare_unused(core->hw);
        } else if (core->ops->unprepare) {
            core->ops->unprepare(core->hw);
        }
    }

    clock_pm_runtime_put(core);
}

static void clock_disable_unused_subtree(struct clock_core *core)
{
    struct clock_core *child;
    uint64_t           flags;

    lockdep_assert_held(&prepare_lock);

    hlist_for_each_entry(child, &core->children, child_node) clock_disable_unused_subtree(child);

    if (core->flags & CLK_OPS_PARENT_ENABLE) {
        clock_core_prepare_enable(core->parent);
    }

    if (clock_pm_runtime_get(core)) {
        goto unprepare_out;
    }

    flags = clock_enable_lock();

    if (core->enable_count) {
        goto unlock_out;
    }

    if (core->flags & CLK_IGNORE_UNUSED) {
        goto unlock_out;
    }

    /*
     * some gate clocks have special needs during the disable-unused
     * sequence.  call .disable_unused if available, otherwise fall
     * back to .disable
     */
    if (clock_core_is_enabled(core)) {
        if (core->ops->disable_unused) {
            core->ops->disable_unused(core->hw);
        } else if (core->ops->disable) {
            core->ops->disable(core->hw);
        }
    }

unlock_out:
    clock_enable_unlock(flags);
    clock_pm_runtime_put(core);
unprepare_out:

    if (core->flags & CLK_OPS_PARENT_ENABLE) {
        clock_core_disable_unprepare(core->parent);
    }
}

static bool clock_ignore_unused;

static int __init clock_ignore_unused_setup(char *__not_used)
{
    clock_ignore_unused = true;
    return VMM_OK;
}

vmm_early_param("vmm.clock_ignore_unused", clock_ignore_unused_setup);

int clock_disable_unused(void)
{
    struct clock_core *core;

    if (clock_ignore_unused) {
        vmm_lwarning("clk", "Not disabling unused clocks\n");
        return 0;
    }

    clock_prepare_lock();

    hlist_for_each_entry(core, &clock_root_list, child_node) clock_disable_unused_subtree(core);

    hlist_for_each_entry(core, &clock_orphan_list, child_node) clock_disable_unused_subtree(core);

    hlist_for_each_entry(core, &clock_root_list, child_node) clock_unprepare_unused_subtree(core);

    hlist_for_each_entry(core, &clock_orphan_list, child_node) clock_unprepare_unused_subtree(core);

    clock_prepare_unlock();

    return 0;
}

VMM_EXPORT_SYMBOL(clock_disable_unused);

static int clock_core_round_rate_nolock(struct clock_core *core, struct clock_rate_request *req)
{
    struct clock_core *parent;
    long               rate;

    lockdep_assert_held(&prepare_lock);

    if (!core) {
        return 0;
    }

    parent = core->parent;

    if (parent) {
        req->best_parent_hw   = parent->hw;
        req->best_parent_rate = parent->rate;
    } else {
        req->best_parent_hw   = NULL;
        req->best_parent_rate = 0;
    }

    if (core->ops->determine_rate) {
        return core->ops->determine_rate(core->hw, req);
    } else if (core->ops->round_rate) {
        rate = core->ops->round_rate(core->hw, req->rate, &req->best_parent_rate);

        if (rate < 0) {
            return rate;
        }

        req->rate = rate;
    } else if (core->flags & CLK_SET_RATE_PARENT) {
        return clock_core_round_rate_nolock(parent, req);
    } else {
        req->rate = core->rate;
    }

    return 0;
}

/**
 * __clock_determine_rate - get the closest rate actually supported by a clock
 * @hw: determine the rate of this clock
 * @req: target rate request
 *
 * Useful for clock_ops such as .set_rate and .determine_rate.
 */
int __clock_determine_rate(struct clock_hw *hw, struct clock_rate_request *req)
{
    if (!hw) {
        req->rate = 0;
        return 0;
    }

    return clock_core_round_rate_nolock(hw->core, req);
}

VMM_EXPORT_SYMBOL(__clock_determine_rate);

uint64_t clock_hw_round_rate(struct clock_hw *hw, uint64_t rate)
{
    int                       ret;
    struct clock_rate_request req;

    clock_core_get_boundaries(hw->core, &req.min_rate, &req.max_rate);
    req.rate = rate;

    ret      = clock_core_round_rate_nolock(hw->core, &req);

    if (ret) {
        return 0;
    }

    return req.rate;
}

VMM_EXPORT_SYMBOL(clock_hw_round_rate);

/**
 * clock_round_rate - round the given rate for a clk
 * @clk: the clk for which we are rounding a rate
 * @rate: the rate which is to be rounded
 *
 * Takes in a rate as input and rounds it to a rate that the clk can actually
 * use which is then returned.  If clk doesn't support round_rate operation
 * then the parent rate is returned.
 */
long clock_round_rate(struct clk *clk, uint64_t rate)
{
    struct clock_rate_request req;
    int                       ret;

    if (!clk) {
        return 0;
    }

    clock_prepare_lock();

    clock_core_get_boundaries(clk->core, &req.min_rate, &req.max_rate);
    req.rate = rate;

    ret      = clock_core_round_rate_nolock(clk->core, &req);
    clock_prepare_unlock();

    if (ret) {
        return ret;
    }

    return req.rate;
}

VMM_EXPORT_SYMBOL(clock_round_rate);

/**
 * __clock_notify - call clk notifier chain
 * @core: clk that is changing rate
 * @msg: clk notifier type
 * @old_rate: old clk rate
 * @new_rate: new clk rate
 *
 * Triggers a notifier call chain on the clk rate-change notification
 * for 'clk'.  Passes a pointer to the struct clk and the previous
 * and current rates to the notifier callback.  Intended to be called by
 * internal clock code only.  Returns NOTIFY_DONE from the last driver
 * called if all went well, or NOTIFY_STOP or NOTIFY_BAD immediately if
 * a driver returns that.
 */
static int __clock_notify(struct clock_core *core, uint64_t msg, uint64_t old_rate, uint64_t new_rate)
{
    struct clock_notifier     *cn;
    struct clock_notifier_data cnd;
    int                        ret = NOTIFY_DONE;

    cnd.old_rate                   = old_rate;
    cnd.new_rate                   = new_rate;

    list_for_each_entry(cn, &clock_notifier_list, node)
    {
        if (cn->clk->core == core) {
            cnd.clk = cn->clk;
            ret     = vmm_atomic_notifier_call(&cn->notifier_head, msg, &cnd);

            if (ret & NOTIFY_STOP_MASK) {
                return ret;
            }
        }
    }

    return ret;
}

/**
 * __clock_recalc_accuracies
 * @core: first clk in the subtree
 *
 * Walks the subtree of clks starting with clk and recalculates accuracies as
 * it goes.  Note that if a clk does not implement the .recalc_accuracy
 * callback then it is assumed that the clock will take on the accuracy of its
 * parent.
 */
static void __clock_recalc_accuracies(struct clock_core *core)
{
    uint64_t           parent_accuracy = 0;
    struct clock_core *child;

    lockdep_assert_held(&prepare_lock);

    if (core->parent) {
        parent_accuracy = core->parent->accuracy;
    }

    if (core->ops->recalc_accuracy) {
        core->accuracy = core->ops->recalc_accuracy(core->hw, parent_accuracy);
    } else {
        core->accuracy = parent_accuracy;
    }

    hlist_for_each_entry(child, &core->children, child_node) __clock_recalc_accuracies(child);
}

static long clock_core_get_accuracy(struct clock_core *core)
{
    uint64_t accuracy;

    clock_prepare_lock();

    if (core && (core->flags & CLK_GET_ACCURACY_NOCACHE)) {
        __clock_recalc_accuracies(core);
    }

    accuracy = __clock_get_accuracy(core);
    clock_prepare_unlock();

    return accuracy;
}

/**
 * clock_get_accuracy - return the accuracy of clk
 * @clk: the clk whose accuracy is being returned
 *
 * Simply returns the cached accuracy of the clk, unless
 * CLK_GET_ACCURACY_NOCACHE flag is set, which means a recalc_rate will be
 * issued.
 * If clk is NULL then returns 0.
 */
long clock_get_accuracy(struct clk *clk)
{
    if (!clk) {
        return 0;
    }

    return clock_core_get_accuracy(clk->core);
}

VMM_EXPORT_SYMBOL(clock_get_accuracy);

static uint64_t clock_recalc(struct clock_core *core, uint64_t parent_rate)
{
    uint64_t rate = parent_rate;

    if (core->ops->recalc_rate && !clock_pm_runtime_get(core)) {
        rate = core->ops->recalc_rate(core->hw, parent_rate);
        clock_pm_runtime_put(core);
    }

    return rate;
}

/**
 * __clock_recalc_rates
 * @core: first clk in the subtree
 * @msg: notification type (see include/linux/clk.h)
 *
 * Walks the subtree of clks starting with clk and recalculates rates as it
 * goes.  Note that if a clk does not implement the .recalc_rate callback then
 * it is assumed that the clock will take on the rate of its parent.
 *
 * clock_recalc_rates also propagates the POST_RATE_CHANGE notification,
 * if necessary.
 */
static void __clock_recalc_rates(struct clock_core *core, uint64_t msg)
{
    uint64_t           old_rate;
    uint64_t           parent_rate = 0;
    struct clock_core *child;

    lockdep_assert_held(&prepare_lock);

    old_rate = core->rate;

    if (core->parent) {
        parent_rate = core->parent->rate;
    }

    core->rate = clock_recalc(core, parent_rate);

    /*
     * ignore NOTIFY_STOP and NOTIFY_BAD return values for POST_RATE_CHANGE
     * & ABORT_RATE_CHANGE notifiers
     */
    if (core->notifier_count && msg) {
        __clock_notify(core, msg, old_rate, core->rate);
    }

    hlist_for_each_entry(child, &core->children, child_node) __clock_recalc_rates(child, msg);
}

static uint64_t clock_core_get_rate(struct clock_core *core)
{
    uint64_t rate;

    clock_prepare_lock();

    if (core && (core->flags & CLK_GET_RATE_NOCACHE)) {
        __clock_recalc_rates(core, 0);
    }

    rate = clock_core_get_rate_nolock(core);
    clock_prepare_unlock();

    return rate;
}

/**
 * clock_get_rate - return the rate of clk
 * @clk: the clk whose rate is being returned
 *
 * Simply returns the cached rate of the clk, unless CLK_GET_RATE_NOCACHE flag
 * is set, which means a recalc_rate will be issued.
 * If clk is NULL then returns 0.
 */
uint64_t clock_get_rate(struct clk *clk)
{
    if (!clk) {
        return 0;
    }

    return clock_core_get_rate(clk->core);
}

VMM_EXPORT_SYMBOL(clock_get_rate);

static int clock_fetch_parent_index(struct clock_core *core, struct clock_core *parent)
{
    int i;

    if (!parent) {
        return VMM_EINVALID;
    }

    for (i = 0; i < core->num_parents; i++) {
        if (clock_core_get_parent_by_index(core, i) == parent) {
            return i;
        }
    }

    return VMM_EINVALID;
}

/*
 * Update the orphan status of @core and all its children.
 */
static void clock_core_update_orphan_status(struct clock_core *core, bool is_orphan)
{
    struct clock_core *child;

    core->orphan = is_orphan;

    hlist_for_each_entry(child, &core->children, child_node) clock_core_update_orphan_status(child, is_orphan);
}

static void clock_reparent(struct clock_core *core, struct clock_core *new_parent)
{
    bool was_orphan = core->orphan;

    hlist_del(&core->child_node);

    if (new_parent) {
        bool becomes_orphan = new_parent->orphan;

        /* avoid duplicate POST_RATE_CHANGE notifications */
        if (new_parent->new_child == core) {
            new_parent->new_child = NULL;
        }

        hlist_add_head(&core->child_node, &new_parent->children);

        if (was_orphan != becomes_orphan) {
            clock_core_update_orphan_status(core, becomes_orphan);
        }
    } else {
        hlist_add_head(&core->child_node, &clock_orphan_list);

        if (!was_orphan) {
            clock_core_update_orphan_status(core, true);
        }
    }

    core->parent = new_parent;
}

static struct clock_core *__clock_set_parent_before(struct clock_core *core, struct clock_core *parent)
{
    uint64_t           flags;
    struct clock_core *old_parent = core->parent;

    /*
     * 1. enable parents for CLK_OPS_PARENT_ENABLE clock
     *
     * 2. Migrate prepare state between parents and prevent race with
     * clock_enable().
     *
     * If the clock is not prepared, then a race with
     * clock_enable/disable() is impossible since we already have the
     * prepare lock (future calls to clock_enable() need to be preceded by
     * a clock_prepare()).
     *
     * If the clock is prepared, migrate the prepared state to the new
     * parent and also protect against a race with clock_enable() by
     * forcing the clock and the new parent on.  This ensures that all
     * future calls to clock_enable() are practically NOPs with respect to
     * hardware and software states.
     *
     * See also: Comment for clock_set_parent() below.
     */

    /* enable old_parent & parent if CLK_OPS_PARENT_ENABLE is set */
    if (core->flags & CLK_OPS_PARENT_ENABLE) {
        clock_core_prepare_enable(old_parent);
        clock_core_prepare_enable(parent);
    }

    /* migrate prepare count if > 0 */
    if (core->prepare_count) {
        clock_core_prepare_enable(parent);
        clock_core_enable_lock(core);
    }

    /* update the clk tree topology */
    flags = clock_enable_lock();
    clock_reparent(core, parent);
    clock_enable_unlock(flags);

    return old_parent;
}

static void __clock_set_parent_after(struct clock_core *core, struct clock_core *parent, struct clock_core *old_parent)
{
    /*
     * Finish the migration of prepare state and undo the changes done
     * for preventing a race with clock_enable().
     */
    if (core->prepare_count) {
        clock_core_disable_lock(core);
        clock_core_disable_unprepare(old_parent);
    }

    /* re-balance ref counting if CLK_OPS_PARENT_ENABLE is set */
    if (core->flags & CLK_OPS_PARENT_ENABLE) {
        clock_core_disable_unprepare(parent);
        clock_core_disable_unprepare(old_parent);
    }
}

static int __clock_set_parent(struct clock_core *core, struct clock_core *parent, uint8_t p_index)
{
    uint64_t           flags;
    int                ret = 0;
    struct clock_core *old_parent;

    old_parent = __clock_set_parent_before(core, parent);

    /* change clock input source */
    if (parent && core->ops->set_parent) {
        ret = core->ops->set_parent(core->hw, p_index);
    }

    if (ret) {
        flags = clock_enable_lock();
        clock_reparent(core, old_parent);
        clock_enable_unlock(flags);
        __clock_set_parent_after(core, old_parent, parent);

        return ret;
    }

    __clock_set_parent_after(core, parent, old_parent);

    return 0;
}

/**
 * __clock_speculate_rates
 * @core: first clk in the subtree
 * @parent_rate: the "future" rate of clk's parent
 *
 * Walks the subtree of clks starting with clk, speculating rates as it
 * goes and firing off PRE_RATE_CHANGE notifications as necessary.
 *
 * Unlike clock_recalc_rates, clock_speculate_rates exists only for sending
 * pre-rate change notifications and returns early if no clks in the
 * subtree have subscribed to the notifications.  Note that if a clk does not
 * implement the .recalc_rate callback then it is assumed that the clock will
 * take on the rate of its parent.
 */
static int __clock_speculate_rates(struct clock_core *core, uint64_t parent_rate)
{
    struct clock_core *child;
    uint64_t           new_rate;
    int                ret = NOTIFY_DONE;

    lockdep_assert_held(&prepare_lock);

    new_rate = clock_recalc(core, parent_rate);

    /* abort rate change if a driver returns NOTIFY_BAD or NOTIFY_STOP */
    if (core->notifier_count) {
        ret = __clock_notify(core, PRE_RATE_CHANGE, core->rate, new_rate);
    }

    if (ret & NOTIFY_STOP_MASK) {
        vmm_lerror(__func__, "clk notifier callback for clock %s aborted with error %d\n", core->name, ret);
        goto out;
    }

    hlist_for_each_entry(child, &core->children, child_node)
    {
        ret = __clock_speculate_rates(child, new_rate);

        if (ret & NOTIFY_STOP_MASK) {
            break;
        }
    }

out:
    return ret;
}

static void clock_calc_subtree(struct clock_core *core, uint64_t new_rate, struct clock_core *new_parent, uint8_t p_index)
{
    struct clock_core *child;

    core->new_rate         = new_rate;
    core->new_parent       = new_parent;
    core->new_parent_index = p_index;
    /* include clk in new parent's PRE_RATE_CHANGE notifications */
    core->new_child        = NULL;

    if (new_parent && new_parent != core->parent) {
        new_parent->new_child = core;
    }

    hlist_for_each_entry(child, &core->children, child_node)
    {
        child->new_rate = clock_recalc(child, new_rate);
        clock_calc_subtree(child, child->new_rate, NULL, 0);
    }
}

/*
 * calculate the new rates returning the topmost clock that has to be
 * changed.
 */
static struct clock_core *clock_calc_new_rates(struct clock_core *core, uint64_t rate)
{
    struct clock_core *top = core;
    struct clock_core *old_parent, *parent;
    uint64_t           best_parent_rate = 0;
    uint64_t           new_rate;
    uint64_t           min_rate;
    uint64_t           max_rate;
    int                p_index = 0;
    long               ret;

    /* sanity */
    if (VMM_IS_ERR_OR_NULL(core)) {
        return NULL;
    }

    /* save parent rate, if it exists */
    parent = old_parent = core->parent;

    if (parent) {
        best_parent_rate = parent->rate;
    }

    clock_core_get_boundaries(core, &min_rate, &max_rate);

    /* find the closest rate and parent clk/rate */
    if (core->ops->determine_rate) {
        struct clock_rate_request req;

        req.rate     = rate;
        req.min_rate = min_rate;
        req.max_rate = max_rate;

        if (parent) {
            req.best_parent_hw   = parent->hw;
            req.best_parent_rate = parent->rate;
        } else {
            req.best_parent_hw   = NULL;
            req.best_parent_rate = 0;
        }

        ret = core->ops->determine_rate(core->hw, &req);

        if (ret < 0) {
            return NULL;
        }

        best_parent_rate = req.best_parent_rate;
        new_rate         = req.rate;
        parent           = req.best_parent_hw ? req.best_parent_hw->core : NULL;
    } else if (core->ops->round_rate) {
        ret = core->ops->round_rate(core->hw, rate, &best_parent_rate);

        if (ret < 0) {
            return NULL;
        }

        new_rate = ret;

        if (new_rate < min_rate || new_rate > max_rate) {
            return NULL;
        }
    } else if (!parent || !(core->flags & CLK_SET_RATE_PARENT)) {
        /* pass-through clock without adjustable parent */
        core->new_rate = core->rate;
        return NULL;
    } else {
        /* pass-through clock with adjustable parent */
        top      = clock_calc_new_rates(parent, rate);
        new_rate = parent->new_rate;
        goto out;
    }

    /* some clocks must be gated to change parent */
    if (parent != old_parent && (core->flags & CLK_SET_PARENT_GATE) && core->prepare_count) {
        vmm_lerror(__func__, "%s not gated but wants to reparent\n", core->name);
        return NULL;
    }

    /* try finding the new parent index */
    if (parent && core->num_parents > 1) {
        p_index = clock_fetch_parent_index(core, parent);

        if (p_index < 0) {
            vmm_lerror(__func__, "clk %s can not be parent of clk %s\n", parent->name, core->name);
            return NULL;
        }
    }

    if ((core->flags & CLK_SET_RATE_PARENT) && parent && best_parent_rate != parent->rate) {
        top = clock_calc_new_rates(parent, best_parent_rate);
    }

out:
    clock_calc_subtree(core, new_rate, parent, p_index);

    return top;
}

/*
 * Notify about rate changes in a subtree. Always walk down the whole tree
 * so that in case of an error we can walk down the whole tree again and
 * abort the change.
 */
static struct clock_core *clock_propagate_rate_change(struct clock_core *core, uint64_t event)
{
    struct clock_core *child, *tmp_clock, *fail_clock = NULL;
    int                ret = NOTIFY_DONE;

    if (core->rate == core->new_rate) {
        return NULL;
    }

    if (core->notifier_count) {
        ret = __clock_notify(core, event, core->rate, core->new_rate);

        if (ret & NOTIFY_STOP_MASK) {
            fail_clock = core;
        }
    }

    hlist_for_each_entry(child, &core->children, child_node)
    {
        /* Skip children who will be reparented to another clock */
        if (child->new_parent && child->new_parent != core) {
            continue;
        }

        tmp_clock = clock_propagate_rate_change(child, event);

        if (tmp_clock) {
            fail_clock = tmp_clock;
        }
    }

    /* handle the new child who might not be in core->children yet */
    if (core->new_child) {
        tmp_clock = clock_propagate_rate_change(core->new_child, event);

        if (tmp_clock) {
            fail_clock = tmp_clock;
        }
    }

    return fail_clock;
}

static int clock_core_set_rate_nolock(struct clock_core *core, uint64_t req_rate);

/*
 * walk down a subtree and set the new rates notifying the rate
 * change on the way
 */
static void clock_change_rate(struct clock_core *core)
{
    struct clock_core *child;
    struct hlist_node *tmp;
    uint64_t           old_rate;
    uint64_t           best_parent_rate = 0;
    bool               skip_set_rate    = false;
    struct clock_core *old_parent;
    struct clock_core *parent = NULL;

    old_rate                  = core->rate;

    if (core->new_parent) {
        parent           = core->new_parent;
        best_parent_rate = core->new_parent->rate;
    } else if (core->parent) {
        parent           = core->parent;
        best_parent_rate = core->parent->rate;
    }

    if (clock_pm_runtime_get(core)) {
        return;
    }

    if (core->flags & CLK_SET_RATE_UNGATE) {
        uint64_t flags;

        clock_core_prepare(core);
        flags = clock_enable_lock();
        clock_core_enable(core);
        clock_enable_unlock(flags);
    }

    if (core->new_parent && core->new_parent != core->parent) {
        old_parent = __clock_set_parent_before(core, core->new_parent);

        if (core->ops->set_rate_and_parent) {
            skip_set_rate = true;
            core->ops->set_rate_and_parent(core->hw, core->new_rate, best_parent_rate, core->new_parent_index);
        } else if (core->ops->set_parent) {
            core->ops->set_parent(core->hw, core->new_parent_index);
        }

        __clock_set_parent_after(core, core->new_parent, old_parent);
    }

    if (core->flags & CLK_OPS_PARENT_ENABLE) {
        clock_core_prepare_enable(parent);
    }

    if (!skip_set_rate && core->ops->set_rate) {
        core->ops->set_rate(core->hw, core->new_rate, best_parent_rate);
    }

    core->rate = clock_recalc(core, best_parent_rate);

    if (core->flags & CLK_SET_RATE_UNGATE) {
        uint64_t flags;

        flags = clock_enable_lock();
        clock_core_disable(core);
        clock_enable_unlock(flags);
        clock_core_unprepare(core);
    }

    if (core->flags & CLK_OPS_PARENT_ENABLE) {
        clock_core_disable_unprepare(parent);
    }

    if (core->notifier_count && old_rate != core->rate) {
        __clock_notify(core, POST_RATE_CHANGE, old_rate, core->rate);
    }

    if (core->flags & CLK_RECALC_NEW_RATES) {
        (void)clock_calc_new_rates(core, core->new_rate);
    }

    /*
     * Use safe iteration, as change_rate can actually swap parents
     * for certain clock types.
     */
    hlist_for_each_entry_safe(child, tmp, &core->children, child_node)
    {
        /* Skip children who will be reparented to another clock */
        if (child->new_parent && child->new_parent != core) {
            continue;
        }

        clock_change_rate(child);
    }

    /* handle the new child who might not be in core->children yet */
    if (core->new_child) {
        clock_change_rate(core->new_child);
    }

    if ((core->flags & CLK_KEEP_REQ_RATE) != 0 && core->req_rate && core->new_rate != old_rate && core->new_rate != core->req_rate) {
        clock_core_set_rate_nolock(core, core->req_rate);
    }

    clock_pm_runtime_put(core);
}

static int clock_core_set_rate_nolock(struct clock_core *core, uint64_t req_rate)
{
    struct clock_core *top, *fail_clock;
    uint64_t           rate = req_rate;
    int                ret  = 0;

    if (!core) {
        return 0;
    }

    /* bail early if nothing to do */
    if (rate == clock_core_get_rate_nolock(core)) {
        return 0;
    }

    if ((core->flags & CLK_SET_RATE_GATE) && core->prepare_count) {
        return VMM_EBUSY;
    }

    /* calculate new rates and get the topmost changed clock */
    top = clock_calc_new_rates(core, rate);

    if (!top) {
        return VMM_EINVALID;
    }

    ret = clock_pm_runtime_get(core);

    if (ret) {
        return ret;
    }

    /* notify that we are about to change rates */
    fail_clock = clock_propagate_rate_change(top, PRE_RATE_CHANGE);

    if (fail_clock) {
        vmm_lerror(__func__, "failed to set %s rate\n", fail_clock->name);
        clock_propagate_rate_change(top, ABORT_RATE_CHANGE);
        ret = VMM_EBUSY;
        goto err;
    }

    /* change the rates */
    clock_change_rate(top);

    core->req_rate = req_rate;
err:
    clock_pm_runtime_put(core);

    return ret;
}

/**
 * clock_set_rate - specify a new rate for clk
 * @clk: the clk whose rate is being changed
 * @rate: the new rate for clk
 *
 * In the simplest case clock_set_rate will only adjust the rate of clk.
 *
 * Setting the CLK_SET_RATE_PARENT flag allows the rate change operation to
 * propagate up to clk's parent; whether or not this happens depends on the
 * outcome of clk's .round_rate implementation.  If *parent_rate is unchanged
 * after calling .round_rate then upstream parent propagation is ignored.  If
 * *parent_rate comes back with a new rate for clk's parent then we propagate
 * up to clk's parent and set its rate.  Upward propagation will continue
 * until either a clk does not support the CLK_SET_RATE_PARENT flag or
 * .round_rate stops requesting changes to clk's parent_rate.
 *
 * Rate changes are accomplished via tree traversal that also recalculates the
 * rates for the clocks and fires off POST_RATE_CHANGE notifiers.
 *
 * Returns 0 on success, VMM_EERROR otherwise.
 */
int clock_set_rate(struct clk *clk, uint64_t rate)
{
    int ret;

    if (!clk) {
        return 0;
    }

    /* prevent racing with updates to the clock topology */
    clock_prepare_lock();

    ret = clock_core_set_rate_nolock(clk->core, rate);

    clock_prepare_unlock();

    return ret;
}

VMM_EXPORT_SYMBOL(clock_set_rate);

/**
 * clock_set_rate_range - set a rate range for a clock source
 * @clk: clock source
 * @min: desired minimum clock rate in Hz, inclusive
 * @max: desired maximum clock rate in Hz, inclusive
 *
 * Returns success (0) or negative errno.
 */
int clock_set_rate_range(struct clk *clk, uint64_t min, uint64_t max)
{
    int ret = 0;

    if (!clk) {
        return 0;
    }

    if (min > max) {
        vmm_lerror(__func__, "clk %s dev %s con %s: invalid range [%lu, %lu]\n", clk->core->name, clk->dev_id, clk->con_id, min, max);
        return VMM_EINVALID;
    }

    clock_prepare_lock();

    if (min != clk->min_rate || max != clk->max_rate) {
        clk->min_rate = min;
        clk->max_rate = max;
        ret           = clock_core_set_rate_nolock(clk->core, clk->core->req_rate);
    }

    clock_prepare_unlock();

    return ret;
}

VMM_EXPORT_SYMBOL(clock_set_rate_range);

/**
 * clock_set_min_rate - set a minimum clock rate for a clock source
 * @clk: clock source
 * @rate: desired minimum clock rate in Hz, inclusive
 *
 * Returns success (0) or negative errno.
 */
int clock_set_min_rate(struct clk *clk, uint64_t rate)
{
    if (!clk) {
        return 0;
    }

    return clock_set_rate_range(clk, rate, clk->max_rate);
}

VMM_EXPORT_SYMBOL(clock_set_min_rate);

/**
 * clock_set_max_rate - set a maximum clock rate for a clock source
 * @clk: clock source
 * @rate: desired maximum clock rate in Hz, inclusive
 *
 * Returns success (0) or negative errno.
 */
int clock_set_max_rate(struct clk *clk, uint64_t rate)
{
    if (!clk) {
        return 0;
    }

    return clock_set_rate_range(clk, clk->min_rate, rate);
}

VMM_EXPORT_SYMBOL(clock_set_max_rate);

/**
 * clock_get_parent - return the parent of a clk
 * @clk: the clk whose parent gets returned
 *
 * Simply returns clk->parent.  Returns NULL if clk is NULL.
 */
struct clk *clock_get_parent(struct clk *clk)
{
    struct clk *parent;

    if (!clk) {
        return NULL;
    }

    clock_prepare_lock();
    /* TODO: Create a per-user clk and change callers to call clock_put */
    parent = !clk->core->parent ? NULL : clk->core->parent->hw->clk;
    clock_prepare_unlock();

    return parent;
}

VMM_EXPORT_SYMBOL(clock_get_parent);

static struct clock_core *__clock_init_parent(struct clock_core *core)
{
    uint8_t index = 0;

    if (core->num_parents > 1 && core->ops->get_parent) {
        index = core->ops->get_parent(core->hw);
    }

    return clock_core_get_parent_by_index(core, index);
}

static void clock_core_reparent(struct clock_core *core, struct clock_core *new_parent)
{
    clock_reparent(core, new_parent);
    __clock_recalc_accuracies(core);
    __clock_recalc_rates(core, POST_RATE_CHANGE);
}

void clock_hw_reparent(struct clock_hw *hw, struct clock_hw *new_parent)
{
    if (!hw) {
        return;
    }

    clock_core_reparent(hw->core, !new_parent ? NULL : new_parent->core);
}

/**
 * clock_has_parent - check if a clock is a possible parent for another
 * @clk: clock source
 * @parent: parent clock source
 *
 * This function can be used in drivers that need to check that a clock can be
 * the parent of another without actually changing the parent.
 *
 * Returns true if @parent is a possible parent for @clk, false otherwise.
 */
bool clock_has_parent(struct clk *clk, struct clk *parent)
{
    struct clock_core *core, *parent_core;
    uint32_t           i;

    /* NULL clocks should be nops, so return success if either is NULL. */
    if (!clk || !parent) {
        return true;
    }

    core        = clk->core;
    parent_core = parent->core;

    /* Optimize for the case where the parent is already the parent. */
    if (core->parent == parent_core) {
        return true;
    }

    for (i = 0; i < core->num_parents; i++) {
        if (strcmp(core->parent_names[i], parent_core->name) == 0) {
            return true;
        }
    }

    return false;
}

VMM_EXPORT_SYMBOL(clock_has_parent);

static int clock_core_set_parent(struct clock_core *core, struct clock_core *parent)
{
    int      ret     = 0;
    int      p_index = 0;
    uint64_t p_rate  = 0;

    if (!core) {
        return 0;
    }

    /* prevent racing with updates to the clock topology */
    clock_prepare_lock();

    if (core->parent == parent) {
        goto out;
    }

    /* verify ops for for multi-parent clks */
    if ((core->num_parents > 1) && (!core->ops->set_parent)) {
        ret = VMM_ENOSYS;
        goto out;
    }

    /* check that we are allowed to re-parent if the clock is in use */
    if ((core->flags & CLK_SET_PARENT_GATE) && core->prepare_count) {
        ret = VMM_EBUSY;
        goto out;
    }

    /* try finding the new parent index */
    if (parent) {
        p_index = clock_fetch_parent_index(core, parent);

        if (p_index < 0) {
            vmm_lerror(__func__, "clk %s can not be parent of clk %s\n", parent->name, core->name);
            ret = p_index;
            goto out;
        }

        p_rate = parent->rate;
    }

    ret = clock_pm_runtime_get(core);

    if (ret) {
        goto out;
    }

    /* propagate PRE_RATE_CHANGE notifications */
    ret = __clock_speculate_rates(core, p_rate);

    /* abort if a driver objects */
    if (ret & NOTIFY_STOP_MASK) {
        goto runtime_put;
    }

    /* do the re-parent */
    ret = __clock_set_parent(core, parent, p_index);

    /* propagate rate an accuracy recalculation accordingly */
    if (ret) {
        __clock_recalc_rates(core, ABORT_RATE_CHANGE);
    } else {
        __clock_recalc_rates(core, POST_RATE_CHANGE);
        __clock_recalc_accuracies(core);
    }

runtime_put:
    clock_pm_runtime_put(core);
out:
    clock_prepare_unlock();

    return ret;
}

/**
 * clock_set_parent - switch the parent of a mux clk
 * @clk: the mux clk whose input we are switching
 * @parent: the new input to clk
 *
 * Re-parent clk to use parent as its new input source.  If clk is in
 * prepared state, the clk will get enabled for the duration of this call. If
 * that's not acceptable for a specific clk (Eg: the consumer can't handle
 * that, the reparenting is glitchy in hardware, etc), use the
 * CLK_SET_PARENT_GATE flag to allow reparenting only when clk is unprepared.
 *
 * After successfully changing clk's parent clock_set_parent will update the
 * clk topology, sysfs topology and propagate rate recalculation via
 * __clock_recalc_rates.
 *
 * Returns 0 on success, VMM_EERROR otherwise.
 */
int clock_set_parent(struct clk *clk, struct clk *parent)
{
    if (!clk) {
        return 0;
    }

    return clock_core_set_parent(clk->core, parent ? parent->core : NULL);
}

VMM_EXPORT_SYMBOL(clock_set_parent);

/**
 * clock_set_phase - adjust the phase shift of a clock signal
 * @clk: clock signal source
 * @degrees: number of degrees the signal is shifted
 *
 * Shifts the phase of a clock signal by the specified
 * degrees. Returns 0 on success, VMM_EERROR otherwise.
 *
 * This function makes no distinction about the input or reference
 * signal that we adjust the clock signal phase against. For example
 * phase locked-loop clock signal generators we may shift phase with
 * respect to feedback clock signal input, but for other cases the
 * clock phase may be shifted with respect to some other, unspecified
 * signal.
 *
 * Additionally the concept of phase shift does not propagate through
 * the clock tree hierarchy, which sets it apart from clock rates and
 * clock accuracy. A parent clock phase attribute does not have an
 * impact on the phase attribute of a child clock.
 */
int clock_set_phase(struct clk *clk, int degrees)
{
    int ret = VMM_EINVALID;

    if (!clk) {
        return 0;
    }

    /* sanity check degrees */
    degrees = smod32(degrees, 360);

    if (degrees < 0) {
        degrees += 360;
    }

    clock_prepare_lock();

    if (clk->core->ops->set_phase) {
        ret = clk->core->ops->set_phase(clk->core->hw, degrees);
    }

    if (!ret) {
        clk->core->phase = degrees;
    }

    clock_prepare_unlock();

    return ret;
}

VMM_EXPORT_SYMBOL(clock_set_phase);

static int clock_core_get_phase(struct clock_core *core)
{
    int ret;

    clock_prepare_lock();
    ret = core->phase;
    clock_prepare_unlock();

    return ret;
}

/**
 * clock_get_phase - return the phase shift of a clock signal
 * @clk: clock signal source
 *
 * Returns the phase shift of a clock node in degrees, otherwise returns
 * VMM_EERROR.
 */
int clock_get_phase(struct clk *clk)
{
    if (!clk) {
        return 0;
    }

    return clock_core_get_phase(clk->core);
}

VMM_EXPORT_SYMBOL(clock_get_phase);

/**
 * clock_is_match - check if two clk's point to the same hardware clock
 * @p: clk compared against q
 * @q: clk compared against p
 *
 * Returns true if the two struct clk pointers both point to the same hardware
 * clock node. Put differently, returns true if struct clk *p and struct clk *q
 * share the same struct clock_core object.
 *
 * Returns false otherwise. Note that two NULL clks are treated as matching.
 */
bool clock_is_match(const struct clk *p, const struct clk *q)
{
    /* trivial case: identical struct clk's or both NULL */
    if (p == q) {
        return true;
    }

    /* true if clk->core pointers match. Avoid dereferencing garbage */
    if (!VMM_IS_ERR_OR_NULL(p) && !VMM_IS_ERR_OR_NULL(q)) {
        if (p->core == q->core) {
            return true;
        }
    }

    return false;
}

VMM_EXPORT_SYMBOL(clock_is_match);

/***        debugfs support        ***/

static void clock_summary_show_one(vmm_char_device_t *cdev, struct clock_core *c, int level)
{
    if (!c) {
        return;
    }

    vmm_cdev_printf(
        cdev, " %-30s %-11d %-12d %-11lu %-10lu %-3d\n", c->name, c->enable_count, c->prepare_count, clock_core_get_rate(c),
        clock_core_get_accuracy(c), clock_core_get_phase(c));
}

static void clock_summary_show_subtree(vmm_char_device_t *cdev, struct clock_core *c, int level)
{
    struct clock_core *child;

    if (!c) {
        return;
    }

    clock_summary_show_one(cdev, c, level);

    hlist_for_each_entry(child, &c->children, child_node) clock_summary_show_subtree(cdev, child, level + 1);
}

int clock_summary_show(vmm_char_device_t *cdev)
{
    struct clock_core *c;

    vmm_cdev_printf(cdev, "----------------------------------------------------------------------------------------\n");
    vmm_cdev_printf(cdev, " clock                          enable_cnt  prepare_cnt  rate        accuracy   phase\n");
    vmm_cdev_printf(cdev, "----------------------------------------------------------------------------------------\n");

    clock_prepare_lock();

    hlist_for_each_entry(c, &clock_root_list, child_node) clock_summary_show_subtree(cdev, c, 0);

    hlist_for_each_entry(c, &clock_orphan_list, child_node) clock_summary_show_subtree(cdev, c, 0);

    clock_prepare_unlock();

    vmm_cdev_printf(cdev, "----------------------------------------------------------------------------------------\n");

    return 0;
}

VMM_EXPORT_SYMBOL(clock_summary_show);

static void clock_dump_indent(vmm_char_device_t *cdev, int level)
{
    level++;
    vmm_cdev_printf(cdev, "\n");

    while (level--) {
        vmm_cdev_printf(cdev, "\t");
    }
}

static void clock_dump_one(vmm_char_device_t *cdev, struct clock_core *c, int level)
{
    if (!c) {
        return;
    }

    clock_dump_indent(cdev, level);
    vmm_cdev_printf(cdev, "\"%s\": { ", c->name);
    clock_dump_indent(cdev, level + 1);
    vmm_cdev_printf(cdev, "\"enable_count\": %d,", c->enable_count);
    clock_dump_indent(cdev, level + 1);
    vmm_cdev_printf(cdev, "\"prepare_count\": %d,", c->prepare_count);
    clock_dump_indent(cdev, level + 1);
    vmm_cdev_printf(cdev, "\"rate\": %lu,", clock_core_get_rate(c));
    clock_dump_indent(cdev, level + 1);
    vmm_cdev_printf(cdev, "\"accuracy\": %lu", clock_core_get_accuracy(c));
    clock_dump_indent(cdev, level + 1);
    vmm_cdev_printf(cdev, "\"phase\": %d", clock_core_get_phase(c));
}

static void clock_dump_subtree(vmm_char_device_t *cdev, struct clock_core *c, int level)
{
    struct clock_core *child;

    if (!c) {
        return;
    }

    clock_dump_one(cdev, c, level);

    hlist_for_each_entry(child, &c->children, child_node)
    {
        vmm_cdev_printf(cdev, ",");
        clock_dump_subtree(cdev, child, level + 1);
    }

    clock_dump_indent(cdev, level);
    vmm_cdev_printf(cdev, "}");
}

int clock_dump(vmm_char_device_t *cdev)
{
    struct clock_core *c;
    bool               first_node = true;

    vmm_cdev_printf(cdev, "{");

    clock_prepare_lock();

    hlist_for_each_entry(c, &clock_root_list, child_node)
    {
        if (!first_node) {
            vmm_cdev_printf(cdev, ",");
        }

        first_node = false;
        clock_dump_subtree(cdev, c, 0);
    }

    hlist_for_each_entry(c, &clock_orphan_list, child_node)
    {
        vmm_cdev_printf(cdev, ",");
        clock_dump_subtree(cdev, c, 0);
    }

    clock_prepare_unlock();

    vmm_cdev_printf(cdev, "\n}\n");
    return 0;
}

VMM_EXPORT_SYMBOL(clock_dump);

static inline int clock_debug_register(struct clock_core *core)
{
    return 0;
}

static inline void clock_debug_reparent(struct clock_core *core, struct clock_core *new_parent) {}

static inline void clock_debug_unregister(struct clock_core *core) {}

/**
 * __clock_core_init - initialize the data structures in a struct clock_core
 * @core:   clock_core being initialized
 *
 * Initializes the lists in struct clock_core, queries the hardware for the
 * parent and rate and sets them both.
 */
static int __clock_core_init(struct clock_core *core)
{
    int                i, ret;
    struct clock_core *orphan;
    struct hlist_node *tmp2;
    uint64_t           rate;

    if (!core) {
        return VMM_EINVALID;
    }

    clock_prepare_lock();

    ret = clock_pm_runtime_get(core);

    if (ret) {
        goto unlock;
    }

    /* check to see if a clock with this name is already registered */
    if (clock_core_lookup(core->name)) {
        vmm_lerror(__func__, "clk %s already initialized\n", core->name);
        ret = VMM_EEXIST;
        goto out;
    }

    /* check that clock_ops are sane.  See Documentation/clk.txt */
    if (core->ops->set_rate && !((core->ops->round_rate || core->ops->determine_rate) && core->ops->recalc_rate)) {
        vmm_lerror(__func__, "%s must implement .round_rate or .determine_rate in addition to .recalc_rate\n", core->name);
        ret = VMM_EINVALID;
        goto out;
    }

    if (core->ops->set_parent && !core->ops->get_parent) {
        vmm_lerror(__func__, "%s must implement .get_parent & .set_parent\n", core->name);
        ret = VMM_EINVALID;
        goto out;
    }

    if (core->num_parents > 1 && !core->ops->get_parent) {
        vmm_lerror(__func__, "%s must implement .get_parent as it has multi parents\n", core->name);
        ret = VMM_EINVALID;
        goto out;
    }

    if (core->ops->set_rate_and_parent && !(core->ops->set_parent && core->ops->set_rate)) {
        vmm_lerror(__func__, "%s must implement .set_parent & .set_rate\n", core->name);
        ret = VMM_EINVALID;
        goto out;
    }

    /* throw a WARN if any entries in parent_names are NULL */
    for (i = 0; i < core->num_parents; i++) {
        WARN(!core->parent_names[i], "%s: invalid NULL in %s's .parent_names\n", __func__, core->name);
    }

    core->parent = __clock_init_parent(core);

    /*
     * Populate core->parent if parent has already been clock_core_init'd. If
     * parent has not yet been clock_core_init'd then place clk in the orphan
     * list.  If clk doesn't have any parents then place it in the root
     * clk list.
     *
     * Every time a new clk is clock_init'd then we walk the list of orphan
     * clocks and re-parent any that are children of the clock currently
     * being clock_init'd.
     */
    if (core->parent) {
        hlist_add_head(&core->child_node, &core->parent->children);
        core->orphan = core->parent->orphan;
    } else if (!core->num_parents) {
        hlist_add_head(&core->child_node, &clock_root_list);
        core->orphan = false;
    } else {
        hlist_add_head(&core->child_node, &clock_orphan_list);
        core->orphan = true;
    }

    /*
     * Set clk's accuracy.  The preferred method is to use
     * .recalc_accuracy. For simple clocks and lazy developers the default
     * fallback is to use the parent's accuracy.  If a clock doesn't have a
     * parent (or is orphaned) then accuracy is set to zero (perfect
     * clock).
     */
    if (core->ops->recalc_accuracy) {
        core->accuracy = core->ops->recalc_accuracy(core->hw, __clock_get_accuracy(core->parent));
    } else if (core->parent) {
        core->accuracy = core->parent->accuracy;
    } else {
        core->accuracy = 0;
    }

    /*
     * Set clk's phase.
     * Since a phase is by definition relative to its parent, just
     * query the current clock phase, or just assume it's in phase.
     */
    if (core->ops->get_phase) {
        core->phase = core->ops->get_phase(core->hw);
    } else {
        core->phase = 0;
    }

    /*
     * Set clk's rate.  The preferred method is to use .recalc_rate.  For
     * simple clocks and lazy developers the default fallback is to use the
     * parent's rate.  If a clock doesn't have a parent (or is orphaned)
     * then rate is set to zero.
     */
    if (core->ops->recalc_rate) {
        rate = core->ops->recalc_rate(core->hw, clock_core_get_rate_nolock(core->parent));
    } else if (core->parent) {
        rate = core->parent->rate;
    } else {
        rate = 0;
    }

    core->rate = core->req_rate = rate;

    /*
     * walk the list of orphan clocks and reparent any that newly finds a
     * parent.
     */
    hlist_for_each_entry_safe(orphan, tmp2, &clock_orphan_list, child_node)
    {
        struct clock_core *parent = __clock_init_parent(orphan);

        /*
         * we could call __clock_set_parent, but that would result in a
         * redundant call to the .set_rate op, if it exists
         */
        if (parent) {
            __clock_set_parent_before(orphan, parent);
            __clock_set_parent_after(orphan, parent, NULL);
            __clock_recalc_accuracies(orphan);
            __clock_recalc_rates(orphan, 0);
        }
    }

    /*
     * optional platform-specific magic
     *
     * The .init callback is not used by any of the basic clock types, but
     * exists for weird hardware that must perform initialization magic.
     * Please consider other ways of solving initialization problems before
     * using this callback, as its use is discouraged.
     */
    if (core->ops->init) {
        core->ops->init(core->hw);
    }

    if (core->flags & CLK_IS_CRITICAL) {
        uint64_t flags;

        clock_core_prepare(core);

        flags = clock_enable_lock();
        clock_core_enable(core);
        clock_enable_unlock(flags);
    }

    xref_init(&core->ref);
out:
    clock_pm_runtime_put(core);
unlock:
    clock_prepare_unlock();

    if (!ret) {
        clock_debug_register(core);
    }

    return ret;
}

struct clk *__clock_create_clock(struct clock_hw *hw, const char *dev_id, const char *con_id)
{
    struct clk *clk;

    /* This is to allow this function to be chained to others */
    if (VMM_IS_ERR_OR_NULL(hw)) {
        return VMM_ERR_CAST(hw);
    }

    clk = vmm_zalloc(sizeof(*clk));

    if (!clk) {
        return VMM_ERR_PTR(VMM_ENOMEM);
    }

    clk->core     = hw->core;
    clk->dev_id   = dev_id;
    clk->con_id   = vmm_strdup_const(con_id);
    clk->max_rate = ULONG_MAX;

    clock_prepare_lock();
    hlist_add_head(&clk->clks_node, &hw->core->clks);
    clock_prepare_unlock();

    return clk;
}

void __clock_free_clock(struct clk *clk)
{
    clock_prepare_lock();
    hlist_del(&clk->clks_node);
    clock_prepare_unlock();

    if (clk->con_id) {
        vmm_free((void *)clk->con_id);
    }

    vmm_free(clk);
}

/**
 * clock_register - allocate a new clock, register it and return an opaque cookie
 * @dev: device that is registering this clock
 * @hw: link to hardware-specific clock data
 *
 * clock_register is the primary interface for populating the clock tree with new
 * clock nodes.  It returns a pointer to the newly allocated struct clk which
 * cannot be dereferenced by driver code but may be used in conjunction with the
 * rest of the clock API.  In the event of an error clock_register will return an
 * error code; drivers must test for an error code after calling clock_register.
 */
struct clk *clock_register(vmm_device_t *dev, struct clock_hw *hw)
{
    int                i, ret;
    struct clock_core *core;

    core = vmm_zalloc(sizeof(*core));

    if (!core) {
        ret = VMM_ENOMEM;
        goto fail_out;
    }

    core->name = vmm_strdup_const(hw->init->name);

    if (!core->name) {
        ret = VMM_ENOMEM;
        goto fail_name;
    }

    core->ops = hw->init->ops;

    if (dev && pm_runtime_enabled(dev)) {
        core->dev = dev;
    }

    core->hw           = hw;
    core->flags        = hw->init->flags;
    core->num_parents  = hw->init->num_parents;
    core->min_rate     = 0;
    core->max_rate     = ULONG_MAX;
    hw->core           = core;

    /* allocate local copy in case parent_names is __initdata */
    core->parent_names = vmm_calloc(core->num_parents, sizeof(char *));

    if (!core->parent_names) {
        ret = VMM_ENOMEM;
        goto fail_parent_names;
    }

    /* copy each string name in case parent_names is __initdata */
    for (i = 0; i < core->num_parents; i++) {
        core->parent_names[i] = vmm_strdup_const(hw->init->parent_names[i]);

        if (!core->parent_names[i]) {
            ret = VMM_ENOMEM;
            goto fail_parent_names_copy;
        }
    }

    /* avoid unnecessary string look-ups of clock_core's possible parents. */
    core->parents = vmm_calloc(core->num_parents, sizeof(*core->parents));

    if (!core->parents) {
        ret = VMM_ENOMEM;
        goto fail_parents;
    };

    INIT_HLIST_HEAD(&core->clks);

    hw->clk = __clock_create_clock(hw, NULL, NULL);

    if (VMM_IS_ERR(hw->clk)) {
        ret = VMM_PTR_ERR(hw->clk);
        goto fail_parents;
    }

    ret = __clock_core_init(core);

    if (!ret) {
        return hw->clk;
    }

    __clock_free_clock(hw->clk);
    hw->clk = NULL;

fail_parents:
    vmm_free(core->parents);
fail_parent_names_copy:

    while (--i >= 0) {
        vmm_free((void *)core->parent_names[i]);
    }

    vmm_free((void *)core->parent_names);
fail_parent_names:
    vmm_free((void *)core->name);
fail_name:
    vmm_free(core);
fail_out:
    return VMM_ERR_PTR(ret);
}

VMM_EXPORT_SYMBOL(clock_register);

/**
 * clock_hw_register - register a clock_hw and return an error code
 * @dev: device that is registering this clock
 * @hw: link to hardware-specific clock data
 *
 * clock_hw_register is the primary interface for populating the clock tree with
 * new clock nodes. It returns an integer equal to zero indicating success or
 * less than zero indicating failure. Drivers must test for an error code after
 * calling clock_hw_register().
 */
int clock_hw_register(vmm_device_t *dev, struct clock_hw *hw)
{
    return VMM_PTR_RET(clock_register(dev, hw));
}

VMM_EXPORT_SYMBOL(clock_hw_register);

/* Free memory allocated for a clock. */
static void __clock_release(struct xref *ref)
{
    struct clock_core *core = container_of(ref, struct clock_core, ref);
    int                i    = core->num_parents;

    lockdep_assert_held(&prepare_lock);

    vmm_free(core->parents);

    while (--i >= 0) {
        vmm_free((void *)core->parent_names[i]);
    }

    vmm_free((void *)core->parent_names);
    vmm_free((void *)core->name);
    vmm_free(core);
}

/*
 * Empty clock_ops for unregistered clocks. These are used temporarily
 * after clock_unregister() was called on a clock and until last clock
 * consumer calls clock_put() and the struct clk object is freed.
 */
static int clock_nodrv_prepare_enable(struct clock_hw *hw)
{
    return VMM_EIO;
}

static void clock_nodrv_disable_unprepare(struct clock_hw *hw)
{
    WARN_ON(1);
}

static int clock_nodrv_set_rate(struct clock_hw *hw, uint64_t rate, uint64_t parent_rate)
{
    return VMM_EIO;
}

static int clock_nodrv_set_parent(struct clock_hw *hw, uint8_t index)
{
    return VMM_EIO;
}

static const struct clock_ops clock_nodrv_ops = {
    .enable     = clock_nodrv_prepare_enable,
    .disable    = clock_nodrv_disable_unprepare,
    .prepare    = clock_nodrv_prepare_enable,
    .unprepare  = clock_nodrv_disable_unprepare,
    .set_rate   = clock_nodrv_set_rate,
    .set_parent = clock_nodrv_set_parent,
};

/**
 * clock_unregister - unregister a currently registered clock
 * @clk: clock to unregister
 */
void clock_unregister(struct clk *clk)
{
    uint64_t flags;

    if (!clk || WARN_ON(VMM_IS_ERR(clk))) {
        return;
    }

    clock_debug_unregister(clk->core);

    clock_prepare_lock();

    if (clk->core->ops == &clock_nodrv_ops) {
        vmm_lerror(__func__, "unregistered clock: %s\n", clk->core->name);
        goto unlock;
    }

    /*
     * Assign empty clock ops for consumers that might still hold
     * a reference to this clock.
     */
    flags          = clock_enable_lock();
    clk->core->ops = &clock_nodrv_ops;
    clock_enable_unlock(flags);

    if (!hlist_empty(&clk->core->children)) {
        struct clock_core *child;
        struct hlist_node *t;

        /* Reparent all children to the orphan list. */
        hlist_for_each_entry_safe(child, t, &clk->core->children, child_node) clock_core_set_parent(child, NULL);
    }

    hlist_del_init(&clk->core->child_node);

    if (clk->core->prepare_count) {
        vmm_lwarning(__func__, "unregistering prepared clock: %s\n", clk->core->name);
    }

    xref_put(&clk->core->ref, __clock_release);
unlock:
    clock_prepare_unlock();
}

VMM_EXPORT_SYMBOL(clock_unregister);

/**
 * clock_hw_unregister - unregister a currently registered clock_hw
 * @hw: hardware-specific clock data to unregister
 */
void clock_hw_unregister(struct clock_hw *hw)
{
    clock_unregister(hw->clk);
}

VMM_EXPORT_SYMBOL(clock_hw_unregister);

static void devm_clock_release(vmm_device_t *dev, void *res)
{
    clock_unregister(*(struct clk **)res);
}

static void devm_clock_hw_release(vmm_device_t *dev, void *res)
{
    clock_hw_unregister(*(struct clock_hw **)res);
}

/**
 * devm_clock_register - resource managed clock_register()
 * @dev: device that is registering this clock
 * @hw: link to hardware-specific clock data
 *
 * Managed clock_register(). Clocks returned from this function are
 * automatically clock_unregister()ed on driver detach. See clock_register() for
 * more information.
 */
struct clk *devm_clock_register(vmm_device_t *dev, struct clock_hw *hw)
{
    struct clk  *clk;
    struct clk **clkp;

    clkp = vmm_device_resource_alloc(devm_clock_release, sizeof(*clkp));

    if (!clkp) {
        return VMM_ERR_PTR(VMM_ENOMEM);
    }

    clk = clock_register(dev, hw);

    if (!VMM_IS_ERR(clk)) {
        *clkp = clk;
        vmm_device_resource_add(dev, clkp);
    } else {
        vmm_device_resource_free(clkp);
    }

    return clk;
}

VMM_EXPORT_SYMBOL(devm_clock_register);

/**
 * devm_clock_hw_register - resource managed clock_hw_register()
 * @dev: device that is registering this clock
 * @hw: link to hardware-specific clock data
 *
 * Managed clock_hw_register(). Clocks registered by this function are
 * automatically clock_hw_unregister()ed on driver detach. See clock_hw_register()
 * for more information.
 */
int devm_clock_hw_register(vmm_device_t *dev, struct clock_hw *hw)
{
    struct clock_hw **hwp;
    int               ret;

    hwp = vmm_device_resource_alloc(devm_clock_hw_release, sizeof(*hwp));

    if (!hwp) {
        return VMM_ENOMEM;
    }

    ret = clock_hw_register(dev, hw);

    if (!ret) {
        *hwp = hw;
        vmm_device_resource_add(dev, hwp);
    } else {
        vmm_device_resource_free(hwp);
    }

    return ret;
}

VMM_EXPORT_SYMBOL(devm_clock_hw_register);

static int devm_clock_match(vmm_device_t *dev, void *res, void *data)
{
    struct clk *c = res;

    if (WARN_ON(!c)) {
        return 0;
    }

    return c == data;
}

static int devm_clock_hw_match(vmm_device_t *dev, void *res, void *data)
{
    struct clock_hw *hw = res;

    if (WARN_ON(!hw)) {
        return 0;
    }

    return hw == data;
}

/**
 * devm_clock_unregister - resource managed clock_unregister()
 * @clk: clock to unregister
 *
 * Deallocate a clock allocated with devm_clock_register(). Normally
 * this function will not need to be called and the resource management
 * code will ensure that the resource is freed.
 */
void devm_clock_unregister(vmm_device_t *dev, struct clk *clk)
{
    WARN_ON(vmm_device_resource_release(dev, devm_clock_release, devm_clock_match, clk));
}

VMM_EXPORT_SYMBOL(devm_clock_unregister);

/**
 * devm_clock_hw_unregister - resource managed clock_hw_unregister()
 * @dev: device that is unregistering the hardware-specific clock data
 * @hw: link to hardware-specific clock data
 *
 * Unregister a clock_hw registered with devm_clock_hw_register(). Normally
 * this function will not need to be called and the resource management
 * code will ensure that the resource is freed.
 */
void devm_clock_hw_unregister(vmm_device_t *dev, struct clock_hw *hw)
{
    WARN_ON(vmm_device_resource_release(dev, devm_clock_hw_release, devm_clock_hw_match, hw));
}

VMM_EXPORT_SYMBOL(devm_clock_hw_unregister);

/*
 * clkdev helpers
 */
int __clock_get(struct clk *clk)
{
    struct clock_core *core = !clk ? NULL : clk->core;

    if (core) {
        xref_get(&core->ref);
    }

    return 1;
}

void __clock_put(struct clk *clk)
{
    if (!clk || WARN_ON(VMM_IS_ERR(clk))) {
        return;
    }

    clock_prepare_lock();

    hlist_del(&clk->clks_node);

    if (clk->min_rate > clk->core->req_rate || clk->max_rate < clk->core->req_rate) {
        clock_core_set_rate_nolock(clk->core, clk->core->req_rate);
    }

    xref_put(&clk->core->ref, __clock_release);

    clock_prepare_unlock();

    vmm_free(clk);
}

/***        clk rate change notifiers        ***/

/**
 * clock_notifier_register - add a clk rate change notifier
 * @clk: struct clk * to watch
 * @nb: struct notifier_block * with callback info
 *
 * Request notification when clk's rate changes.  This uses an SRCU
 * notifier because we want it to block and notifier unregistrations are
 * uncommon.  The callbacks associated with the notifier must not
 * re-enter into the clk framework by calling any top-level clk APIs;
 * this will cause a nested prepare_lock mutex.
 *
 * In all notification cases (pre, post and abort rate change) the original
 * clock rate is passed to the callback via struct clock_notifier_data.old_rate
 * and the new frequency is passed via struct clock_notifier_data.new_rate.
 *
 * clock_notifier_register() must be called from non-atomic context.
 * Returns VMM_EINVALID if called with null arguments, VMM_ENOMEM upon
 * allocation failure; otherwise, passes along the return value of
 * vmm_atomic_notifier_register().
 */
int clock_notifier_register(struct clk *clk, vmm_notifier_block_t *nb)
{
    struct clock_notifier *cn;
    int                    ret = VMM_ENOMEM;

    if (!clk || !nb) {
        return VMM_EINVALID;
    }

    clock_prepare_lock();

    /* search the list of notifiers for this clk */

    list_for_each_entry(cn, &clock_notifier_list, node) if (cn->clk == clk) break;

    /* if clk wasn't in the notifier list, allocate new clock_notifier */
    if (cn->clk != clk) {
        cn = vmm_zalloc(sizeof(*cn));

        if (!cn) {
            goto out;
        }

        cn->clk = clk;
        ATOMIC_INIT_NOTIFIER_CHAIN(&cn->notifier_head);

        list_add(&cn->node, &clock_notifier_list);
    }

    ret = vmm_atomic_notifier_register(&cn->notifier_head, nb);

    clk->core->notifier_count++;

out:
    clock_prepare_unlock();

    return ret;
}

VMM_EXPORT_SYMBOL(clock_notifier_register);

/**
 * clock_notifier_unregister - remove a clk rate change notifier
 * @clk: struct clk *
 * @nb: struct notifier_block * with callback info
 *
 * Request no further notification for changes to 'clk' and frees memory
 * allocated in clock_notifier_register.
 *
 * Returns VMM_EINVALID if called with null arguments; otherwise, passes
 * along the return value of vmm_atomic_notifier_unregister().
 */
int clock_notifier_unregister(struct clk *clk, vmm_notifier_block_t *nb)
{
    struct clock_notifier *cn  = NULL;
    int                    ret = VMM_EINVALID;

    if (!clk || !nb) {
        return VMM_EINVALID;
    }

    clock_prepare_lock();

    list_for_each_entry(cn, &clock_notifier_list, node) if (cn->clk == clk) break;

    if (cn->clk == clk) {
        ret = vmm_atomic_notifier_unregister(&cn->notifier_head, nb);

        clk->core->notifier_count--;

        /* XXX the notifier code should handle this better */
        if (!cn->notifier_head.head) {
            list_del(&cn->node);
            vmm_free(cn);
        }
    } else {
        ret = VMM_ENOENT;
    }

    clock_prepare_unlock();

    return ret;
}

VMM_EXPORT_SYMBOL(clock_notifier_unregister);

#ifdef CONFIG_OF
/**
 * struct of_clock_provider - Clock provider registration structure
 * @link: Entry in global list of clock providers
 * @node: Pointer to device tree node of clock provider
 * @get: Get clock callback.  Returns NULL or a struct clk for the
 *       given clock specifier
 * @data: context pointer to be passed into @get callback
 */
struct of_clock_provider {
    list_head_t link;

    vmm_device_tree_node_t *node;
    struct clk *(*get)(struct vmm_device_tree_phandle_args *clkspec, void *data);
    struct clock_hw *(*get_hw)(struct vmm_device_tree_phandle_args *clkspec, void *data);
    void *data;
};

static LIST_HEAD(of_clock_providers);
static DEFINE_SPINLOCK(of_clock_slock);

struct clk *of_clock_src_simple_get(struct vmm_device_tree_phandle_args *clkspec, void *data)
{
    return data;
}

VMM_EXPORT_SYMBOL(of_clock_src_simple_get);

struct clock_hw *of_clock_hw_simple_get(struct vmm_device_tree_phandle_args *clkspec, void *data)
{
    return data;
}

VMM_EXPORT_SYMBOL(of_clock_hw_simple_get);

struct clk *of_clock_src_onecell_get(struct vmm_device_tree_phandle_args *clkspec, void *data)
{
    struct clock_onecell_data *clock_data = data;
    uint32_t                   idx        = clkspec->args[0];

    if (idx >= clock_data->clock_num) {
        vmm_lerror(__func__, "invalid clock index %u\n", idx);
        return VMM_ERR_PTR(VMM_EINVALID);
    }

    return clock_data->clks[idx];
}

VMM_EXPORT_SYMBOL(of_clock_src_onecell_get);

struct clock_hw *of_clock_hw_onecell_get(struct vmm_device_tree_phandle_args *clkspec, void *data)
{
    struct clock_hw_onecell_data *hw_data = data;
    uint32_t                      idx     = clkspec->args[0];

    if (idx >= hw_data->num) {
        vmm_lerror(__func__, "invalid index %u\n", idx);
        return VMM_ERR_PTR(VMM_EINVALID);
    }

    return hw_data->hws[idx];
}

VMM_EXPORT_SYMBOL(of_clock_hw_onecell_get);

/**
 * of_clock_add_provider() - Register a clock provider for a node
 * @np: Device node pointer associated with clock provider
 * @clock_src_get: callback for decoding clock
 * @data: context pointer for @clock_src_get callback.
 */
int of_clock_add_provider(
    vmm_device_tree_node_t *np, struct clk *(*clock_src_get)(struct vmm_device_tree_phandle_args *clkspec, void *data), void *data)
{
    struct of_clock_provider *cp;
    int                       ret;

    cp = vmm_zalloc(sizeof(struct of_clock_provider));

    if (!cp) {
        return VMM_ENOMEM;
    }

    cp->node = vmm_device_tree_ref_node(np);
    cp->data = data;
    cp->get  = clock_src_get;

    vmm_spin_lock(&of_clock_slock);
    list_add(&cp->link, &of_clock_providers);
    vmm_spin_unlock(&of_clock_slock);

    ret = of_clock_set_defaults(np, true);

    if (ret < 0) {
        of_clock_del_provider(np);
    }

    return ret;
}

VMM_EXPORT_SYMBOL(of_clock_add_provider);

/**
 * of_clock_add_hw_provider() - Register a clock provider for a node
 * @np: Device node pointer associated with clock provider
 * @get: callback for decoding clock_hw
 * @data: context pointer for @get callback.
 */
int of_clock_add_hw_provider(
    vmm_device_tree_node_t *np, struct clock_hw *(*get)(struct vmm_device_tree_phandle_args *clkspec, void *data), void *data)
{
    struct of_clock_provider *cp;
    int                       ret;

    cp = vmm_zalloc(sizeof(*cp));

    if (!cp) {
        return VMM_ENOMEM;
    }

    cp->node   = vmm_device_tree_ref_node(np);
    cp->data   = data;
    cp->get_hw = get;

    vmm_spin_lock(&of_clock_slock);
    list_add(&cp->link, &of_clock_providers);
    vmm_spin_unlock(&of_clock_slock);

    ret = of_clock_set_defaults(np, true);

    if (ret < 0) {
        of_clock_del_provider(np);
    }

    return ret;
}

VMM_EXPORT_SYMBOL(of_clock_add_hw_provider);

static void devm_of_clock_release_provider(vmm_device_t *dev, void *res)
{
    of_clock_del_provider(*(vmm_device_tree_node_t **)res);
}

int devm_of_clock_add_hw_provider(vmm_device_t *dev, struct clock_hw *(*get)(struct vmm_device_tree_phandle_args *clkspec, void *data), void *data)
{
    vmm_device_tree_node_t **ptr, *np;
    int                      ret;

    ptr = vmm_device_resource_alloc(devm_of_clock_release_provider, sizeof(*ptr));

    if (!ptr) {
        return VMM_ENOMEM;
    }

    np  = dev->of_node;
    ret = of_clock_add_hw_provider(np, get, data);

    if (!ret) {
        *ptr = np;
        vmm_device_resource_add(dev, ptr);
    } else {
        vmm_device_resource_free(ptr);
    }

    return ret;
}

VMM_EXPORT_SYMBOL(devm_of_clock_add_hw_provider);

/**
 * of_clock_del_provider() - Remove a previously registered clock provider
 * @np: Device node pointer associated with clock provider
 */
void of_clock_del_provider(vmm_device_tree_node_t *np)
{
    struct of_clock_provider *cp;

    vmm_spin_lock(&of_clock_slock);
    list_for_each_entry(cp, &of_clock_providers, link)
    {
        if (cp->node == np) {
            list_del(&cp->link);
            vmm_device_tree_dref_node(cp->node);
            vmm_free(cp);
            break;
        }
    }
    vmm_spin_unlock(&of_clock_slock);
}

VMM_EXPORT_SYMBOL(of_clock_del_provider);

static int devm_clock_provider_match(vmm_device_t *dev, void *res, void *data)
{
    vmm_device_tree_node_t **np = res;

    if (WARN_ON(!np || !*np)) {
        return 0;
    }

    return *np == data;
}

void devm_of_clock_del_provider(vmm_device_t *dev)
{
    int ret;

    ret = vmm_device_resource_release(dev, devm_of_clock_release_provider, devm_clock_provider_match, dev->of_node);

    WARN_ON(ret);
}

VMM_EXPORT_SYMBOL(devm_of_clock_del_provider);

static struct clock_hw *__of_clock_get_hw_from_provider(struct of_clock_provider *provider, struct vmm_device_tree_phandle_args *clkspec)
{
    struct clk *clk;

    if (provider->get_hw) {
        return provider->get_hw(clkspec, provider->data);
    }

    clk = provider->get(clkspec, provider->data);

    if (VMM_IS_ERR(clk)) {
        return VMM_ERR_CAST(clk);
    }

    return __clock_get_hw(clk);
}

struct clk *__of_clock_get_from_provider(struct vmm_device_tree_phandle_args *clkspec, const char *dev_id, const char *con_id)
{
    struct of_clock_provider *provider;
    struct clk               *clk = VMM_ERR_PTR(VMM_EPROBE_DEFER);
    struct clock_hw          *hw;

    if (!clkspec) {
        return VMM_ERR_PTR(VMM_EINVALID);
    }

    /* Check if we have such a provider in our array */
    vmm_spin_lock(&of_clock_slock);
    list_for_each_entry(provider, &of_clock_providers, link)
    {
        if (provider->node == clkspec->np) {
            hw  = __of_clock_get_hw_from_provider(provider, clkspec);
            clk = __clock_create_clock(hw, dev_id, con_id);
        }

        if (!VMM_IS_ERR(clk)) {
            if (!__clock_get(clk)) {
                __clock_free_clock(clk);
                clk = VMM_ERR_PTR(VMM_ENOENT);
            }

            break;
        }
    }
    vmm_spin_unlock(&of_clock_slock);

    return clk;
}

/**
 * of_clock_get_from_provider() - Lookup a clock from a clock provider
 * @clkspec: pointer to a clock specifier data structure
 *
 * This function looks up a struct clk from the registered list of clock
 * providers, an input is a clock specifier data structure as returned
 * from the of_parse_phandle_with_args() function call.
 */
struct clk *of_clock_get_from_provider(struct vmm_device_tree_phandle_args *clkspec)
{
    return __of_clock_get_from_provider(clkspec, NULL, __func__);
}

VMM_EXPORT_SYMBOL(of_clock_get_from_provider);

/**
 * of_clock_get_parent_count() - Count the number of clocks a device node has
 * @np: device node to count
 *
 * Returns: The number of clocks that are possible parents of this node
 */
uint32_t of_clock_get_parent_count(vmm_device_tree_node_t *np)
{
    int count;

    count = vmm_device_tree_count_phandle_with_args(np, "clocks", "#clock-cells");

    if (count < 0) {
        return 0;
    }

    return count;
}

VMM_EXPORT_SYMBOL(of_clock_get_parent_count);

const char *of_clock_get_parent_name(vmm_device_tree_node_t *np, int index)
{
    struct vmm_device_tree_phandle_args clkspec;
    struct vmm_device_tree_attr        *prop;
    const char                         *clock_name;
    const uint32_t                     *vp;
    uint32_t                            pv;
    int                                 rc;
    int                                 count;
    struct clk                         *clk;

    rc = vmm_device_tree_parse_phandle_with_args(np, "clocks", "#clock-cells", index, &clkspec);

    if (rc) {
        return NULL;
    }

    index = clkspec.args_count ? clkspec.args[0] : 0;
    count = 0;

    /* if there is an indices property, use it to transfer the index
     * specified into an array offset for the clock-output-names property.
     */
    vmm_device_tree_for_each_u32(clkspec.np, "clock-indices", prop, vp, pv)
    {
        if (index == pv) {
            index = count;
            break;
        }

        count++;
    }

    /* We went off the end of 'clock-indices' without finding it */
    if (prop && !vp) {
        return NULL;
    }

    if (vmm_device_tree_string_index(clkspec.np, "clock-output-names", index, &clock_name) < 0) {
        /*
         * Best effort to get the name if the clock has been
         * registered with the framework. If the clock isn't
         * registered, we return the node name as the name of
         * the clock as long as #clock-cells = 0.
         */
        clk = of_clock_get_from_provider(&clkspec);

        if (VMM_IS_ERR(clk)) {
            if (clkspec.args_count == 0) {
                clock_name = clkspec.np->name;
            } else {
                clock_name = NULL;
            }
        } else {
            clock_name = __clock_get_name(clk);
            clock_put(clk);
        }
    }

    vmm_device_tree_dref_node(clkspec.np);
    return clock_name;
}

VMM_EXPORT_SYMBOL(of_clock_get_parent_name);

/**
 * of_clock_parent_fill() - Fill @parents with names of @np's parents and return
 * number of parents
 * @np: Device node pointer associated with clock provider
 * @parents: pointer to char array that hold the parents' names
 * @size: size of the @parents array
 *
 * Return: number of parents for the clock node.
 */
int of_clock_parent_fill(vmm_device_tree_node_t *np, const char **parents, uint32_t size)
{
    uint32_t i = 0;

    while (i < size && (parents[i] = of_clock_get_parent_name(np, i)) != NULL) {
        i++;
    }

    return i;
}

VMM_EXPORT_SYMBOL(of_clock_parent_fill);

static void of_clock_init_matching(vmm_device_tree_node_t *np, const struct vmm_device_tree_nodeid *match, void *data)
{
    of_clock_init_cb_t clock_init_cb = match->data;

    if (clock_init_cb) {
        clock_init_cb(np);
    }
}

/**
 * of_clock_init() - Scan and init clock providers from the DT
 * @matches: array of compatible values and init functions for providers.
 *
 * This function scans the device tree for matching clock providers
 * and calls their initialization functions. It also does it by trying
 * to follow the dependencies.
 */
void __init of_clock_init(const struct vmm_device_tree_nodeid *matches)
{
    bool destroy_matches = FALSE;

    /* array of compatible values is mandatory for us */
    if (!matches) {
        matches = vmm_device_tree_nidtable_create_matches("clk-provider");

        if (!matches) {
            return;
        }

        destroy_matches = TRUE;
    }

    /* Iterate over each matching node */
    vmm_device_tree_iterate_matching(NULL, matches, of_clock_init_matching, NULL);

    /* If required destroy matches table */
    if (destroy_matches) {
        vmm_device_tree_nidtable_destroy_matches(matches);
    }
}
#endif
