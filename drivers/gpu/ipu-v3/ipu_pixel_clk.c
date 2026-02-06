/*
 * Copyright 2008-2014 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @file ipu_pixel_clock.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief IPU pixel clock implementation
 *
 * @ingroup IPU
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <video/ipu-v3.h>

#include "ipu_prv.h"
#include "ipu_regs.h"

/*
 * muxd clock implementation
 */
struct clock_di_mux {
    struct clock_hw hw;
    uint8_t         ipu_id;
    uint8_t         di_id;
    uint8_t         flags;
    uint8_t         index;
};

#define to_clock_di_mux(_hw) container_of(_hw, struct clock_di_mux, hw)

static int _ipu_pixel_clock_set_parent(struct clock_hw *hw, uint8_t index)
{
    struct clock_di_mux *mux = to_clock_di_mux(hw);
    struct ipu_soc      *ipu = ipu_get_soc(mux->ipu_id);
    uint32_t             di_gen;

    di_gen = ipu_di_read(ipu, mux->di_id, DI_GENERAL);

    if (index == 0) {
        /* ipu1_clock or ipu2_clock internal clk */
        di_gen &= ~DI_GEN_DI_CLK_EXT;
    } else {
        di_gen |= DI_GEN_DI_CLK_EXT;
    }

    ipu_di_write(ipu, mux->di_id, di_gen, DI_GENERAL);
    mux->index = index;
    pr_debug("ipu_pixel_clock: di_clock_ext:0x%x, di_gen reg:0x%x.\n", !(di_gen & DI_GEN_DI_CLK_EXT), di_gen);
    return 0;
}

static uint8_t _ipu_pixel_clock_get_parent(struct clock_hw *hw)
{
    struct clock_di_mux *mux = to_clock_di_mux(hw);

    return mux->index;
}

const struct clock_ops clock_mux_di_ops = {
    .get_parent = _ipu_pixel_clock_get_parent,
    .set_parent = _ipu_pixel_clock_set_parent,
};

struct clk *clock_register_mux_pix_clock(
    struct device *dev, const char *name, const char **parent_names, uint8_t num_parents, uint64_t flags, uint8_t ipu_id, uint8_t di_id,
    uint8_t clock_mux_flags)
{
    struct clock_di_mux   *mux;
    struct clk            *clk;
    struct clock_init_data init;

    mux = kzalloc(sizeof(struct clock_di_mux), GFP_KERNEL);

    if (!mux) {
        return ERR_PTR(-ENOMEM);
    }

    init.name         = name;
    init.ops          = &clock_mux_di_ops;
    init.flags        = flags;
    init.parent_names = parent_names;
    init.num_parents  = num_parents;

    mux->ipu_id       = ipu_id;
    mux->di_id        = di_id;
    mux->flags        = clock_mux_flags | CLK_SET_RATE_PARENT;
    mux->hw.init      = &init;

    clk               = clock_register(dev, &mux->hw);

    if (IS_ERR(clk)) {
        kfree(mux);
    }

    return clk;
}

/*
 * Gated clock implementation
 */
struct clock_di_div {
    struct clock_hw hw;
    uint8_t         ipu_id;
    uint8_t         di_id;
    uint8_t         flags;
};

#define to_clock_di_div(_hw) container_of(_hw, struct clock_di_div, hw)

static uint64_t _ipu_pixel_clock_div_recalc_rate(struct clock_hw *hw, uint64_t parent_rate)
{
    struct clock_di_div *di_div = to_clock_di_div(hw);
    struct ipu_soc      *ipu    = ipu_get_soc(di_div->ipu_id);
    uint32_t             div;
    uint64_t             final_rate = (uint64_t)parent_rate * 16;

    _ipu_get(ipu);
    div = ipu_di_read(ipu, di_div->di_id, DI_BS_CLKGEN0);
    _ipu_put(ipu);
    pr_debug("ipu_di%d read BS_CLKGEN0 div:%d, final_rate:%lld, prate:%ld\n", di_div->di_id, div, final_rate, parent_rate);

    if (div == 0) {
        return 0;
    }

    final_rate = udiv64(final_rate, div);

    return (uint64_t)final_rate;
}

static long _ipu_pixel_clock_div_round_rate(struct clock_hw *hw, uint64_t rate, uint64_t *parent_clock_rate)
{
    uint64_t div, final_rate;
    uint64_t remainder;
    uint64_t parent_rate = (uint64_t)(*parent_clock_rate) * 16;

    /*
     * Calculate divider
     * Fractional part is 4 bits,
     * so simply multiply by 2^4 to get fractional part.
     */
    div                  = parent_rate;
    div                  = do_udiv64(div, rate, &remainder);

    /* Round the divider value */
    if (remainder > (rate / 2)) {
        div++;
    }

    if (div < 0x10) { /* Min DI disp clock divider is 1 */
        div = 0x10;
    }

    if (div & ~0xFEF) {
        div &= 0xFF8;
    } else {
        /* Round up divider if it gets us closer to desired pix clk */
        if ((div & 0xC) == 0xC) {
            div += 0x10;
            div &= ~0xF;
        }
    }

    final_rate = parent_rate;
    final_rate = udiv64(final_rate, div);

    return final_rate;
}

static int _ipu_pixel_clock_div_set_rate(struct clock_hw *hw, uint64_t rate, uint64_t parent_clock_rate)
{
    struct clock_di_div *di_div = to_clock_di_div(hw);
    struct ipu_soc      *ipu    = ipu_get_soc(di_div->ipu_id);
    uint64_t             div, parent_rate;
    uint64_t             remainder;

    parent_rate = (uint64_t)parent_clock_rate * 16;
    div         = parent_rate;
    div         = do_udiv64(div, rate, &remainder);

    /* Round the divider value */
    if (remainder > (rate / 2)) {
        div++;
    }

    /* Round up divider if it gets us closer to desired pix clk */
    if ((div & 0xC) == 0xC) {
        div += 0x10;
        div &= ~0xF;
    }

    if (div > 0x1000) {
        pr_err("Overflow, di:%d, DI_BS_CLKGEN0 div:0x%x\n", di_div->di_id, (uint32_t)div);
    }

    _ipu_get(ipu);
    ipu_di_write(ipu, di_div->di_id, (uint32_t)div, DI_BS_CLKGEN0);

    /* Setup pixel clock timing */
    /* FIXME: needs to be more flexible */
    /* Down time is half of period */
    ipu_di_write(ipu, di_div->di_id, ((uint32_t)div / 16) << 16, DI_BS_CLKGEN1);
    _ipu_put(ipu);

    return 0;
}

static struct clock_ops clock_div_ops = {
    .recalc_rate = _ipu_pixel_clock_div_recalc_rate,
    .round_rate  = _ipu_pixel_clock_div_round_rate,
    .set_rate    = _ipu_pixel_clock_div_set_rate,
};

struct clk *clock_register_div_pix_clock(
    struct device *dev, const char *name, const char *parent_name, uint64_t flags, uint8_t ipu_id, uint8_t di_id, uint8_t clock_div_flags)
{
    struct clock_di_div   *di_div;
    struct clk            *clk;
    struct clock_init_data init;

    di_div = kzalloc(sizeof(struct clock_di_div), GFP_KERNEL);

    if (!di_div) {
        return ERR_PTR(-ENOMEM);
    }

    /* struct clock_di_div assignments */
    di_div->ipu_id    = ipu_id;
    di_div->di_id     = di_id;
    di_div->flags     = clock_div_flags;

    init.name         = name;
    init.ops          = &clock_div_ops;
    init.flags        = flags | CLK_SET_RATE_PARENT;
    init.parent_names = parent_name ? &parent_name : NULL;
    init.num_parents  = parent_name ? 1 : 0;

    di_div->hw.init   = &init;

    clk               = clock_register(dev, &di_div->hw);

    if (IS_ERR(clk)) {
        kfree(clk);
    }

    return clk;
}

/*
 * Gated clock implementation
 */
struct clock_di_gate {
    struct clock_hw hw;
    uint8_t         ipu_id;
    uint8_t         di_id;
    uint8_t         flags;
};

#define to_clock_di_gate(_hw) container_of(_hw, struct clock_di_gate, hw)

static int _ipu_pixel_clock_enable(struct clock_hw *hw)
{
    struct clock_di_gate *gate = to_clock_di_gate(hw);
    struct ipu_soc       *ipu  = ipu_get_soc(gate->ipu_id);
    uint32_t              disp_gen;

    disp_gen = ipu_cm_read(ipu, IPU_DISP_GEN);
    disp_gen |= gate->di_id ? DI1_COUNTER_RELEASE : DI0_COUNTER_RELEASE;
    ipu_cm_write(ipu, disp_gen, IPU_DISP_GEN);

    return 0;
}

static void _ipu_pixel_clock_disable(struct clock_hw *hw)
{
    struct clock_di_gate *gate = to_clock_di_gate(hw);
    struct ipu_soc       *ipu  = ipu_get_soc(gate->ipu_id);
    uint32_t              disp_gen;

    disp_gen = ipu_cm_read(ipu, IPU_DISP_GEN);
    disp_gen &= gate->di_id ? ~DI1_COUNTER_RELEASE : ~DI0_COUNTER_RELEASE;
    ipu_cm_write(ipu, disp_gen, IPU_DISP_GEN);
}

static struct clock_ops clock_gate_di_ops = {
    .enable  = _ipu_pixel_clock_enable,
    .disable = _ipu_pixel_clock_disable,
};

struct clk *clock_register_gate_pix_clock(
    struct device *dev, const char *name, const char *parent_name, uint64_t flags, uint8_t ipu_id, uint8_t di_id, uint8_t clock_gate_flags)
{
    struct clock_di_gate  *gate;
    struct clk            *clk;
    struct clock_init_data init;

    gate = kzalloc(sizeof(struct clock_di_gate), GFP_KERNEL);

    if (!gate) {
        return ERR_PTR(-ENOMEM);
    }

    gate->ipu_id      = ipu_id;
    gate->di_id       = di_id;
    gate->flags       = clock_gate_flags;

    init.name         = name;
    init.ops          = &clock_gate_di_ops;
    init.flags        = flags | CLK_SET_RATE_PARENT;
    init.parent_names = parent_name ? &parent_name : NULL;
    init.num_parents  = parent_name ? 1 : 0;

    gate->hw.init     = &init;

    clk               = clock_register(dev, &gate->hw);

    return clk;
}
