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
 * @file clk-bulk.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic clocking bulk APIs
 *
 * Adapted from linux/drivers/clk/clk-bulk.c
 *
 * Copyright 2017 NXP
 *
 * Dong Aisheng <aisheng.dong@nxp.com>
 *
 * The original source is licensed under GPL.
 */

#include <drv/clk.h>
#include <vmm_device_driver.h>
#include <vmm_error.h>
#include <vmm_modules.h>

void clock_bulk_put(int num_clocks, struct clock_bulk_data *clks)
{
    while (--num_clocks >= 0) {
        clock_put(clks[num_clocks].clk);
        clks[num_clocks].clk = NULL;
    }
}

VMM_ERR_XPORT_SYMBOL(clock_bulk_put);

int clock_bulk_get(vmm_device_t *dev, int num_clocks, struct clock_bulk_data *clks)
{
    int ret;
    int i;

    for (i = 0; i < num_clocks; i++) {
        clks[i].clk = NULL;
    }

    for (i = 0; i < num_clocks; i++) {
        clks[i].clk = clock_get(dev, clks[i].id);

        if (VMM_IS_ERR(clks[i].clk)) {
            ret = VMM_PTR_ERR(clks[i].clk);
            vmm_lerror(dev->name, "Failed to get clk '%s': %d\n", clks[i].id, ret);
            clks[i].clk = NULL;
            goto err;
        }
    }

    return 0;

err:
    clock_bulk_put(i, clks);

    return ret;
}

VMM_ERR_XPORT_SYMBOL(clock_bulk_get);

#ifdef CONFIG_HAVE_CLK_PREPARE

/**
 * clock_bulk_unprepare - undo preparation of a set of clock sources
 * @num_clocks: the number of clock_bulk_data
 * @clks: the clock_bulk_data table being unprepared
 *
 * clock_bulk_unprepare may sleep, which differentiates it from clock_bulk_disable.
 * Returns 0 on success, VMM_ERR_ERROR otherwise.
 */
void clock_bulk_unprepare(int num_clocks, const struct clock_bulk_data *clks)
{
    while (--num_clocks >= 0) {
        clock_unprepare(clks[num_clocks].clk);
    }
}

VMM_ERR_XPORT_SYMBOL(clock_bulk_unprepare);

/**
 * clock_bulk_prepare - prepare a set of clocks
 * @num_clocks: the number of clock_bulk_data
 * @clks: the clock_bulk_data table being prepared
 *
 * clock_bulk_prepare may sleep, which differentiates it from clock_bulk_enable.
 * Returns 0 on success, VMM_ERR_ERROR otherwise.
 */
int clock_bulk_prepare(int num_clocks, const struct clock_bulk_data *clks)
{
    int ret;
    int i;

    for (i = 0; i < num_clocks; i++) {
        ret = clock_prepare(clks[i].clk);

        if (ret) {
            vmm_lerror(__func__, "Failed to prepare clk '%s': %d\n", clks[i].id, ret);
            goto err;
        }
    }

    return 0;

err:
    clock_bulk_unprepare(i, clks);

    return ret;
}

VMM_ERR_XPORT_SYMBOL(clock_bulk_prepare);

#endif /* CONFIG_HAVE_CLK_PREPARE */

/**
 * clock_bulk_disable - gate a set of clocks
 * @num_clocks: the number of clock_bulk_data
 * @clks: the clock_bulk_data table being gated
 *
 * clock_bulk_disable must not sleep, which differentiates it from
 * clock_bulk_unprepare. clock_bulk_disable must be called before
 * clock_bulk_unprepare.
 */
void clock_bulk_disable(int num_clocks, const struct clock_bulk_data *clks)
{

    while (--num_clocks >= 0) {
        clock_disable(clks[num_clocks].clk);
    }
}

VMM_ERR_XPORT_SYMBOL(clock_bulk_disable);

/**
 * clock_bulk_enable - ungate a set of clocks
 * @num_clocks: the number of clock_bulk_data
 * @clks: the clock_bulk_data table being ungated
 *
 * clock_bulk_enable must not sleep
 * Returns 0 on success, VMM_ERR_ERROR otherwise.
 */
int clock_bulk_enable(int num_clocks, const struct clock_bulk_data *clks)
{
    int ret;
    int i;

    for (i = 0; i < num_clocks; i++) {
        ret = clock_enable(clks[i].clk);

        if (ret) {
            vmm_lerror(__func__, "Failed to enable clk '%s': %d\n", clks[i].id, ret);
            goto err;
        }
    }

    return 0;

err:
    clock_bulk_disable(i, clks);

    return ret;
}

VMM_ERR_XPORT_SYMBOL(clock_bulk_enable);
