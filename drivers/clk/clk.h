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
 * @file clk.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief internal clk header
 *
 * Adapted from linux/drivers/clk/clk.h
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 * Sylwester Nawrocki <s.nawrocki@samsung.com>
 *
 * The original source is licensed under GPL.
 */

struct clock_hw;

#if defined(CONFIG_OF) && defined(CONFIG_COMMON_CLK)
struct clk *__of_clock_get_from_provider(struct vmm_device_tree_phandle_args *clkspec, const char *dev_id, const char *con_id);
#endif

#ifdef CONFIG_COMMON_CLK
struct clk *__clock_create_clock(struct clock_hw *hw, const char *dev_id, const char *con_id);
void        __clock_free_clock(struct clk *clk);
#else
/* All these casts to avoid ifdefs in clkdev... */
static inline struct clk *__clock_create_clock(struct clock_hw *hw, const char *dev_id, const char *con_id)
{
    return (struct clk *)hw;
}

static inline void __clock_free_clock(struct clk *clk) {}

static struct clock_hw *__clock_get_hw(struct clk *clk)
{
    return (struct clock_hw *)clk;
}

#endif
