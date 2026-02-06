/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file clk.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic interface for clocking framework
 *
 * Adapted from linux/include/linux/clk.h
 *
 *  Copyright (C) 2004 ARM Limited.
 *  Written by Deep Blue Solutions Limited.
 *  Copyright (C) 2011-2012 Linaro Ltd <mturquette@linaro.org>
 *
 * The original source is licensed under GPL.
 */
#ifndef __CLK_H__
#define __CLK_H__

#include <libs/list.h>
#include <vmm_notifier.h>
#include <vmm_types.h>

struct vmm_device;
struct vmm_device_tree_node;
typedef struct vmm_device_tree_node vmm_device_tree_node_t;
struct vmm_device_tree_phandle_args;
struct clk;

typedef struct vmm_device vmm_device_t;

/**
 * DOC: clk notifier callback types
 *
 * PRE_RATE_CHANGE - called immediately before the clk rate is changed,
 *     to indicate that the rate change will proceed.  Drivers must
 *     immediately terminate any operations that will be affected by the
 *     rate change.  Callbacks may either return NOTIFY_DONE, NOTIFY_OK,
 *     NOTIFY_STOP or NOTIFY_BAD.
 *
 * ABORT_RATE_CHANGE: called if the rate change failed for some reason
 *     after PRE_RATE_CHANGE.  In this case, all registered notifiers on
 *     the clk will be called with ABORT_RATE_CHANGE. Callbacks must
 *     always return NOTIFY_DONE or NOTIFY_OK.
 *
 * POST_RATE_CHANGE - called after the clk rate change has successfully
 *     completed.  Callbacks must always return NOTIFY_DONE or NOTIFY_OK.
 *
 */
#define PRE_RATE_CHANGE   BIT(0)
#define POST_RATE_CHANGE  BIT(1)
#define ABORT_RATE_CHANGE BIT(2)

/**
 * struct clock_notifier - associate a clk with a notifier
 * @clk: struct clk * to associate the notifier with
 * @notifier_head: a blocking_notifier_head for this clk
 * @node: linked list pointers
 *
 * A list of struct clock_notifier is maintained by the notifier code.
 * An entry is created whenever code registers the first notifier on a
 * particular @clk.  Future notifiers on that @clk are added to the
 * @notifier_head.
 */
struct clock_notifier {
    struct clk                 *clk;
    vmm_atomic_notifier_chain_t notifier_head;
    list_head_t                 node;
};

/**
 * struct clock_notifier_data - rate data to pass to the notifier callback
 * @clk: struct clk * being changed
 * @old_rate: previous rate of this clk
 * @new_rate: new rate of this clk
 *
 * For a pre-notifier, old_rate is the clk's rate before this rate
 * change, and new_rate is what the rate will be in the future.  For a
 * post-notifier, old_rate and new_rate are both set to the clk's
 * current rate (this was done to optimize the implementation).
 */
struct clock_notifier_data {
    struct clk *clk;
    uint64_t    old_rate;
    uint64_t    new_rate;
};

/**
 * struct clock_bulk_data - Data used for bulk clk operations.
 *
 * @id: clock consumer ID
 * @clk: struct clk * to store the associated clock
 *
 * The CLK APIs provide a series of clock_bulk_() API calls as
 * a convenience to consumers which require multiple clks.  This
 * structure is used to manage data for these calls.
 */
struct clock_bulk_data {
    const char *id;
    struct clk *clk;
};

#ifdef CONFIG_COMMON_CLK

/**
 * clock_notifier_register: register a clock rate-change notifier callback
 * @clk: clock whose rate we are interested in
 * @nb: notifier block with callback function pointer
 *
 * ProTip: debugging across notifier chains can be frustrating. Make sure that
 * your notifier callback function prints a nice big warning in case of
 * failure.
 */
int clock_notifier_register(struct clk *clk, vmm_notifier_block_t *nb);

/**
 * clock_notifier_unregister: unregister a clock rate-change notifier callback
 * @clk: clock whose rate we are no longer interested in
 * @nb: notifier block which will be unregistered
 */
int clock_notifier_unregister(struct clk *clk, vmm_notifier_block_t *nb);

/**
 * clock_get_accuracy - obtain the clock accuracy in ppb (parts per billion)
 *            for a clock source.
 * @clk: clock source
 *
 * This gets the clock source accuracy expressed in ppb.
 * A perfect clock returns 0.
 */
long clock_get_accuracy(struct clk *clk);

/**
 * clock_set_phase - adjust the phase shift of a clock signal
 * @clk: clock signal source
 * @degrees: number of degrees the signal is shifted
 *
 * Shifts the phase of a clock signal by the specified degrees. Returns 0 on
 * success, VMM_EERROR otherwise.
 */
int clock_set_phase(struct clk *clk, int degrees);

/**
 * clock_get_phase - return the phase shift of a clock signal
 * @clk: clock signal source
 *
 * Returns the phase shift of a clock node in degrees, otherwise returns
 * VMM_EERROR.
 */
int clock_get_phase(struct clk *clk);

/**
 * clock_is_match - check if two clk's point to the same hardware clock
 * @p: clk compared against q
 * @q: clk compared against p
 *
 * Returns true if the two struct clk pointers both point to the same hardware
 * clock node. Put differently, returns true if @p and @q
 * share the same &struct clock_core object.
 *
 * Returns false otherwise. Note that two NULL clks are treated as matching.
 */
bool clock_is_match(const struct clk *p, const struct clk *q);

#else

static inline int clock_notifier_register(struct clk *clk, vmm_notifier_block_t *nb)
{
    return VMM_ENOTSUPP;
}

static inline int clock_notifier_unregister(struct clk *clk, vmm_notifier_block_t *nb)
{
    return VMM_ENOTSUPP;
}

static inline long clock_get_accuracy(struct clk *clk)
{
    return VMM_ENOTSUPP;
}

static inline long clock_set_phase(struct clk *clk, int phase)
{
    return VMM_ENOTSUPP;
}

static inline long clock_get_phase(struct clk *clk)
{
    return VMM_ENOTSUPP;
}

static inline bool clock_is_match(const struct clk *p, const struct clk *q)
{
    return p == q;
}

#endif

/**
 * clock_prepare - prepare a clock source
 * @clk: clock source
 *
 * This prepares the clock source for use.
 *
 * Must not be called from within atomic context.
 */
#ifdef CONFIG_HAVE_CLK_PREPARE
int clock_prepare(struct clk *clk);
int clock_bulk_prepare(int num_clocks, const struct clock_bulk_data *clks);
#else
static inline int clock_prepare(struct clk *clk)
{
    return 0;
}

static inline int clock_bulk_prepare(int num_clocks, struct clock_bulk_data *clks)
{
    return 0;
}
#endif

/**
 * clock_unprepare - undo preparation of a clock source
 * @clk: clock source
 *
 * This undoes a previously prepared clock.  The caller must balance
 * the number of prepare and unprepare calls.
 *
 * Must not be called from within atomic context.
 */
#ifdef CONFIG_HAVE_CLK_PREPARE
void clock_unprepare(struct clk *clk);
void clock_bulk_unprepare(int num_clocks, const struct clock_bulk_data *clks);
#else
static inline void clock_unprepare(struct clk *clk) {}

static inline void clock_bulk_unprepare(int num_clocks, struct clock_bulk_data *clks) {}
#endif

#ifdef CONFIG_HAVE_CLK
/**
 * clock_get - lookup and obtain a reference to a clock producer.
 * @dev: device for clock "consumer"
 * @id: clock consumer ID
 *
 * Returns a struct clk corresponding to the clock producer, or
 * valid IS_ERR() condition containing errno.  The implementation
 * uses @dev and @id to determine the clock consumer, and thereby
 * the clock producer.  (IOW, @id may be identical strings, but
 * clock_get may return different clock producers depending on @dev.)
 *
 * Drivers must assume that the clock source is not enabled.
 *
 * clock_get should not be called from within interrupt context.
 */
struct clk *clock_get(vmm_device_t *dev, const char *id);

/**
 * clock_bulk_get - lookup and obtain a number of references to clock producer.
 * @dev: device for clock "consumer"
 * @num_clocks: the number of clock_bulk_data
 * @clks: the clock_bulk_data table of consumer
 *
 * This helper function allows drivers to get several clk consumers in one
 * operation. If any of the clk cannot be acquired then any clks
 * that were obtained will be freed before returning to the caller.
 *
 * Returns 0 if all clocks specified in clock_bulk_data table are obtained
 * successfully, or valid IS_ERR() condition containing errno.
 * The implementation uses @dev and @clock_bulk_data.id to determine the
 * clock consumer, and thereby the clock producer.
 * The clock returned is stored in each @clock_bulk_data.clk field.
 *
 * Drivers must assume that the clock source is not enabled.
 *
 * clock_bulk_get should not be called from within interrupt context.
 */
int clock_bulk_get(vmm_device_t *dev, int num_clocks, struct clock_bulk_data *clks);

/**
 * devm_clock_bulk_get - managed get multiple clk consumers
 * @dev: device for clock "consumer"
 * @num_clocks: the number of clock_bulk_data
 * @clks: the clock_bulk_data table of consumer
 *
 * Return 0 on success, an errno on failure.
 *
 * This helper function allows drivers to get several clk
 * consumers in one operation with management, the clks will
 * automatically be freed when the device is unbound.
 */
int devm_clock_bulk_get(vmm_device_t *dev, int num_clocks, struct clock_bulk_data *clks);

/**
 * devm_clock_get - lookup and obtain a managed reference to a clock producer.
 * @dev: device for clock "consumer"
 * @id: clock consumer ID
 *
 * Returns a struct clk corresponding to the clock producer, or
 * valid IS_ERR() condition containing errno.  The implementation
 * uses @dev and @id to determine the clock consumer, and thereby
 * the clock producer.  (IOW, @id may be identical strings, but
 * clock_get may return different clock producers depending on @dev.)
 *
 * Drivers must assume that the clock source is not enabled.
 *
 * devm_clock_get should not be called from within interrupt context.
 *
 * The clock will automatically be freed when the device is unbound
 * from the bus.
 */
struct clk *devm_clock_get(vmm_device_t *dev, const char *id);

/**
 * devm_get_clock_from_child - lookup and obtain a managed reference to a
 *               clock producer from child node.
 * @dev: device for clock "consumer"
 * @np: pointer to clock consumer node
 * @con_id: clock consumer ID
 *
 * This function parses the clocks, and uses them to look up the
 * struct clk from the registered list of clock providers by using
 * @np and @con_id
 *
 * The clock will automatically be freed when the device is unbound
 * from the bus.
 */
struct clk *devm_get_clock_from_child(vmm_device_t *dev, vmm_device_tree_node_t *np, const char *con_id);

/**
 * clock_enable - inform the system when the clock source should be running.
 * @clk: clock source
 *
 * If the clock can not be enabled/disabled, this should return success.
 *
 * May be called from atomic contexts.
 *
 * Returns success (0) or negative errno.
 */
int clock_enable(struct clk *clk);

/**
 * clock_bulk_enable - inform the system when the set of clks should be running.
 * @num_clocks: the number of clock_bulk_data
 * @clks: the clock_bulk_data table of consumer
 *
 * May be called from atomic contexts.
 *
 * Returns success (0) or negative errno.
 */
int clock_bulk_enable(int num_clocks, const struct clock_bulk_data *clks);

/**
 * clock_disable - inform the system when the clock source is no longer required.
 * @clk: clock source
 *
 * Inform the system that a clock source is no longer required by
 * a driver and may be shut down.
 *
 * May be called from atomic contexts.
 *
 * Implementation detail: if the clock source is shared between
 * multiple drivers, clock_enable() calls must be balanced by the
 * same number of clock_disable() calls for the clock source to be
 * disabled.
 */
void clock_disable(struct clk *clk);

/**
 * clock_bulk_disable - inform the system when the set of clks is no
 *            longer required.
 * @num_clocks: the number of clock_bulk_data
 * @clks: the clock_bulk_data table of consumer
 *
 * Inform the system that a set of clks is no longer required by
 * a driver and may be shut down.
 *
 * May be called from atomic contexts.
 *
 * Implementation detail: if the set of clks is shared between
 * multiple drivers, clock_bulk_enable() calls must be balanced by the
 * same number of clock_bulk_disable() calls for the clock source to be
 * disabled.
 */
void clock_bulk_disable(int num_clocks, const struct clock_bulk_data *clks);

/**
 * clock_get_rate - obtain the current clock rate (in Hz) for a clock source.
 *        This is only valid once the clock source has been enabled.
 * @clk: clock source
 */
uint64_t clock_get_rate(struct clk *clk);

/**
 * clock_put    - "free" the clock source
 * @clk: clock source
 *
 * Note: drivers must ensure that all clock_enable calls made on this
 * clock source are balanced by clock_disable calls prior to calling
 * this function.
 *
 * clock_put should not be called from within interrupt context.
 */
void clock_put(struct clk *clk);

/**
 * clock_bulk_put   - "free" the clock source
 * @num_clocks: the number of clock_bulk_data
 * @clks: the clock_bulk_data table of consumer
 *
 * Note: drivers must ensure that all clock_bulk_enable calls made on this
 * clock source are balanced by clock_bulk_disable calls prior to calling
 * this function.
 *
 * clock_bulk_put should not be called from within interrupt context.
 */
void clock_bulk_put(int num_clocks, struct clock_bulk_data *clks);

/**
 * devm_clock_put   - "free" a managed clock source
 * @dev: device used to acquire the clock
 * @clk: clock source acquired with devm_clock_get()
 *
 * Note: drivers must ensure that all clock_enable calls made on this
 * clock source are balanced by clock_disable calls prior to calling
 * this function.
 *
 * clock_put should not be called from within interrupt context.
 */
void devm_clock_put(vmm_device_t *dev, struct clk *clk);

/*
 * The remaining APIs are optional for machine class support.
 */

/**
 * clock_round_rate - adjust a rate to the exact rate a clock can provide
 * @clk: clock source
 * @rate: desired clock rate in Hz
 *
 * This answers the question "if I were to pass @rate to clock_set_rate(),
 * what clock rate would I end up with?" without changing the hardware
 * in any way.  In other words:
 *
 *   rate = clock_round_rate(clk, r);
 *
 * and:
 *
 *   clock_set_rate(clk, r);
 *   rate = clock_get_rate(clk);
 *
 * are equivalent except the former does not modify the clock hardware
 * in any way.
 *
 * Returns rounded clock rate in Hz, or negative errno.
 */
long clock_round_rate(struct clk *clk, uint64_t rate);

/**
 * clock_set_rate - set the clock rate for a clock source
 * @clk: clock source
 * @rate: desired clock rate in Hz
 *
 * Returns success (0) or negative errno.
 */
int clock_set_rate(struct clk *clk, uint64_t rate);

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
bool clock_has_parent(struct clk *clk, struct clk *parent);

/**
 * clock_set_rate_range - set a rate range for a clock source
 * @clk: clock source
 * @min: desired minimum clock rate in Hz, inclusive
 * @max: desired maximum clock rate in Hz, inclusive
 *
 * Returns success (0) or negative errno.
 */
int clock_set_rate_range(struct clk *clk, uint64_t min, uint64_t max);

/**
 * clock_set_min_rate - set a minimum clock rate for a clock source
 * @clk: clock source
 * @rate: desired minimum clock rate in Hz, inclusive
 *
 * Returns success (0) or negative errno.
 */
int clock_set_min_rate(struct clk *clk, uint64_t rate);

/**
 * clock_set_max_rate - set a maximum clock rate for a clock source
 * @clk: clock source
 * @rate: desired maximum clock rate in Hz, inclusive
 *
 * Returns success (0) or negative errno.
 */
int clock_set_max_rate(struct clk *clk, uint64_t rate);

/**
 * clock_set_parent - set the parent clock source for this clock
 * @clk: clock source
 * @parent: parent clock source
 *
 * Returns success (0) or negative errno.
 */
int clock_set_parent(struct clk *clk, struct clk *parent);

/**
 * clock_get_parent - get the parent clock source for this clock
 * @clk: clock source
 *
 * Returns struct clk corresponding to parent clock source, or
 * valid VMM_IS_ERR() condition containing errno.
 */
struct clk *clock_get_parent(struct clk *clk);

/**
 * clock_get_sys - get a clock based upon the device name
 * @dev_id: device name
 * @con_id: connection ID
 *
 * Returns a struct clk corresponding to the clock producer, or
 * valid VMM_IS_ERR() condition containing errno.  The implementation
 * uses @dev_id and @con_id to determine the clock consumer, and
 * thereby the clock producer. In contrast to clock_get() this function
 * takes the device name instead of the device itself for identification.
 *
 * Drivers must assume that the clock source is not enabled.
 *
 * clock_get_sys should not be called from within interrupt context.
 */
struct clk *clock_get_sys(const char *dev_id, const char *con_id);

#else /* !CONFIG_HAVE_CLK */

static inline struct clk *clock_get(vmm_device_t *dev, const char *id)
{
    return NULL;
}

static inline int clock_bulk_get(vmm_device_t *dev, int num_clocks, struct clock_bulk_data *clks)
{
    return 0;
}

static inline struct clk *devm_clock_get(vmm_device_t *dev, const char *id)
{
    return NULL;
}

static inline int devm_clock_bulk_get(vmm_device_t *dev, int num_clocks, struct clock_bulk_data *clks)
{
    return 0;
}

static inline struct clk *devm_get_clock_from_child(vmm_device_t *dev, vmm_device_tree_node_t *np, const char *con_id)
{
    return NULL;
}

static inline void clock_put(struct clk *clk) {}

static inline void clock_bulk_put(int num_clocks, struct clock_bulk_data *clks) {}

static inline void devm_clock_put(vmm_device_t *dev, struct clk *clk) {}

static inline int clock_enable(struct clk *clk)
{
    return 0;
}

static inline int clock_bulk_enable(int num_clocks, struct clock_bulk_data *clks)
{
    return 0;
}

static inline void clock_disable(struct clk *clk) {}

static inline void clock_bulk_disable(int num_clocks, struct clock_bulk_data *clks) {}

static inline uint64_t clock_get_rate(struct clk *clk)
{
    return 0;
}

static inline int clock_set_rate(struct clk *clk, uint64_t rate)
{
    return 0;
}

static inline long clock_round_rate(struct clk *clk, uint64_t rate)
{
    return 0;
}

static inline bool clock_has_parent(struct clk *clk, struct clk *parent)
{
    return true;
}

static inline int clock_set_parent(struct clk *clk, struct clk *parent)
{
    return 0;
}

static inline struct clk *clock_get_parent(struct clk *clk)
{
    return NULL;
}

static inline struct clk *clock_get_sys(const char *dev_id, const char *con_id)
{
    return NULL;
}
#endif

/* clock_prepare_enable helps cases using clock_enable in non-atomic context. */
static inline int clock_prepare_enable(struct clk *clk)
{
    int ret;

    ret = clock_prepare(clk);

    if (ret) {
        return ret;
    }

    ret = clock_enable(clk);

    if (ret) {
        clock_unprepare(clk);
    }

    return ret;
}

/* clock_disable_unprepare helps cases using clock_disable in non-atomic context. */
static inline void clock_disable_unprepare(struct clk *clk)
{
    clock_disable(clk);
    clock_unprepare(clk);
}

static inline int clock_bulk_prepare_enable(int num_clocks, struct clock_bulk_data *clks)
{
    int ret;

    ret = clock_bulk_prepare(num_clocks, clks);

    if (ret) {
        return ret;
    }

    ret = clock_bulk_enable(num_clocks, clks);

    if (ret) {
        clock_bulk_unprepare(num_clocks, clks);
    }

    return ret;
}

static inline void clock_bulk_disable_unprepare(int num_clocks, struct clock_bulk_data *clks)
{
    clock_bulk_disable(num_clocks, clks);
    clock_bulk_unprepare(num_clocks, clks);
}

#if defined(CONFIG_OF) && defined(CONFIG_COMMON_CLK)
struct clk *of_clock_get(vmm_device_tree_node_t *np, int index);
struct clk *of_clock_get_by_name(vmm_device_tree_node_t *np, const char *name);
struct clk *of_clock_get_from_provider(struct vmm_device_tree_phandle_args *clkspec);
#else
static inline struct clk *of_clock_get(vmm_device_tree_node_t *np, int index)
{
    return VMM_ERR_PTR(VMM_ENOENT);
}

static inline struct clk *of_clock_get_by_name(vmm_device_tree_node_t *np, const char *name)
{
    return VMM_ERR_PTR(VMM_ENOENT);
}

static inline struct clk *of_clock_get_from_provider(struct vmm_device_tree_phandle_args *clkspec)
{
    return VMM_ERR_PTR(VMM_ENOENT);
}
#endif

/* Xvisor specific clock APIs */
#if defined(CONFIG_COMMON_CLK)
int clock_dump(vmm_char_device_t *cdev);
int clock_summary_show(vmm_char_device_t *cdev);
int clock_disable_unused(void);
#endif

#endif
