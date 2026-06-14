/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file rtc-pl031.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Real Time Clock interface for ARM AMBA PrimeCell 031 RTC
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/rtc/rtc-pl031.c
 *
 * Author: Deepak Saxena <dsaxena@plexity.net>
 *
 * Copyright 2006 (c) MontaVista Software, Inc.
 *
 * Author: Mian Yousaf Kaukab <mian.yousaf.kaukab@stericsson.com>
 * Copyright 2010 (c) ST-Ericsson AB
 *
 * The original code is licensed under the GPL.
 */

#include <drv/amba/bus.h>
#include <drv/rtc.h>
#include <libs/bcd.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_device_driver.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

#define MODULE_DESC      "PL031 RTC Driver"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (RTC_DEVICE_CLASS_IPRIORITY + 1)
#define MODULE_INIT      pl031_driver_init
#define MODULE_EXIT      pl031_driver_exit

/*
 * Register definitions
 */
#define RTC_DR           0x00 /* Data read register */
#define RTC_MR           0x04 /* Match register */
#define RTC_LR           0x08 /* Data load register */
#define RTC_CR           0x0c /* Control register */
#define RTC_IMSC         0x10 /* Interrupt mask and set register */
#define RTC_RIS          0x14 /* Raw interrupt status register */
#define RTC_MIS          0x18 /* Masked interrupt status register */
#define RTC_ICR          0x1c /* Interrupt clear register */
/* ST variants have additional timer functionality */
#define RTC_TDR          0x20      /* Timer data read register */
#define RTC_TLR          0x24      /* Timer data load register */
#define RTC_TCR          0x28      /* Timer control register */
#define RTC_YDR          0x30      /* Year data read register */
#define RTC_YMR          0x34      /* Year match register */
#define RTC_YLR          0x38      /* Year data load register */

#define RTC_CR_CWEN      (1 << 26) /* Clockwatch enable bit */

#define RTC_TCR_EN       (1 << 1)  /* Periodic timer enable bit */

/* Common bit definitions for Interrupt status and control registers */
#define RTC_BIT_AI       (1 << 0) /* Alarm interrupt bit */
#define RTC_BIT_PI       (1 << 1) /* Periodic interrupt bit. ST variants only. */

/* Common bit definations for ST v2 for reading/writing time */
#define RTC_SEC_SHIFT    0
#define RTC_SEC_MASK     (0x3F << RTC_SEC_SHIFT)  /* Second [0-59] */
#define RTC_MIN_SHIFT    6
#define RTC_MIN_MASK     (0x3F << RTC_MIN_SHIFT)  /* Minute [0-59] */
#define RTC_HOUR_SHIFT   12
#define RTC_HOUR_MASK    (0x1F << RTC_HOUR_SHIFT) /* Hour [0-23] */
#define RTC_WDAY_SHIFT   17
#define RTC_WDAY_MASK    (0x7 << RTC_WDAY_SHIFT)  /* Day of Week [1-7] 1=Sunday */
#define RTC_MDAY_SHIFT   20
#define RTC_MDAY_MASK    (0x1F << RTC_MDAY_SHIFT) /* Day of Month [1-31] */
#define RTC_MON_SHIFT    25
#define RTC_MON_MASK     (0xF << RTC_MON_SHIFT)   /* Month [1-12] 1=January */

#define RTC_TIMER_FREQ   32768

struct pl031_local {
    struct rtc_device *rtc;
    void              *base;
    uint32_t           irq;
    uint8_t            hw_designer;
    uint8_t            hw_revision : 4;
};

static int pl031_alarm_irq_enable(struct rtc_device *rd, uint32_t enabled)
{
    uint64_t            imsc;
    struct pl031_local *ldata = rd->private;

    /* Clear any pending alarm interrupts. */
    vmm_writel(RTC_BIT_AI, (void *)(ldata->base + RTC_ICR));

    imsc = vmm_readl((void *)(ldata->base + RTC_IMSC));

    if (enabled == 1) {
        vmm_writel(imsc | RTC_BIT_AI, (void *)(ldata->base + RTC_IMSC));
    } else {
        vmm_writel(imsc & ~RTC_BIT_AI, (void *)(ldata->base + RTC_IMSC));
    }

    return 0;
}

/*
 * Convert Gregorian date to ST v2 RTC format.
 */
static int pl031_stv2_tm_to_time(struct rtc_device *rd, struct rtc_time *tm, uint64_t *st_time, uint64_t *bcd_year)
{
    int year = tm->tm_year + 1900;
    int wday = tm->tm_week_of_day;

    /* wday masking is not working in hardware so wday must be valid */
    if (wday < -1 || wday > 6) {
        vmm_lerror(rd->dev.name, "invalid wday value %d\n", tm->tm_week_of_day);
        return VMM_ERR_INVALID;
    } else if (wday == -1) {
        /* wday is not provided, calculate it here */
        uint64_t        time;
        struct rtc_time calc_tm;

        rtc_tm_to_time(tm, &time);
        rtc_time_to_tm(time, &calc_tm);
        wday = calc_tm.tm_week_of_day;
    }

    *bcd_year = (bin2bcd(year % 100) | bin2bcd(year / 100) << 8);

    *st_time  = ((tm->tm_month + 1) << RTC_MON_SHIFT) | (tm->tm_month_of_day << RTC_MDAY_SHIFT) | ((wday + 1) << RTC_WDAY_SHIFT) |
               (tm->tm_hour << RTC_HOUR_SHIFT) | (tm->tm_minute << RTC_MIN_SHIFT) | (tm->tm_second << RTC_SEC_SHIFT);

    return 0;
}

/*
 * Convert ST v2 RTC format to Gregorian date.
 */
static int pl031_stv2_time_to_tm(uint64_t st_time, uint64_t bcd_year, struct rtc_time *tm)
{
    tm->tm_year         = bcd2bin(bcd_year) + (bcd2bin(bcd_year >> 8) * 100);
    tm->tm_month        = ((st_time & RTC_MON_MASK) >> RTC_MON_SHIFT) - 1;
    tm->tm_month_of_day = ((st_time & RTC_MDAY_MASK) >> RTC_MDAY_SHIFT);
    tm->tm_week_of_day  = ((st_time & RTC_WDAY_MASK) >> RTC_WDAY_SHIFT) - 1;
    tm->tm_hour         = ((st_time & RTC_HOUR_MASK) >> RTC_HOUR_SHIFT);
    tm->tm_minute       = ((st_time & RTC_MIN_MASK) >> RTC_MIN_SHIFT);
    tm->tm_second       = ((st_time & RTC_SEC_MASK) >> RTC_SEC_SHIFT);

    tm->tm_year_of_day  = rtc_year_days(tm->tm_month_of_day, tm->tm_month, tm->tm_year);
    tm->tm_year -= 1900;

    return 0;
}

static int pl031_stv2_read_time(struct rtc_device *rd, struct rtc_time *tm)
{
    struct pl031_local *ldata = rd->private;

    pl031_stv2_time_to_tm(vmm_readl(ldata->base + RTC_DR), vmm_readl(ldata->base + RTC_YDR), tm);

    return 0;
}

static int pl031_stv2_set_time(struct rtc_device *rd, struct rtc_time *tm)
{
    uint64_t            time;
    uint64_t            bcd_year;
    struct pl031_local *ldata = rd->private;
    int                 ret;

    ret = pl031_stv2_tm_to_time(rd, tm, &time, &bcd_year);

    if (ret == 0) {
        vmm_writel(bcd_year, ldata->base + RTC_YLR);
        vmm_writel(time, ldata->base + RTC_LR);
    }

    return ret;
}

static int pl031_stv2_read_alarm(struct rtc_device *rd, struct rtc_wkalrm *alarm)
{
    struct pl031_local *ldata = rd->private;
    int                 ret;

    ret            = pl031_stv2_time_to_tm(vmm_readl(ldata->base + RTC_MR), vmm_readl(ldata->base + RTC_YMR), &alarm->time);

    alarm->pending = vmm_readl(ldata->base + RTC_RIS) & RTC_BIT_AI;
    alarm->enabled = vmm_readl(ldata->base + RTC_IMSC) & RTC_BIT_AI;

    return ret;
}

static int pl031_stv2_set_alarm(struct rtc_device *rd, struct rtc_wkalrm *alarm)
{
    struct pl031_local *ldata = rd->private;
    uint64_t            time;
    uint64_t            bcd_year;
    int                 ret;

    /* At the moment, we can only deal with non-wildcarded alarm times. */
    ret = rtc_valid_tm(&alarm->time);

    if (ret == 0) {
        ret = pl031_stv2_tm_to_time(rd, &alarm->time, &time, &bcd_year);

        if (ret == 0) {
            vmm_writel(bcd_year, ldata->base + RTC_YMR);
            vmm_writel(time, ldata->base + RTC_MR);

            pl031_alarm_irq_enable(rd, alarm->enabled);
        }
    }

    return ret;
}

static vmm_irq_return_t pl031_irq_handler(int irq_no, void *dev)
{
    struct pl031_local *ldata = (struct pl031_local *)dev;
    uint64_t            rtcmis;
    uint64_t            events = 0;

    rtcmis                     = vmm_readl((void *)(ldata->base + RTC_MIS));

    if (rtcmis) {
        vmm_writel(rtcmis, (void *)(ldata->base + RTC_ICR));

        if (rtcmis & RTC_BIT_AI) {
            events |= (RTC_AF | RTC_IRQF);
        }

        /* Timer interrupt is only available in ST variants */
        if ((rtcmis & RTC_BIT_PI) && (ldata->hw_designer == AMBA_VENDOR_ST)) {
            events |= (RTC_PF | RTC_IRQF);
        }

        rtc_update_irq(ldata->rtc, 1, events);

        return VMM_IRQ_HANDLED;
    }

    return VMM_IRQ_NONE;
}

static int pl031_read_time(struct rtc_device *rd, struct rtc_time *tm)
{
    struct pl031_local *ldata = rd->private;

    rtc_time_to_tm(vmm_readl(ldata->base + RTC_DR), tm);

    return 0;
}

static int pl031_set_time(struct rtc_device *rd, struct rtc_time *tm)
{
    uint64_t            time;
    struct pl031_local *ldata = rd->private;
    int                 ret;

    ret = rtc_tm_to_time(tm, &time);

    if (ret == 0) {
        vmm_writel(time, ldata->base + RTC_LR);
    }

    return ret;
}

static int pl031_read_alarm(struct rtc_device *rd, struct rtc_wkalrm *alarm)
{
    struct pl031_local *ldata = rd->private;

    rtc_time_to_tm(vmm_readl(ldata->base + RTC_MR), &alarm->time);

    alarm->pending = vmm_readl(ldata->base + RTC_RIS) & RTC_BIT_AI;
    alarm->enabled = vmm_readl(ldata->base + RTC_IMSC) & RTC_BIT_AI;

    return 0;
}

static int pl031_set_alarm(struct rtc_device *rd, struct rtc_wkalrm *alarm)
{
    struct pl031_local *ldata = rd->private;
    uint64_t            time;
    int                 ret;

    /* At the moment, we can only deal with non-wildcarded alarm times. */
    ret = rtc_valid_tm(&alarm->time);

    if (ret == 0) {
        ret = rtc_tm_to_time(&alarm->time, &time);

        if (ret == 0) {
            vmm_writel(time, ldata->base + RTC_MR);
            pl031_alarm_irq_enable(rd, alarm->enabled);
        }
    }

    return ret;
}

static struct rtc_class_ops pl031_arm_ops = {
    .read_time        = pl031_read_time,
    .set_time         = pl031_set_time,
    .read_alarm       = pl031_read_alarm,
    .set_alarm        = pl031_set_alarm,
    .alarm_irq_enable = pl031_alarm_irq_enable,
};

static struct rtc_class_ops pl031_stv1_ops = {
    .read_time        = pl031_read_time,
    .set_time         = pl031_set_time,
    .read_alarm       = pl031_read_alarm,
    .set_alarm        = pl031_set_alarm,
    .alarm_irq_enable = pl031_alarm_irq_enable,
};

static struct rtc_class_ops pl031_stv2_ops = {
    .read_time        = pl031_stv2_read_time,
    .set_time         = pl031_stv2_set_time,
    .read_alarm       = pl031_stv2_read_alarm,
    .set_alarm        = pl031_stv2_set_alarm,
    .alarm_irq_enable = pl031_alarm_irq_enable,
};

static int pl031_driver_probe(vmm_device_t *dev)
{
    int                   rc;
    uint32_t              periphid;
    virtual_addr_t        reg_base;
    struct rtc_class_ops *ops = NULL;
    struct pl031_local   *ldata;

    ldata = vmm_zalloc(sizeof(struct pl031_local));

    if (!ldata) {
        rc = VMM_ERR_NOMEM;
        goto free_nothing;
    }

    rc = vmm_device_tree_request_regmap(dev->of_node, &reg_base, 0, "PL031 RTC");

    if (rc) {
        goto free_ldata;
    }

    ldata->base        = (void *)reg_base;

    ldata->hw_designer = amba_manf(dev);
    ldata->hw_revision = amba_rev(dev);

    ldata->irq         = vmm_device_tree_irq_parse_map(dev->of_node, 0);

    if (!ldata->irq) {
        rc = VMM_ERR_NODEV;
        goto free_reg;
    }

    if ((rc = vmm_host_irq_register(ldata->irq, dev->name, pl031_irq_handler, ldata))) {
        goto free_reg;
    }

    periphid = amba_periphid(dev);

    if ((periphid & 0x000fffff) == 0x00041031) {
        /* ARM variant */
        ops = &pl031_arm_ops;
    } else if ((periphid & 0x00ffffff) == 0x00180031) {
        /* ST Micro variant - stv1 */
        ops = &pl031_stv1_ops;
    } else if ((periphid & 0x00ffffff) == 0x00280031) {
        /* ST Micro variant - stv2 */
        ops = &pl031_stv2_ops;
    } else {
        rc = VMM_ERR_FAIL;
        goto free_irq;
    }

    ldata->rtc = rtc_device_register(dev, dev->name, ops, ldata);

    if (VMM_IS_ERR(ldata->rtc)) {
        rc = VMM_PTR_ERR(ldata->rtc);
        goto free_irq;
    }

    vmm_device_driver_set_data(dev, ldata);

    return VMM_OK;

free_irq:
    vmm_host_irq_unregister(ldata->irq, ldata);
free_reg:
    vmm_device_tree_regunmap_release(dev->of_node, (virtual_addr_t)ldata->base, 0);
free_ldata:
    vmm_free(ldata);
free_nothing:
    return rc;
}

static int pl031_driver_remove(vmm_device_t *dev)
{
    struct pl031_local *ldata = vmm_device_driver_get_data(dev);

    if (ldata) {
        rtc_device_unregister(ldata->rtc);
        vmm_host_irq_unregister(ldata->irq, ldata);
        vmm_device_tree_regunmap_release(dev->of_node, (virtual_addr_t)ldata->base, 0);
        vmm_free(ldata);
        dev->private = NULL;
    }

    return VMM_OK;
}

static struct vmm_device_tree_nodeid pl031_devid_table[] = {
    {.compatible = "arm,pl031"},
    {/* end of list */},
};

static vmm_driver_t pl031_driver = {
    .name        = "pl031_rtc",
    .match_table = pl031_devid_table,
    .probe       = pl031_driver_probe,
    .remove      = pl031_driver_remove,
};

static int __init pl031_driver_init(void)
{
    return vmm_device_driver_register_driver(&pl031_driver);
}

static void __exit pl031_driver_exit(void)
{
    vmm_device_driver_unregister_driver(&pl031_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
