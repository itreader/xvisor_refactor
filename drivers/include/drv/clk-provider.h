/*
 * Copyright (C) 2014 Anup Patel
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file clk-provider.h
 * @author Anup Patel (anup@brainfault.org)
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Clock provider APIs
 *
 * Adapted from linux/include/linux/clk-provider.h
 *
 *  Copyright (c) 2010-2011 Jeremy Kerr <jeremy.kerr@canonical.com>
 *  Copyright (C) 2011-2012 Linaro Ltd <mturquette@linaro.org>
 *
 * The original source is licensed under GPL.
 */
#ifndef __CLK_PROVIDER_H__
#define __CLK_PROVIDER_H__

#include <drv/clk.h>
#include <vmm_device_driver.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_host_io.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#ifdef CONFIG_COMMON_CLK

/*
 * flags used across common struct clk.  these flags should only affect the
 * top-level framework.  custom flags for dealing with hardware specifics
 * belong in struct clock_foo
 */
#define CLK_SET_RATE_GATE        BIT(0)  /* must be gated across rate change */
#define CLK_SET_PARENT_GATE      BIT(1)  /* must be gated across re-parent */
#define CLK_SET_RATE_PARENT      BIT(2)  /* propagate rate change up one level */
#define CLK_IGNORE_UNUSED        BIT(3)  /* do not gate even if unused */
#define CLK_IS_ROOT              BIT(4)  /* root clk, has no parent */
#define CLK_IS_BASIC             BIT(5)  /* Basic clk, can't do a to_clock_foo() */
#define CLK_GET_RATE_NOCACHE     BIT(6)  /* do not use the cached clk rate */
#define CLK_SET_RATE_NO_REPARENT BIT(7)  /* don't re-parent on rate change */
#define CLK_GET_ACCURACY_NOCACHE BIT(8)  /* do not use the cached clk accuracy */
#define CLK_RECALC_NEW_RATES     BIT(9)  /* recalc rates after notifications */
#define CLK_SET_RATE_UNGATE      BIT(10) /* clock needs to run to set rate */
#define CLK_IS_CRITICAL          BIT(11) /* do not gate, ever */
/* parents need enable during gate/ungate, set rate and re-parent */
#define CLK_OPS_PARENT_ENABLE    BIT(12)
#define CLK_KEEP_REQ_RATE        BIT(16) /* keep reqrate on parent rate change */

struct clk;
struct clock_hw;
struct clock_core;

/**
 * struct clock_rate_request - Structure encoding the clk constraints that
 * a clock user might require.
 *
 * @rate:       Requested clock rate. This field will be adjusted by
 *          clock drivers according to hardware capabilities.
 * @min_rate:       Minimum rate imposed by clk users.
 * @max_rate:       Maximum rate imposed by clk users.
 * @best_parent_rate:   The best parent rate a parent can provide to fulfill the
 *          requested constraints.
 * @best_parent_hw: The most appropriate parent clock that fulfills the
 *          requested constraints.
 *
 */
struct clock_rate_request {
    uint64_t         rate;
    uint64_t         min_rate;
    uint64_t         max_rate;
    uint64_t         best_parent_rate;
    struct clock_hw *best_parent_hw;
};

/**
 * struct clock_ops -  Callback operations for hardware clocks; these are to
 * be provided by the clock implementation, and will be called by drivers
 * through the clock_* api.
 *
 * @prepare:    Prepare the clock for enabling. This must not return until
 *      the clock is fully prepared, and it's safe to call clock_enable.
 *      This callback is intended to allow clock implementations to
 *      do any initialisation that may sleep. Called with
 *      prepare_lock held.
 *
 * @unprepare:  Release the clock from its prepared state. This will typically
 *      undo any work done in the @prepare callback. Called with
 *      prepare_lock held.
 *
 * @is_prepared: Queries the hardware to determine if the clock is prepared.
 *      This function is allowed to sleep. Optional, if this op is not
 *      set then the prepare count will be used.
 *
 * @unprepare_unused: Unprepare the clock atomically.  Only called from
 *      clock_disable_unused for prepare clocks with special needs.
 *      Called with prepare mutex held. This function may sleep.
 *
 * @enable: Enable the clock atomically. This must not return until the
 *      clock is generating a valid clock signal, usable by consumer
 *      devices. Called with enable_lock held. This function must not
 *      sleep.
 *
 * @disable:    Disable the clock atomically. Called with enable_lock held.
 *      This function must not sleep.
 *
 * @is_enabled: Queries the hardware to determine if the clock is enabled.
 *      This function must not sleep. Optional, if this op is not
 *      set then the enable count will be used.
 *
 * @disable_unused: Disable the clock atomically.  Only called from
 *      clock_disable_unused for gate clocks with special needs.
 *      Called with enable_lock held.  This function must not
 *      sleep.
 *
 * @recalc_rate Recalculate the rate of this clock, by querying hardware. The
 *      parent rate is an input parameter.  It is up to the caller to
 *      ensure that the prepare_mutex is held across this call.
 *      Returns the calculated rate.  Optional, but recommended - if
 *      this op is not set then clock rate will be initialized to 0.
 *
 * @round_rate: Given a target rate as input, returns the closest rate actually
 *      supported by the clock.
 *
 * @determine_rate: Given a target rate as input, returns the closest rate
 *      actually supported by the clock, and optionally the parent clock
 *      that should be used to provide the clock rate.
 *
 * @get_parent: Queries the hardware to determine the parent of a clock.  The
 *      return value is a uint8_t which specifies the index corresponding to
 *      the parent clock.  This index can be applied to either the
 *      .parent_names or .parents arrays.  In short, this function
 *      translates the parent value read from hardware into an array
 *      index.  Currently only called when the clock is initialized by
 *      __clock_init.  This callback is mandatory for clocks with
 *      multiple parents.  It is optional (and unnecessary) for clocks
 *      with 0 or 1 parents.
 *
 * @set_parent: Change the input source of this clock; for clocks with multiple
 *      possible parents specify a new parent by passing in the index
 *      as a uint8_t corresponding to the parent in either the .parent_names
 *      or .parents arrays.  This function in affect translates an
 *      array index into the value programmed into the hardware.
 *      Returns 0 on success, VMM_ERR_ERROR otherwise.
 *
 * @set_rate:   Change the rate of this clock. The requested rate is specified
 *      by the second argument, which should typically be the return
 *      of .round_rate call.  The third argument gives the parent rate
 *      which is likely helpful for most .set_rate implementation.
 *      Returns 0 on success, VMM_ERR_ERROR otherwise.
 *
 * @recalc_accuracy: Recalculate the accuracy of this clock. The clock accuracy
 *      is expressed in ppb (parts per billion). The parent accuracy is
 *      an input parameter.
 *      Returns the calculated accuracy.  Optional - if this op is not
 *      set then clock accuracy will be initialized to parent accuracy
 *      or 0 (perfect clock) if clock has no parent.
 *
 * @set_rate_and_parent: Change the rate and the parent of this clock. The
 *      requested rate is specified by the second argument, which
 *      should typically be the return of .round_rate call.  The
 *      third argument gives the parent rate which is likely helpful
 *      for most .set_rate_and_parent implementation. The fourth
 *      argument gives the parent index. This callback is optional (and
 *      unnecessary) for clocks with 0 or 1 parents as well as
 *      for clocks that can tolerate switching the rate and the parent
 *      separately via calls to .set_parent and .set_rate.
 *      Returns 0 on success, VMM_ERR_ERROR otherwise.
 *
 * @recalc_accuracy: Recalculate the accuracy of this clock. The clock accuracy
 *      is expressed in ppb (parts per billion). The parent accuracy is
 *      an input parameter.
 *      Returns the calculated accuracy.  Optional - if this op is not
 *      set then clock accuracy will be initialized to parent accuracy
 *      or 0 (perfect clock) if clock has no parent.
 *
 * @get_phase:  Queries the hardware to get the current phase of a clock.
 *      Returned values are 0-359 degrees on success, negative
 *      error codes on failure.
 *
 * @set_phase:  Shift the phase this clock signal in degrees specified
 *      by the second argument. Valid values for degrees are
 *      0-359. Return 0 on success, otherwise VMM_ERR_ERROR.
 *
 * @init:   Perform platform-specific initialization magic.
 *      This is not not used by any of the basic clock types.
 *      Please consider other ways of solving initialization problems
 *      before using this callback, as its use is discouraged.
 *
 *
 * The clock_enable/clock_disable and clock_prepare/clock_unprepare pairs allow
 * implementations to split any work between atomic (enable) and sleepable
 * (prepare) contexts.  If enabling a clock requires code that might sleep,
 * this must be done in clock_prepare.  Clock enable code that will never be
 * called in a sleepable context may be implemented in clock_enable.
 *
 * Typically, drivers will call clock_prepare when a clock may be needed later
 * (eg. when a device is opened), and clock_enable when the clock is actually
 * required (eg. from an interrupt). Note that clock_prepare MUST have been
 * called before clock_enable.
 */
struct clock_ops {
    int (*prepare)(struct clock_hw *hw);
    void (*unprepare)(struct clock_hw *hw);
    int (*is_prepared)(struct clock_hw *hw);
    void (*unprepare_unused)(struct clock_hw *hw);
    int (*enable)(struct clock_hw *hw);
    void (*disable)(struct clock_hw *hw);
    int (*is_enabled)(struct clock_hw *hw);
    void (*disable_unused)(struct clock_hw *hw);
    uint64_t (*recalc_rate)(struct clock_hw *hw, uint64_t parent_rate);
    long (*round_rate)(struct clock_hw *hw, uint64_t, uint64_t *);
    int (*determine_rate)(struct clock_hw *hw, struct clock_rate_request *req);
    int (*set_parent)(struct clock_hw *hw, uint8_t index);
    uint8_t (*get_parent)(struct clock_hw *hw);
    int (*set_rate)(struct clock_hw *hw, uint64_t, uint64_t);
    int (*set_rate_and_parent)(struct clock_hw *hw, uint64_t rate, uint64_t parent_rate, uint8_t index);
    uint64_t (*recalc_accuracy)(struct clock_hw *hw, uint64_t parent_accuracy);
    int (*get_phase)(struct clock_hw *hw);
    int (*set_phase)(struct clock_hw *hw, int degrees);
    void (*init)(struct clock_hw *hw);
};

/**
 * struct clock_init_data - holds init data that's common to all clocks and is
 * shared between the clock provider and the common clock framework.
 *
 * @name: clock name
 * @ops: operations this clock supports
 * @parent_names: array of string names for all possible parents
 * @num_parents: number of possible parents
 * @flags: framework-level hints and quirks
 */
struct clock_init_data {
    const char             *name;
    const struct clock_ops *ops;
    const char *const      *parent_names;
    uint8_t                 num_parents;
    uint64_t                flags;
};

/**
 * struct clock_hw - handle for traversing from a struct clk to its corresponding
 * hardware-specific structure.  struct clock_hw should be declared within struct
 * clock_foo and then referenced by the struct clk instance that uses struct
 * clock_foo's clock_ops
 *
 * @core: pointer to the struct clock_core instance that points back to this
 * struct clock_hw instance
 *
 * @clk: pointer to the per-user struct clk instance that can be used to call
 * into the clk API
 *
 * @init: pointer to struct clock_init_data that contains the init data shared
 * with the common clock framework.
 */
struct clock_hw {
    struct clock_core            *core;
    struct clk                   *clk;
    const struct clock_init_data *init;
};

/*
 * DOC: Basic clock implementations common to many platforms
 *
 * Each basic clock hardware type is comprised of a structure describing the
 * clock hardware, implementations of the relevant callbacks in struct clock_ops,
 * unique flags for that hardware type, a registration function and an
 * alternative macro for static initialization
 */

/**
 * struct clock_fixed_rate - fixed-rate clock
 * @hw:     handle between common and hardware-specific interfaces
 * @fixed_rate: constant frequency of clock
 */
struct clock_fixed_rate {
    struct clock_hw hw;
    uint64_t        fixed_rate;
    uint64_t        fixed_accuracy;
    uint8_t         flags;
};

#define to_clock_fixed_rate(_hw) container_of(_hw, struct clock_fixed_rate, hw)

extern const struct clock_ops clock_fixed_rate_ops;
struct clk      *clock_register_fixed_rate(vmm_device_t *dev, const char *name, const char *parent_name, uint64_t flags, uint64_t fixed_rate);
struct clock_hw *clock_hw_register_fixed_rate(vmm_device_t *dev, const char *name, const char *parent_name, uint64_t flags, uint64_t fixed_rate);
struct clk      *clock_register_fixed_rate_with_accuracy(
         vmm_device_t *dev, const char *name, const char *parent_name, uint64_t flags, uint64_t fixed_rate, uint64_t fixed_accuracy);
void             clock_unregister_fixed_rate(struct clk *clk);
struct clock_hw *clock_hw_register_fixed_rate_with_accuracy(
    vmm_device_t *dev, const char *name, const char *parent_name, uint64_t flags, uint64_t fixed_rate, uint64_t fixed_accuracy);
void clock_hw_unregister_fixed_rate(struct clock_hw *hw);

void of_fixed_clock_setup(vmm_device_tree_node_t *np);

/**
 * struct clock_gate - gating clock
 *
 * @hw:     handle between common and hardware-specific interfaces
 * @reg:    register controlling gate
 * @bit_idx:    single bit controlling gate
 * @flags:  hardware-specific flags
 * @lock:   register lock
 *
 * Clock which can gate its output.  Implements .enable & .disable
 *
 * Flags:
 * CLK_GATE_SET_TO_DISABLE - by default this clock sets the bit at bit_idx to
 *  enable the clock.  Setting this flag does the opposite: setting the bit
 *  disable the clock and clearing it enables the clock
 * CLK_GATE_HIWORD_MASK - The gate settings are only in lower 16-bit
 *  of this register, and mask of gate bits are in higher 16-bit of this
 *  register.  While setting the gate bits, higher 16-bit should also be
 *  updated to indicate changing gate bits.
 */
struct clock_gate {
    struct clock_hw hw;
    void           *reg;
    uint8_t         bit_idx;
    uint8_t         flags;
    vmm_spinlock_t *lock;
};

#define to_clock_gate(_hw)      container_of(_hw, struct clock_gate, hw)

#define CLK_GATE_SET_TO_DISABLE BIT(0)
#define CLK_GATE_HIWORD_MASK    BIT(1)

extern const struct clock_ops clock_gate_ops;
struct clk                   *clock_register_gate(
                      vmm_device_t *dev, const char *name, const char *parent_name, uint64_t flags, void *reg, uint8_t bit_idx, uint8_t clock_gate_flags,
                      vmm_spinlock_t *lock);
struct clock_hw *clock_hw_register_gate(
    vmm_device_t *dev, const char *name, const char *parent_name, uint64_t flags, void *reg, uint8_t bit_idx, uint8_t clock_gate_flags,
    vmm_spinlock_t *lock);
void clock_unregister_gate(struct clk *clk);
void clock_hw_unregister_gate(struct clock_hw *hw);
int  clock_gate_is_enabled(struct clock_hw *hw);

struct clock_div_table {
    uint32_t val;
    uint32_t div;
};

/**
 * struct clock_divider - adjustable divider clock
 *
 * @hw:     handle between common and hardware-specific interfaces
 * @reg:    register containing the divider
 * @shift:  shift to the divider bit field
 * @width:  width of the divider bit field
 * @table:  array of value/divider pairs, last entry should have div = 0
 * @lock:   register lock
 *
 * Clock with an adjustable divider affecting its output frequency.  Implements
 * .recalc_rate, .set_rate and .round_rate
 *
 * Flags:
 * CLK_DIVIDER_ONE_BASED - by default the divisor is the value read from the
 *  register plus one.  If CLK_DIVIDER_ONE_BASED is set then the divider is
 *  the raw value read from the register, with the value of zero considered
 *  invalid, unless CLK_DIVIDER_ALLOW_ZERO is set.
 * CLK_DIVIDER_POWER_OF_TWO - clock divisor is 2 raised to the value read from
 *  the hardware register
 * CLK_DIVIDER_ALLOW_ZERO - Allow zero divisors.  For dividers which have
 *  CLK_DIVIDER_ONE_BASED set, it is possible to end up with a zero divisor.
 *  Some hardware implementations gracefully handle this case and allow a
 *  zero divisor by not modifying their input clock
 *  (divide by one / bypass).
 * CLK_DIVIDER_HIWORD_MASK - The divider settings are only in lower 16-bit
 *  of this register, and mask of divider bits are in higher 16-bit of this
 *  register.  While setting the divider bits, higher 16-bit should also be
 *  updated to indicate changing divider bits.
 * CLK_DIVIDER_ROUND_CLOSEST - Makes the best calculated divider to be rounded
 *  to the closest integer instead of the up one.
 * CLK_DIVIDER_READ_ONLY - The divider settings are preconfigured and should
 *  not be changed by the clock framework.
 * CLK_DIVIDER_MAX_AT_ZERO - For dividers which are like CLK_DIVIDER_ONE_BASED
 *  except when the value read from the register is zero, the divisor is
 *  2^width of the field.
 */
struct clock_divider {
    struct clock_hw               hw;
    void                         *reg;
    uint8_t                       shift;
    uint8_t                       width;
    uint8_t                       flags;
    const struct clock_div_table *table;
    vmm_spinlock_t               *lock;
};

#define to_clock_divider(_hw)     container_of(_hw, struct clock_divider, hw)

#define CLK_DIVIDER_ONE_BASED     BIT(0)
#define CLK_DIVIDER_POWER_OF_TWO  BIT(1)
#define CLK_DIVIDER_ALLOW_ZERO    BIT(2)
#define CLK_DIVIDER_HIWORD_MASK   BIT(3)
#define CLK_DIVIDER_ROUND_CLOSEST BIT(4)
#define CLK_DIVIDER_READ_ONLY     BIT(5)
#define CLK_DIVIDER_MAX_AT_ZERO   BIT(6)

extern const struct clock_ops clock_divider_ops;
extern const struct clock_ops clock_divider_ro_ops;

uint64_t divider_recalc_rate(struct clock_hw *hw, uint64_t parent_rate, uint32_t val, const struct clock_div_table *table, uint64_t flags);
long     divider_round_rate_parent(
        struct clock_hw *hw, struct clock_hw *parent, uint64_t rate, uint64_t *prate, const struct clock_div_table *table, uint8_t width, uint64_t flags);
int divider_get_val(uint64_t rate, uint64_t parent_rate, const struct clock_div_table *table, uint8_t width, uint64_t flags);

struct clk *clock_register_divider(
    vmm_device_t *dev, const char *name, const char *parent_name, uint64_t flags, void *reg, uint8_t shift, uint8_t width,
    uint8_t clock_divider_flags, vmm_spinlock_t *lock);
struct clock_hw *clock_hw_register_divider(
    vmm_device_t *dev, const char *name, const char *parent_name, uint64_t flags, void *reg, uint8_t shift, uint8_t width,
    uint8_t clock_divider_flags, vmm_spinlock_t *lock);
struct clk *clock_register_divider_table(
    vmm_device_t *dev, const char *name, const char *parent_name, uint64_t flags, void *reg, uint8_t shift, uint8_t width,
    uint8_t clock_divider_flags, const struct clock_div_table *table, vmm_spinlock_t *lock);
struct clock_hw *clock_hw_register_divider_table(
    vmm_device_t *dev, const char *name, const char *parent_name, uint64_t flags, void *reg, uint8_t shift, uint8_t width,
    uint8_t clock_divider_flags, const struct clock_div_table *table, vmm_spinlock_t *lock);
void clock_unregister_divider(struct clk *clk);
void clock_hw_unregister_divider(struct clock_hw *hw);

/**
 * struct clock_mux - multiplexer clock
 *
 * @hw:     handle between common and hardware-specific interfaces
 * @reg:    register controlling multiplexer
 * @shift:  shift to multiplexer bit field
 * @width:  width of mutliplexer bit field
 * @flags:  hardware-specific flags
 * @lock:   register lock
 *
 * Clock with multiple selectable parents.  Implements .get_parent, .set_parent
 * and .recalc_rate
 *
 * Flags:
 * CLK_MUX_INDEX_ONE - register index starts at 1, not 0
 * CLK_MUX_INDEX_BIT - register index is a single bit (power of two)
 * CLK_MUX_HIWORD_MASK - The mux settings are only in lower 16-bit of this
 *  register, and mask of mux bits are in higher 16-bit of this register.
 *  While setting the mux bits, higher 16-bit should also be updated to
 *  indicate changing mux bits.
 * CLK_MUX_ROUND_CLOSEST - Use the parent rate that is closest to the desired
 *  frequency.
 */
struct clock_mux {
    struct clock_hw hw;
    void           *reg;
    uint32_t       *table;
    uint32_t        mask;
    uint8_t         shift;
    uint8_t         flags;
    vmm_spinlock_t *lock;
};

#define to_clock_mux(_hw)     container_of(_hw, struct clock_mux, hw)

#define CLK_MUX_INDEX_ONE     BIT(0)
#define CLK_MUX_INDEX_BIT     BIT(1)
#define CLK_MUX_HIWORD_MASK   BIT(2)
#define CLK_MUX_READ_ONLY     BIT(3) /* mux can't be changed */
#define CLK_MUX_ROUND_CLOSEST BIT(4)

extern const struct clock_ops clock_mux_ops;
extern const struct clock_ops clock_mux_ro_ops;

struct clk *clock_register_mux(
    vmm_device_t *dev, const char *name, const char *const *parent_names, uint8_t num_parents, uint64_t flags, void *reg, uint8_t shift,
    uint8_t width, uint8_t clock_mux_flags, vmm_spinlock_t *lock);
struct clock_hw *clock_hw_register_mux(
    vmm_device_t *dev, const char *name, const char *const *parent_names, uint8_t num_parents, uint64_t flags, void *reg, uint8_t shift,
    uint8_t width, uint8_t clock_mux_flags, vmm_spinlock_t *lock);

struct clk *clock_register_mux_table(
    vmm_device_t *dev, const char *name, const char *const *parent_names, uint8_t num_parents, uint64_t flags, void *reg, uint8_t shift,
    uint32_t mask, uint8_t clock_mux_flags, uint32_t *table, vmm_spinlock_t *lock);
struct clock_hw *clock_hw_register_mux_table(
    vmm_device_t *dev, const char *name, const char *const *parent_names, uint8_t num_parents, uint64_t flags, void *reg, uint8_t shift,
    uint32_t mask, uint8_t clock_mux_flags, uint32_t *table, vmm_spinlock_t *lock);

void clock_unregister_mux(struct clk *clk);
void clock_hw_unregister_mux(struct clock_hw *hw);

void of_fixed_factor_clock_setup(vmm_device_tree_node_t *node);

/**
 * struct clock_fixed_factor - fixed multiplier and divider clock
 *
 * @hw:     handle between common and hardware-specific interfaces
 * @mult:   multiplier
 * @div:    divider
 *
 * Clock with a fixed multiplier and divider. The output frequency is the
 * parent clock rate divided by div and multiplied by mult.
 * Implements .recalc_rate, .set_rate and .round_rate
 */

struct clock_fixed_factor {
    struct clock_hw hw;
    uint32_t        mult;
    uint32_t        div;
};

#define to_clock_fixed_factor(_hw) container_of(_hw, struct clock_fixed_factor, hw)

extern const struct clock_ops clock_fixed_factor_ops;
struct clk *clock_register_fixed_factor(vmm_device_t *dev, const char *name, const char *parent_name, uint64_t flags, uint32_t mult, uint32_t div);
void        clock_unregister_fixed_factor(struct clk *clk);
struct clock_hw *clock_hw_register_fixed_factor(
    vmm_device_t *dev, const char *name, const char *parent_name, uint64_t flags, uint32_t mult, uint32_t div);
void clock_hw_unregister_fixed_factor(struct clock_hw *hw);

/**
 * struct clock_fractional_divider - adjustable fractional divider clock
 *
 * @hw:     handle between common and hardware-specific interfaces
 * @reg:    register containing the divider
 * @mshift: shift to the numerator bit field
 * @mwidth: width of the numerator bit field
 * @nshift: shift to the denominator bit field
 * @nwidth: width of the denominator bit field
 * @lock:   register lock
 *
 * Clock with adjustable fractional divider affecting its output frequency.
 */
struct clock_fractional_divider {
    struct clock_hw hw;
    void           *reg;
    uint8_t         mshift;
    uint8_t         mwidth;
    uint32_t        mmask;
    uint8_t         nshift;
    uint8_t         nwidth;
    uint32_t        nmask;
    uint8_t         flags;
    long            max_prate;
    void (*approximation)(struct clock_hw *hw, uint64_t rate, uint64_t *parent_rate, uint64_t *m, uint64_t *n);
    vmm_spinlock_t *lock;
};

#define to_clock_fd(_hw) container_of(_hw, struct clock_fractional_divider, hw)

extern const struct clock_ops clock_fractional_divider_ops;
struct clk                   *clock_register_fractional_divider(
                      vmm_device_t *dev, const char *name, const char *parent_name, uint64_t flags, void *reg, uint8_t mshift, uint8_t mwidth, uint8_t nshift,
                      uint8_t nwidth, uint8_t clock_divider_flags, vmm_spinlock_t *lock);
struct clock_hw *clock_hw_register_fractional_divider(
    vmm_device_t *dev, const char *name, const char *parent_name, uint64_t flags, void *reg, uint8_t mshift, uint8_t mwidth, uint8_t nshift,
    uint8_t nwidth, uint8_t clock_divider_flags, vmm_spinlock_t *lock);
void clock_hw_unregister_fractional_divider(struct clock_hw *hw);

/**
 * struct clock_multiplier - adjustable multiplier clock
 *
 * @hw:     handle between common and hardware-specific interfaces
 * @reg:    register containing the multiplier
 * @shift:  shift to the multiplier bit field
 * @width:  width of the multiplier bit field
 * @lock:   register lock
 *
 * Clock with an adjustable multiplier affecting its output frequency.
 * Implements .recalc_rate, .set_rate and .round_rate
 *
 * Flags:
 * CLK_MULTIPLIER_ZERO_BYPASS - By default, the multiplier is the value read
 *  from the register, with 0 being a valid value effectively
 *  zeroing the output clock rate. If CLK_MULTIPLIER_ZERO_BYPASS is
 *  set, then a null multiplier will be considered as a bypass,
 *  leaving the parent rate unmodified.
 * CLK_MULTIPLIER_ROUND_CLOSEST - Makes the best calculated divider to be
 *  rounded to the closest integer instead of the down one.
 */
struct clock_multiplier {
    struct clock_hw hw;
    void           *reg;
    uint8_t         shift;
    uint8_t         width;
    uint8_t         flags;
    vmm_spinlock_t *lock;
};

#define to_clock_multiplier(_hw)     container_of(_hw, struct clock_multiplier, hw)

#define CLK_MULTIPLIER_ZERO_BYPASS   BIT(0)
#define CLK_MULTIPLIER_ROUND_CLOSEST BIT(1)

extern const struct clock_ops clock_multiplier_ops;

/***
 * struct clock_composite - aggregate clock of mux, divider and gate clocks
 *
 * @hw:     handle between common and hardware-specific interfaces
 * @mux_hw: handle between composite and hardware-specific mux clock
 * @rate_hw:    handle between composite and hardware-specific rate clock
 * @gate_hw:    handle between composite and hardware-specific gate clock
 * @mux_ops:    clock ops for mux
 * @rate_ops:   clock ops for rate
 * @gate_ops:   clock ops for gate
 */
struct clock_composite {
    struct clock_hw  hw;
    struct clock_ops ops;

    struct clock_hw *mux_hw;
    struct clock_hw *rate_hw;
    struct clock_hw *gate_hw;

    const struct clock_ops *mux_ops;
    const struct clock_ops *rate_ops;
    const struct clock_ops *gate_ops;
};

#define to_clock_composite(_hw) container_of(_hw, struct clock_composite, hw)

struct clk *clock_register_composite(
    vmm_device_t *dev, const char *name, const char *const *parent_names, int num_parents, struct clock_hw *mux_hw, const struct clock_ops *mux_ops,
    struct clock_hw *rate_hw, const struct clock_ops *rate_ops, struct clock_hw *gate_hw, const struct clock_ops *gate_ops, uint64_t flags);
void             clock_unregister_composite(struct clk *clk);
struct clock_hw *clock_hw_register_composite(
    vmm_device_t *dev, const char *name, const char *const *parent_names, int num_parents, struct clock_hw *mux_hw, const struct clock_ops *mux_ops,
    struct clock_hw *rate_hw, const struct clock_ops *rate_ops, struct clock_hw *gate_hw, const struct clock_ops *gate_ops, uint64_t flags);
void clock_hw_unregister_composite(struct clock_hw *hw);

/**
 * clock_register - allocate a new clock, register it and return an opaque cookie
 * @dev: device that is registering this clock
 * @hw: link to hardware-specific clock data
 *
 * clock_register is the primary interface for populating the clock tree with new
 * clock nodes.  It returns a pointer to the newly allocated struct clk which
 * cannot be dereferenced by driver code but may be used in conjuction with the
 * rest of the clock API.  In the event of an error clock_register will return an
 * error code; drivers must test for an error code after calling clock_register.
 */
struct clk *clock_register(vmm_device_t *dev, struct clock_hw *hw);
struct clk *devm_clock_register(vmm_device_t *dev, struct clock_hw *hw);

int clock_hw_register(vmm_device_t *dev, struct clock_hw *hw);
int devm_clock_hw_register(vmm_device_t *dev, struct clock_hw *hw);

void clock_unregister(struct clk *clk);
void devm_clock_unregister(vmm_device_t *dev, struct clk *clk);

void clock_hw_unregister(struct clock_hw *hw);
void devm_clock_hw_unregister(vmm_device_t *dev, struct clock_hw *hw);

/* helper functions */
const char      *__clock_get_name(const struct clk *clk);
const char      *clock_hw_get_name(const struct clock_hw *hw);
struct clock_hw *__clock_get_hw(struct clk *clk);
uint32_t         clock_hw_get_num_parents(const struct clock_hw *hw);
struct clock_hw *clock_hw_get_parent(const struct clock_hw *hw);
struct clock_hw *clock_hw_get_parent_by_index(const struct clock_hw *hw, uint32_t index);
uint32_t         __clock_get_enable_count(struct clk *clk);
uint64_t         clock_hw_get_rate(const struct clock_hw *hw);
uint64_t         __clock_get_flags(struct clk *clk);
uint64_t         clock_hw_get_flags(const struct clock_hw *hw);
bool             clock_hw_is_prepared(const struct clock_hw *hw);
bool             clock_hw_is_enabled(const struct clock_hw *hw);
bool             __clock_is_enabled(struct clk *clk);
struct clk      *__clock_lookup(const char *name);
int              __clock_mux_determine_rate(struct clock_hw *hw, struct clock_rate_request *req);
int              __clock_determine_rate(struct clock_hw *core, struct clock_rate_request *req);
int              __clock_mux_determine_rate_closest(struct clock_hw *hw, struct clock_rate_request *req);
void             clock_hw_reparent(struct clock_hw *hw, struct clock_hw *new_parent);
void             clock_hw_set_rate_range(struct clock_hw *hw, uint64_t min_rate, uint64_t max_rate);

static inline void __clock_hw_set_clock(struct clock_hw *dst, struct clock_hw *src)
{
    dst->clk  = src->clk;
    dst->core = src->core;
}

static inline long divider_round_rate(
    struct clock_hw *hw, uint64_t rate, uint64_t *prate, const struct clock_div_table *table, uint8_t width, uint64_t flags)
{
    return divider_round_rate_parent(hw, clock_hw_get_parent(hw), rate, prate, table, width, flags);
}

/*
 * FIXME clock api without lock protection
 */
uint64_t clock_hw_round_rate(struct clock_hw *hw, uint64_t rate);

typedef void (*of_clock_init_cb_t)(vmm_device_tree_node_t *);

struct clock_onecell_data {
    struct clk **clks;
    uint32_t     clock_num;
};

struct clock_hw_onecell_data {
    uint32_t         num;
    struct clock_hw *hws[];
};

#define CLK_OF_DECLARE(name, compat, fn) VMM_DEVICE_TREE_NIDTBL_ENTRY(name, "clk-provider", "", "", compat, fn)

#ifdef CONFIG_OF
int of_clock_add_provider(
    vmm_device_tree_node_t *np, struct clk *(*clock_src_get)(struct vmm_device_tree_phandle_args *args, void *data), void *data);
int of_clock_add_hw_provider(
    vmm_device_tree_node_t *np, struct clock_hw *(*get)(struct vmm_device_tree_phandle_args *clkspec, void *data), void *data);
int  devm_of_clock_add_hw_provider(vmm_device_t *dev, struct clock_hw *(*get)(struct vmm_device_tree_phandle_args *clkspec, void *data), void *data);
void of_clock_del_provider(vmm_device_tree_node_t *np);
void devm_of_clock_del_provider(vmm_device_t *dev);
struct clk      *of_clock_src_simple_get(struct vmm_device_tree_phandle_args *clkspec, void *data);
struct clock_hw *of_clock_hw_simple_get(struct vmm_device_tree_phandle_args *clkspec, void *data);
struct clk      *of_clock_src_onecell_get(struct vmm_device_tree_phandle_args *clkspec, void *data);
struct clock_hw *of_clock_hw_onecell_get(struct vmm_device_tree_phandle_args *clkspec, void *data);
uint32_t         of_clock_get_parent_count(vmm_device_tree_node_t *np);
int              of_clock_parent_fill(vmm_device_tree_node_t *np, const char **parents, uint32_t size);
const char      *of_clock_get_parent_name(vmm_device_tree_node_t *np, int index);
int              of_clock_detect_critical(vmm_device_tree_node_t *np, int index, uint64_t *flags);
void             of_clock_init(const struct vmm_device_tree_nodeid *matches);

#else  /* !CONFIG_OF */

static inline int of_clock_add_provider(
    vmm_device_tree_node_t *np, struct clk *(*clock_src_get)(struct vmm_device_tree_phandle_args *args, void *data), void *data)
{
    return 0;
}

static inline int of_clock_add_hw_provider(
    vmm_device_tree_node_t *np, struct clock_hw *(*get)(struct vmm_device_tree_phandle_args *clkspec, void *data), void *data)
{
    return 0;
}

static inline int devm_of_clock_add_hw_provider(
    vmm_device_t *dev, struct clock_hw *(*get)(struct vmm_device_tree_phandle_args *clkspec, void *data), void *data)
{
    return 0;
}

static inline void of_clock_del_provider(vmm_device_tree_node_t *np) {}

static inline void devm_of_clock_del_provider(vmm_device_t *dev) {}

static inline struct clk *of_clock_src_simple_get(struct vmm_device_tree_phandle_args *clkspec, void *data)
{
    return VMM_ERR_RR_PTR(VMM_ERR_NOENT);
}

static inline struct clock_hw *of_clock_hw_simple_get(struct vmm_device_tree_phandle_args *clkspec, void *data)
{
    return VMM_ERR_RR_PTR(VMM_ERR_NOENT);
}

static inline struct clk *of_clock_src_onecell_get(struct vmm_device_tree_phandle_args *clkspec, void *data)
{
    return VMM_ERR_RR_PTR(VMM_ERR_NOENT);
}

static inline struct clock_hw *of_clock_hw_onecell_get(struct vmm_device_tree_phandle_args *clkspec, void *data)
{
    return VMM_ERR_RR_PTR(VMM_ERR_NOENT);
}

static inline uint32_t of_clock_get_parent_count(vmm_device_tree_node_t *np)
{
    return 0;
}

static inline int of_clock_parent_fill(vmm_device_tree_node_t *np, const char **parents, uint32_t size)
{
    return 0;
}

static inline const char *of_clock_get_parent_name(vmm_device_tree_node_t *np, int index)
{
    return NULL;
}

static inline int of_clock_detect_critical(vmm_device_tree_node_t *np, int index, uint64_t *flags)
{
    return 0;
}

static inline void of_clock_init(const struct vmm_device_tree_nodeid *matches) {}
#endif /* CONFIG_OF */

/*
 * wrap access to peripherals in accessor routines
 * for improved portability across platforms
 */

#ifdef CONFIG_PPC

static inline uint32_t clock_readl(uint32_t *reg)
{
    return vmm_ioread32be(reg);
}

static inline void clock_writel(uint32_t val, uint32_t *reg)
{
    vmm_iowrite32be(val, reg);
}

#else  /* platform dependent I/O accessors */

static inline uint32_t clock_readl(uint32_t *reg)
{
    return vmm_readl(reg);
}

static inline void clock_writel(uint32_t val, uint32_t *reg)
{
    vmm_writel(val, reg);
}

#endif /* platform dependent I/O accessors */

#endif /* CONFIG_COMMON_CLK */

#endif /* __CLK_PROVIDER_H__ */
