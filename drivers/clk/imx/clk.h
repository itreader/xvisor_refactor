/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Adapted from Linux Kernel 3.13.6 arch/arm/mach-imx/clk.h
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
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Freescale i.MX clock management function helper header
 */

#ifndef __MACH_IMX_CLK_H
#define __MACH_IMX_CLK_H

#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/spinlock.h>

#define __iomem

extern spinlock_t imx_ccm_lock;

void imx_check_clocks(struct clk *clks[], uint32_t count);

extern void imx_cscmr1_fixup(uint32_t *val);

struct clk *imx_clock_pllv1(const char *name, const char *parent, void __iomem *base);

struct clk *imx_clock_pllv2(const char *name, const char *parent, void __iomem *base);

enum imx_pllv3_type {
    IMX_PLLV3_GENERIC,
    IMX_PLLV3_SYS,
    IMX_PLLV3_USB,
    IMX_PLLV3_AV,
    IMX_PLLV3_ENET,
};

struct clk *imx_clock_pllv3(enum imx_pllv3_type type, const char *name, const char *parent_name, void __iomem *base, uint32_t div_mask);

struct clk *clock_register_gate2(
    struct device *dev, const char *name, const char *parent_name, uint64_t flags, void __iomem *reg, uint8_t bit_idx, uint8_t clock_gate_flags,
    spinlock_t *lock, uint32_t *share_count);

struct clk *imx_obtain_fixed_clock(const char *name, uint64_t rate);

static inline struct clk *imx_clock_gate2(const char *name, const char *parent, void __iomem *reg, uint8_t shift)
{
    return clock_register_gate2(NULL, name, parent, CLK_SET_RATE_PARENT, reg, shift, 0, &imx_ccm_lock, NULL);
}

static inline struct clk *imx_clock_gate2_shared(const char *name, const char *parent, void __iomem *reg, uint8_t shift, uint32_t *share_count)
{
    return clock_register_gate2(NULL, name, parent, CLK_SET_RATE_PARENT, reg, shift, 0, &imx_ccm_lock, share_count);
}

struct clk *imx_clock_pfd(const char *name, const char *parent_name, void __iomem *reg, uint8_t idx);

struct clk *imx_clock_busy_divider(
    const char *name, const char *parent_name, void __iomem *reg, uint8_t shift, uint8_t width, void __iomem *busy_reg, uint8_t busy_shift);

struct clk *imx_clock_busy_mux(
    const char *name, void __iomem *reg, uint8_t shift, uint8_t width, void __iomem *busy_reg, uint8_t busy_shift, const char **parent_names,
    int num_parents);

struct clk *imx_clock_fixup_divider(
    const char *name, const char *parent, void __iomem *reg, uint8_t shift, uint8_t width, void (*fixup)(uint32_t *val));

struct clk *imx_clock_fixup_mux(
    const char *name, void __iomem *reg, uint8_t shift, uint8_t width, const char **parents, int num_parents, void (*fixup)(uint32_t *val));

static inline struct clk *imx_clock_fixed(const char *name, int rate)
{
    return clock_register_fixed_rate(NULL, name, NULL, CLK_IS_ROOT, rate);
}

static inline struct clk *imx_clock_divider(const char *name, const char *parent, void __iomem *reg, uint8_t shift, uint8_t width)
{
    return clock_register_divider(NULL, name, parent, CLK_SET_RATE_PARENT, reg, shift, width, 0, &imx_ccm_lock);
}

static inline struct clk *imx_clock_divider_flags(
    const char *name, const char *parent, void __iomem *reg, uint8_t shift, uint8_t width, uint64_t flags)
{
    return clock_register_divider(NULL, name, parent, flags, reg, shift, width, 0, &imx_ccm_lock);
}

static inline struct clk *imx_clock_gate(const char *name, const char *parent, void __iomem *reg, uint8_t shift)
{
    return clock_register_gate(NULL, name, parent, CLK_SET_RATE_PARENT, reg, shift, 0, &imx_ccm_lock);
}

static inline struct clk *imx_clock_mux(const char *name, void __iomem *reg, uint8_t shift, uint8_t width, const char **parents, int num_parents)
{
    return clock_register_mux(NULL, name, parents, num_parents, CLK_SET_RATE_NO_REPARENT, reg, shift, width, 0, &imx_ccm_lock);
}

static inline struct clk *imx_clock_mux_flags(
    const char *name, void __iomem *reg, uint8_t shift, uint8_t width, const char **parents, int num_parents, uint64_t flags)
{
    return clock_register_mux(NULL, name, parents, num_parents, flags | CLK_SET_RATE_NO_REPARENT, reg, shift, width, 0, &imx_ccm_lock);
}

static inline struct clk *imx_clock_fixed_factor(const char *name, const char *parent, uint32_t mult, uint32_t div)
{
    return clock_register_fixed_factor(NULL, name, parent, CLK_SET_RATE_PARENT, mult, div);
}

#endif
