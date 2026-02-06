/*
 * Copyright (c) 2014 MundoReader S.L.
 * Author: Heiko Stuebner <heiko@sntech.de>
 *
 * Copyright (c) 2016 Rockchip Electronics Co. Ltd.
 * Author: Xing Zheng <zhengxing@rock-chips.com>
 *
 * based on
 *
 * samsung/clk.c
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Copyright (c) 2013 Linaro Ltd.
 * Author: Thomas Abraham <thomas.ab@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "clk.h"
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/mfd/syscon.h>
#include <linux/rational.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/slab.h>

/**
 * Register a clock branch.
 * Most clock branches have a form like
 *
 * src1 --|--\
 *        |M |--[GATE]-[DIV]-
 * src2 --|--/
 *
 * sometimes without one of those components.
 */
static struct clk *rockchip_clock_register_branch(
    const char *name, const char *const *parent_names, uint8_t num_parents, void __iomem *base, int muxdiv_offset, uint8_t mux_shift,
    uint8_t mux_width, uint8_t mux_flags, int div_offset, uint8_t div_shift, uint8_t div_width, uint8_t div_flags, struct clock_div_table *div_table,
    int gate_offset, uint8_t gate_shift, uint8_t gate_flags, uint64_t flags, spinlock_t *lock)
{
    struct clk             *clk;
    struct clock_mux       *mux     = NULL;
    struct clock_gate      *gate    = NULL;
    struct clock_divider   *div     = NULL;
    const struct clock_ops *mux_ops = NULL, *div_ops = NULL, *gate_ops = NULL;

    if (num_parents > 1) {
        mux = kzalloc(sizeof(*mux), GFP_KERNEL);

        if (!mux) {
            return ERR_PTR(-ENOMEM);
        }

        mux->reg   = base + muxdiv_offset;
        mux->shift = mux_shift;
        mux->mask  = BIT(mux_width) - 1;
        mux->flags = mux_flags;
        mux->lock  = lock;
        mux_ops    = (mux_flags & CLK_MUX_READ_ONLY) ? &clock_mux_ro_ops : &clock_mux_ops;
    }

    if (gate_offset >= 0) {
        gate = kzalloc(sizeof(*gate), GFP_KERNEL);

        if (!gate) {
            goto err_gate;
        }

        gate->flags   = gate_flags;
        gate->reg     = base + gate_offset;
        gate->bit_idx = gate_shift;
        gate->lock    = lock;
        gate_ops      = &clock_gate_ops;
    }

    if (div_width > 0) {
        div = kzalloc(sizeof(*div), GFP_KERNEL);

        if (!div) {
            goto err_div;
        }

        div->flags = div_flags;

        if (div_offset) {
            div->reg = base + div_offset;
        } else {
            div->reg = base + muxdiv_offset;
        }

        div->shift = div_shift;
        div->width = div_width;
        div->lock  = lock;
        div->table = div_table;
        div_ops    = (div_flags & CLK_DIVIDER_READ_ONLY) ? &clock_divider_ro_ops : &clock_divider_ops;
    }

    clk = clock_register_composite(
        NULL, name, parent_names, num_parents, mux ? &mux->hw : NULL, mux_ops, div ? &div->hw : NULL, div_ops, gate ? &gate->hw : NULL, gate_ops,
        flags);

    return clk;
err_div:
    kfree(gate);
err_gate:
    kfree(mux);
    return ERR_PTR(-ENOMEM);
}

struct rockchip_clock_frac {
    struct notifier_block           clock_nb;
    struct clock_fractional_divider div;
    struct clock_gate               gate;

    struct clock_mux        mux;
    const struct clock_ops *mux_ops;
    int                     mux_frac_idx;

    bool rate_change_remuxed;
    int  rate_change_idx;
};

#define to_rockchip_clock_frac_nb(nb) container_of(nb, struct rockchip_clock_frac, clock_nb)

static int rockchip_clock_frac_notifier_cb(struct notifier_block *nb, uint64_t event, void *data)
{
#if 0
    struct clock_notifier_data *ndata = data;
#endif
    struct rockchip_clock_frac *frac     = to_rockchip_clock_frac_nb(nb);
    struct clock_mux           *frac_mux = &frac->mux;
    int                         ret      = 0;

    pr_debug("%s: event %lu, old_rate %lu, new_rate: %lu\n", __func__, event, ndata->old_rate, ndata->new_rate);

    if (event == PRE_RATE_CHANGE) {
        frac->rate_change_idx = frac->mux_ops->get_parent(&frac_mux->hw);

        if (frac->rate_change_idx != frac->mux_frac_idx) {
            frac->mux_ops->set_parent(&frac_mux->hw, frac->mux_frac_idx);
            frac->rate_change_remuxed = 1;
        }
    } else if (event == POST_RATE_CHANGE) {
        /*
         * The POST_RATE_CHANGE notifier runs directly after the
         * divider clock is set in clock_change_rate, so we'll have
         * remuxed back to the original parent before clock_change_rate
         * reaches the mux itself.
         */
        if (frac->rate_change_remuxed) {
            frac->mux_ops->set_parent(&frac_mux->hw, frac->rate_change_idx);
            frac->rate_change_remuxed = 0;
        }
    }

    return notifier_from_errno(ret);
}

/**
 * fractional divider must set that denominator is 20 times larger than
 * numerator to generate precise clock frequency.
 */
static void rockchip_fractional_approximation(struct clock_hw *hw, uint64_t rate, uint64_t *parent_rate, uint64_t *m, uint64_t *n)
{
    struct clock_fractional_divider *fd = to_clock_fd(hw);
    uint64_t                         p_rate, p_parent_rate;
    struct clock_hw                 *p_parent;
    uint64_t                         scale;
    uint32_t                         div;

    p_rate = clock_hw_get_rate(clock_hw_get_parent(hw));

    if (strstr(clock_hw_get_name(hw), "uart")) {
        if (rate <= 24000000) {
            *parent_rate = 24000000;
        } else {
            if (fd->max_prate) {
                *parent_rate = fd->max_prate;
            } else {
                *parent_rate = 480000000;
            }
        }

        goto frac_ration;
    }

    if (((rate * 20 > p_rate) && (p_rate % rate != 0)) || (fd->max_prate && fd->max_prate < p_rate)) {
        p_parent = clock_hw_get_parent(clock_hw_get_parent(hw));

        if (!p_parent) {
            *parent_rate = p_rate;
        } else {
            p_parent_rate = clock_hw_get_rate(p_parent);
            *parent_rate  = p_parent_rate;

            if (fd->max_prate && p_parent_rate > fd->max_prate) {
                div          = DIV_ROUND_UP(p_parent_rate, fd->max_prate);
                *parent_rate = p_parent_rate / div;
            }
        }

        if (*parent_rate < rate * 20) {
            pr_warn("%s p_rate(%ld) is low than rate(%ld)*20, use integer or half-div\n", clock_hw_get_name(hw), *parent_rate, rate);
            *m = 0;
            *n = 1;
            return;
        }
    }

frac_ration:
    /*
     * Get rate closer to *parent_rate to guarantee there is no overflow
     * for m and n. In the result it will be the nearest rate left shifted
     * by (scale - fd->nwidth) bits.
     */
    scale = fls_long(*parent_rate / rate - 1);

    if (scale > fd->nwidth) {
        rate <<= scale - fd->nwidth;
    }

    rational_best_approximation(rate, *parent_rate, GENMASK(fd->mwidth - 1, 0), GENMASK(fd->nwidth - 1, 0), m, n);
}

static struct clk *rockchip_clock_register_frac_branch(
    struct rockchip_clock_provider *ctx, const char *name, const char *const *parent_names, uint8_t num_parents, void __iomem *base,
    int muxdiv_offset, uint8_t div_flags, int gate_offset, uint8_t gate_shift, uint8_t gate_flags, uint64_t flags,
    struct rockchip_clock_branch *child, uint64_t max_prate, spinlock_t *lock)
{
    struct rockchip_clock_frac      *frac;
    struct clk                      *clk;
    struct clock_gate               *gate    = NULL;
    struct clock_fractional_divider *div     = NULL;
    const struct clock_ops          *div_ops = NULL, *gate_ops = NULL;

    if (muxdiv_offset < 0) {
        return ERR_PTR(-EINVAL);
    }

    if (child && child->branch_type != branch_mux) {
        pr_err("%s: fractional child clock for %s can only be a mux\n", __func__, name);
        return ERR_PTR(-EINVAL);
    }

    frac = kzalloc(sizeof(*frac), GFP_KERNEL);

    if (!frac) {
        return ERR_PTR(-ENOMEM);
    }

    if (gate_offset >= 0) {
        gate          = &frac->gate;
        gate->flags   = gate_flags;
        gate->reg     = base + gate_offset;
        gate->bit_idx = gate_shift;
        gate->lock    = lock;
        gate_ops      = &clock_gate_ops;
    }

    div                = &frac->div;
    div->flags         = div_flags;
    div->reg           = base + muxdiv_offset;
    div->mshift        = 16;
    div->mwidth        = 16;
    div->mmask         = GENMASK(div->mwidth - 1, 0) << div->mshift;
    div->nshift        = 0;
    div->nwidth        = 16;
    div->nmask         = GENMASK(div->nwidth - 1, 0) << div->nshift;
    div->lock          = lock;
    div->approximation = rockchip_fractional_approximation;
    div->max_prate     = max_prate;
    div_ops            = &clock_fractional_divider_ops;

    clk                = clock_register_composite(
        NULL, name, parent_names, num_parents, NULL, NULL, &div->hw, div_ops, gate ? &gate->hw : NULL, gate_ops, flags | CLK_SET_RATE_UNGATE);

    if (IS_ERR(clk)) {
        kfree(frac);
        return clk;
    }

    if (child) {
        struct clock_mux      *frac_mux = &frac->mux;
        struct clock_init_data init;
        struct clk            *mux_clock;
        int                    i, ret;

        frac->mux_frac_idx = -1;

        for (i = 0; i < child->num_parents; i++) {
            if (!strcmp(name, child->parent_names[i])) {
                pr_debug("%s: found fractional parent in mux at pos %d\n", __func__, i);
                frac->mux_frac_idx = i;
                break;
            }
        }

        frac->mux_ops                = &clock_mux_ops;
        frac->clock_nb.notifier_call = rockchip_clock_frac_notifier_cb;

        frac_mux->reg                = base + child->muxdiv_offset;
        frac_mux->shift              = child->mux_shift;
        frac_mux->mask               = BIT(child->mux_width) - 1;
        frac_mux->flags              = child->mux_flags;
        frac_mux->lock               = lock;
        frac_mux->hw.init            = &init;

        init.name                    = child->name;
        init.flags                   = child->flags | CLK_SET_RATE_PARENT;
        init.ops                     = frac->mux_ops;
        init.parent_names            = child->parent_names;
        init.num_parents             = child->num_parents;

        mux_clock                    = clock_register(NULL, &frac_mux->hw);

        if (IS_ERR(mux_clock)) {
            return clk;
        }

        rockchip_clock_add_lookup(ctx, mux_clock, child->id);

        /* notifier on the fraction divider to catch rate changes */
        if (frac->mux_frac_idx >= 0) {
            ret = clock_notifier_register(clk, &frac->clock_nb);

            if (ret) {
                pr_err("%s: failed to register clock notifier for %s\n", __func__, name);
            }
        } else {
            pr_warn("%s: could not find %s as parent of %s, rate changes may not work\n", __func__, name, child->name);
        }
    }

    return clk;
}

static struct clk *rockchip_clock_register_factor_branch(
    const char *name, const char *const *parent_names, uint8_t num_parents, void __iomem *base, uint32_t mult, uint32_t div, int gate_offset,
    uint8_t gate_shift, uint8_t gate_flags, uint64_t flags, spinlock_t *lock)
{
    struct clk                *clk;
    struct clock_gate         *gate = NULL;
    struct clock_fixed_factor *fix  = NULL;

    /* without gate, register a simple factor clock */
    if (gate_offset == 0) {
        return clock_register_fixed_factor(NULL, name, parent_names[0], flags, mult, div);
    }

    gate = kzalloc(sizeof(*gate), GFP_KERNEL);

    if (!gate) {
        return ERR_PTR(-ENOMEM);
    }

    gate->flags   = gate_flags;
    gate->reg     = base + gate_offset;
    gate->bit_idx = gate_shift;
    gate->lock    = lock;

    fix           = kzalloc(sizeof(*fix), GFP_KERNEL);

    if (!fix) {
        kfree(gate);
        return ERR_PTR(-ENOMEM);
    }

    fix->mult = mult;
    fix->div  = div;

    clk       = clock_register_composite(
        NULL, name, parent_names, num_parents, NULL, NULL, &fix->hw, &clock_fixed_factor_ops, &gate->hw, &clock_gate_ops, flags);

    if (IS_ERR(clk)) {
        kfree(fix);
        kfree(gate);
    }

    return clk;
}

struct rockchip_clock_provider *__init rockchip_clock_init(struct device_node *np, void __iomem *base, uint64_t nr_clocks)
{
    struct rockchip_clock_provider *ctx;
    struct clk                    **clock_table;
    int                             i;

    ctx = kzalloc(sizeof(struct rockchip_clock_provider), GFP_KERNEL);

    if (!ctx) {
        pr_err("%s: Could not allocate clock provider context\n", __func__);
        return ERR_PTR(-ENOMEM);
    }

    clock_table = kcalloc(nr_clocks, sizeof(struct clk *), GFP_KERNEL);

    if (!clock_table) {
        pr_err("%s: Could not allocate clock lookup table\n", __func__);
        goto err_free;
    }

    for (i = 0; i < nr_clocks; ++i) {
        clock_table[i] = ERR_PTR(-ENOENT);
    }

    ctx->reg_base             = base;
    ctx->clock_data.clks      = clock_table;
    ctx->clock_data.clock_num = nr_clocks;
    ctx->cru_node             = np;
    ctx->grf                  = ERR_PTR(-EPROBE_DEFER);
    spin_lock_init(&ctx->lock);
    ctx->grf = syscon_regmap_lookup_by_phandle(ctx->cru_node, "rockchip,grf");

    return ctx;

err_free:
    kfree(ctx);
    return ERR_PTR(-ENOMEM);
}

void __init rockchip_clock_of_add_provider(struct device_node *np, struct rockchip_clock_provider *ctx)
{
    if (of_clock_add_provider(np, of_clock_src_onecell_get, &ctx->clock_data)) {
        pr_err("%s: could not register clk provider\n", __func__);
    }
}

struct regmap *rockchip_clock_get_grf(struct rockchip_clock_provider *ctx)
{
    if (IS_ERR(ctx->grf)) {
        ctx->grf = syscon_regmap_lookup_by_phandle(ctx->cru_node, "rockchip,grf");
    }

    return ctx->grf;
}

void rockchip_clock_add_lookup(struct rockchip_clock_provider *ctx, struct clk *clk, uint32_t id)
{
    if (ctx->clock_data.clks && id) {
        ctx->clock_data.clks[id] = clk;
    }
}

void __init rockchip_clock_register_plls(struct rockchip_clock_provider *ctx, struct rockchip_pll_clock *list, uint32_t nr_pll, int grf_lock_offset)
{
    struct clk *clk;
    int         idx;

    for (idx = 0; idx < nr_pll; idx++, list++) {
        clk = rockchip_clock_register_pll(
            ctx, list->type, list->name, list->parent_names, list->num_parents, list->con_offset, grf_lock_offset, list->lock_shift,
            list->mode_offset, list->mode_shift, list->rate_table, list->flags, list->pll_flags);

        if (IS_ERR(clk)) {
            pr_err("%s: failed to register clock %s\n", __func__, list->name);
            continue;
        }

        rockchip_clock_add_lookup(ctx, clk, list->id);
    }
}

void __init rockchip_clock_register_branches(struct rockchip_clock_provider *ctx, struct rockchip_clock_branch *list, uint32_t nr_clock)
{
    struct clk *clk = NULL;
    uint32_t    idx;
    uint64_t    flags;

    for (idx = 0; idx < nr_clock; idx++, list++) {
        flags = list->flags;

        /* catch simple muxes */
        switch (list->branch_type) {
            case branch_mux:
                clk = clock_register_mux(
                    NULL, list->name, list->parent_names, list->num_parents, flags, ctx->reg_base + list->muxdiv_offset, list->mux_shift,
                    list->mux_width, list->mux_flags, &ctx->lock);
                break;

            case branch_muxgrf:
                clk = rockchip_clock_register_muxgrf(
                    list->name, list->parent_names, list->num_parents, flags, ctx->grf, list->muxdiv_offset, list->mux_shift, list->mux_width,
                    list->mux_flags);
                break;

            case branch_divider:
                if (list->div_table) {
                    clk = clock_register_divider_table(
                        NULL, list->name, list->parent_names[0], flags, ctx->reg_base + list->muxdiv_offset, list->div_shift, list->div_width,
                        list->div_flags, list->div_table, &ctx->lock);
                } else {
                    clk = clock_register_divider(
                        NULL, list->name, list->parent_names[0], flags, ctx->reg_base + list->muxdiv_offset, list->div_shift, list->div_width,
                        list->div_flags, &ctx->lock);
                }

                break;

            case branch_fraction_divider:
                clk = rockchip_clock_register_frac_branch(
                    ctx, list->name, list->parent_names, list->num_parents, ctx->reg_base, list->muxdiv_offset, list->div_flags, list->gate_offset,
                    list->gate_shift, list->gate_flags, flags, list->child, list->max_prate, &ctx->lock);
                break;

            case branch_half_divider:
                clk = rockchip_clock_register_halfdiv(
                    list->name, list->parent_names, list->num_parents, ctx->reg_base, list->muxdiv_offset, list->mux_shift, list->mux_width,
                    list->mux_flags, list->div_shift, list->div_width, list->div_flags, list->gate_offset, list->gate_shift, list->gate_flags, flags,
                    &ctx->lock);
                break;

            case branch_gate:
                flags |= CLK_SET_RATE_PARENT;

                clk = clock_register_gate(
                    NULL, list->name, list->parent_names[0], flags, ctx->reg_base + list->gate_offset, list->gate_shift, list->gate_flags,
                    &ctx->lock);
                break;

            case branch_composite:
                clk = rockchip_clock_register_branch(
                    list->name, list->parent_names, list->num_parents, ctx->reg_base, list->muxdiv_offset, list->mux_shift, list->mux_width,
                    list->mux_flags, list->div_offset, list->div_shift, list->div_width, list->div_flags, list->div_table, list->gate_offset,
                    list->gate_shift, list->gate_flags, flags, &ctx->lock);
                break;

            case branch_mmc:
                clk = rockchip_clock_register_mmc(
                    list->name, list->parent_names, list->num_parents, ctx->reg_base + list->muxdiv_offset, list->div_shift);
                break;

            case branch_inverter:
                clk = rockchip_clock_register_inverter(
                    list->name, list->parent_names, list->num_parents, ctx->reg_base + list->muxdiv_offset, list->div_shift, list->div_flags,
                    &ctx->lock);
                break;

            case branch_factor:
                clk = rockchip_clock_register_factor_branch(
                    list->name, list->parent_names, list->num_parents, ctx->reg_base, list->div_shift, list->div_width, list->gate_offset,
                    list->gate_shift, list->gate_flags, flags, &ctx->lock);
                break;

            case branch_ddrc:
#if 0
            clk = rockchip_clock_register_ddrclk(
                      list->name, list->flags,
                      list->parent_names, list->num_parents,
                      list->muxdiv_offset, list->mux_shift,
                      list->mux_width, list->div_shift,
                      list->div_width, list->div_flags,
                      ctx->reg_base);
#endif
                break;
        }

        /* none of the cases above matched */
        if (!clk) {
            pr_err("%s: unknown clock type %d\n", __func__, list->branch_type);
            continue;
        }

        if (IS_ERR(clk)) {
            pr_err("%s: failed to register clock %s: %ld\n", __func__, list->name, PTR_ERR(clk));
            continue;
        }

        rockchip_clock_add_lookup(ctx, clk, list->id);
    }
}

void __init rockchip_clock_register_armclk(
    struct rockchip_clock_provider *ctx, uint32_t lookup_id, const char *name, const char *const *parent_names, uint8_t num_parents,
    const struct rockchip_cpuclock_reg_data *reg_data, const struct rockchip_cpuclock_rate_table *rates, int nrates)
{
    struct clk *clk;

    clk = rockchip_clock_register_cpuclk(name, parent_names, num_parents, reg_data, rates, nrates, ctx->reg_base, &ctx->lock);

    if (IS_ERR(clk)) {
        pr_err("%s: failed to register clock %s: %ld\n", __func__, name, PTR_ERR(clk));
        return;
    }

    rockchip_clock_add_lookup(ctx, clk, lookup_id);
}

void __init rockchip_clock_protect_critical(const char *const clocks[], int nclocks)
{
    int i;

    /* Protect the clocks that needs to stay on */
    for (i = 0; i < nclocks; i++) {
        struct clk *clk = __clock_lookup(clocks[i]);

        if (clk) {
            clock_prepare_enable(clk);
        }
    }
}

#if 0
static void __iomem *rst_base;
static uint32_t reg_restart;
static void (*cb_restart)(void);
static int rockchip_restart_notify(struct notifier_block *this,
                                   uint64_t mode, void *cmd)
{
    if (cb_restart)
        cb_restart();

    writel(0xfdb9, rst_base + reg_restart);
    return NOTIFY_DONE;
}

static struct notifier_block rockchip_restart_handler = {
    .notifier_call = rockchip_restart_notify,
    .priority = 128,
};
#endif

void __init rockchip_register_restart_notifier(struct rockchip_clock_provider *ctx, uint32_t reg, void (*cb)(void))
{
#if 0
    int ret;

    rst_base = ctx->reg_base;
    reg_restart = reg;
    cb_restart = cb;
    ret = register_restart_handler(&rockchip_restart_handler);

    if (ret)
        pr_err("%s: cannot register restart handler, %d\n",
               __func__, ret);

#endif
}
