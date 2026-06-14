/**
 * Copyright (c) 2013 Jean-Christophe Dubois.
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
 * @file rtc-s3c.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Exynos RTC support code
 *
 * Adapted from linux/drivers/rtc/rtc-s3c.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com/
 *
 * Copyright (c) 2004,2006 Simtec Electronics
 *  Ben Dooks, <ben@simtec.co.uk>
 *  http://armlinux.simtec.co.uk/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * S3C2410/S3C2440/S3C24XX Internal RTC Driver
 */

#include <linux/bcd.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <exynos/regs-clock.h>
#include <exynos/regs-rtc.h>

#include <exynos/mach/map.h>
#include <vmm_host_address_space.h>

#define MODULE_DESC      "S3C RTC Driver"
#define MODULE_AUTHOR    "Jean-Christophe Dubois"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (RTC_DEVICE_CLASS_IPRIORITY + 1)
#define MODULE_INIT      s3c_rtc_driver_init
#define MODULE_EXIT      s3c_rtc_driver_exit

enum s3c_cpu_type {
    TYPE_S3C2410,
    TYPE_S3C2416,
    TYPE_S3C2443,
    TYPE_S3C64XX,
};

/* I have yet to find an S3C implementation with more than one
 * of these rtc blocks in */

static struct clk       *rtc_clock;
static void __iomem     *s3c_rtc_base;
static int               s3c_rtc_alarmno = NO_IRQ;
static int               s3c_rtc_tickno  = NO_IRQ;
static enum s3c_cpu_type s3c_rtc_cpu_type;

static DEFINE_SPINLOCK(s3c_rtc_pie_lock);

/**
 * This is a temporary solution until we have a clock management
 * API
 */
static int clock_enable(struct clk *clk)
{
    uint32_t perir_reg = vmm_readl((void *)clk);

    if (!(perir_reg & (1 << 15))) {
        perir_reg |= (1 << 15);

        vmm_writel(perir_reg, (void *)clk);
    }

    return 0;
}

/**
 * This is a temporary solution until we have a clock management
 * API
 */
static void clock_disable(struct clk *clk)
{
    uint32_t perir_reg = vmm_readl((void *)clk);

    if (perir_reg & (1 << 15)) {
        perir_reg &= ~(1 << 15);

        vmm_writel(perir_reg, (void *)clk);
    }
}

/**
 * This is a temporary solution until we have a clock management
 * API
 */
static void clock_put(struct clk *clk)
{
    vmm_host_iounmap((virtual_addr_t)clk);
}

/**
 * This is a temporary solution until we have a clock management
 * API
 */
static struct clk *clock_get(vmm_device_t *dev, const char *name)
{
    void *cmu_ptr = (void *)vmm_host_iomap(EXYNOS4_PA_CMU + EXYNOS4_CLKGATE_IP_PERIR, sizeof(uint32_t));

    return cmu_ptr;
}

static bool is_power_of_2(uint64_t n)
{
    return (n && !(n & (n - 1)));
}

static void s3c_rtc_alarm_clock_enable(bool enable)
{
    static DEFINE_SPINLOCK(s3c_rtc_alarm_clock_lock);
    static bool alarm_clock_enabled;
    uint64_t    irq_flags;

    spin_lock_irq_save(&s3c_rtc_alarm_clock_lock, irq_flags);

    if (enable) {
        if (!alarm_clock_enabled) {
            clock_enable(rtc_clock);
            alarm_clock_enabled = true;
        }
    } else {
        if (alarm_clock_enabled) {
            clock_disable(rtc_clock);
            alarm_clock_enabled = false;
        }
    }

    spin_unlock_irq_restore(&s3c_rtc_alarm_clock_lock, irq_flags);
}

/* IRQ Handlers */

static irqreturn_t s3c_rtc_alarmirq(int irq, void *id)
{
    // struct rtc_device *rdev = id;

    clock_enable(rtc_clock);
    rtc_update_irq(rdev, 1, RTC_AF | RTC_IRQF);

    if (s3c_rtc_cpu_type == TYPE_S3C64XX) {
        writeb(S3C2410_INTP_ALM, s3c_rtc_base + S3C2410_INTP);
    }

    clock_disable(rtc_clock);

    s3c_rtc_alarm_clock_enable(false);

    return IRQ_HANDLED;
}

static irqreturn_t s3c_rtc_tickirq(int irq, void *id)
{
    // struct rtc_device *rdev = id;

    clock_enable(rtc_clock);
    rtc_update_irq(rdev, 1, RTC_PF | RTC_IRQF);

    if (s3c_rtc_cpu_type == TYPE_S3C64XX) {
        writeb(S3C2410_INTP_TIC, s3c_rtc_base + S3C2410_INTP);
    }

    clock_disable(rtc_clock);
    return IRQ_HANDLED;
}

/* Update control registers */
static int s3c_rtc_setaie(struct rtc_device *dev, uint32_t enabled)
{
    uint32_t tmp;

    clock_enable(rtc_clock);
    tmp = readb(s3c_rtc_base + S3C2410_RTCALM) & ~S3C2410_RTCALM_ALMEN;

    if (enabled) {
        tmp |= S3C2410_RTCALM_ALMEN;
    }

    writeb(tmp, s3c_rtc_base + S3C2410_RTCALM);
    clock_disable(rtc_clock);

    s3c_rtc_alarm_clock_enable(enabled);

    return 0;
}

static int max_user_freq;

static int s3c_rtc_setfreq(struct rtc_device *rtc_dev, int freq)
{
    // struct platform_device *pdev = to_platform_device(dev);
    // struct rtc_device *rtc_dev = platform_get_drvdata(pdev);
    uint32_t tmp = 0;
    int      val;

    if (!is_power_of_2(freq)) {
        return -EINVAL;
    }

    clock_enable(rtc_clock);
    spin_lock_irq(&s3c_rtc_pie_lock);

    if (s3c_rtc_cpu_type != TYPE_S3C64XX) {
        tmp = readb(s3c_rtc_base + S3C2410_TICNT);
        tmp &= S3C2410_TICNT_ENABLE;
    }

    // val = (rtc_dev->max_user_freq / freq) - 1;
    val = udiv32(max_user_freq, freq) - 1;

    if (s3c_rtc_cpu_type == TYPE_S3C2416 || s3c_rtc_cpu_type == TYPE_S3C2443) {
        tmp |= S3C2443_TICNT_PART(val);
        writel(S3C2443_TICNT1_PART(val), s3c_rtc_base + S3C2443_TICNT1);

        if (s3c_rtc_cpu_type == TYPE_S3C2416) {
            writel(S3C2416_TICNT2_PART(val), s3c_rtc_base + S3C2416_TICNT2);
        }
    } else {
        tmp |= val;
    }

    writel(tmp, s3c_rtc_base + S3C2410_TICNT);
    spin_unlock_irq(&s3c_rtc_pie_lock);
    clock_disable(rtc_clock);

    return 0;
}

/* Time read/write */

static int s3c_rtc_gettime(struct rtc_device *dev, struct rtc_time *rtc_tm)
{
    uint32_t      have_retried = 0;
    void __iomem *base         = s3c_rtc_base;

    clock_enable(rtc_clock);
retry_get_time:
    rtc_tm->tm_minute       = readb(base + S3C2410_RTCMIN);
    rtc_tm->tm_hour         = readb(base + S3C2410_RTCHOUR);
    rtc_tm->tm_month_of_day = readb(base + S3C2410_RTCDATE);
    rtc_tm->tm_month        = readb(base + S3C2410_RTCMON);
    rtc_tm->tm_year         = readb(base + S3C2410_RTCYEAR);
    rtc_tm->tm_second       = readb(base + S3C2410_RTCSEC);

    /* the only way to work out wether the system was mid-update
     * when we read it is to check the second counter, and if it
     * is zero, then we re-try the entire read
     */

    if (rtc_tm->tm_second == 0 && !have_retried) {
        have_retried = 1;
        goto retry_get_time;
    }

    rtc_tm->tm_second       = bcd2bin(rtc_tm->tm_second);
    rtc_tm->tm_minute       = bcd2bin(rtc_tm->tm_minute);
    rtc_tm->tm_hour         = bcd2bin(rtc_tm->tm_hour);
    rtc_tm->tm_month_of_day = bcd2bin(rtc_tm->tm_month_of_day);
    rtc_tm->tm_month        = bcd2bin(rtc_tm->tm_month);
    rtc_tm->tm_year         = bcd2bin(rtc_tm->tm_year);

    rtc_tm->tm_year += 100;

    rtc_tm->tm_month -= 1;

    clock_disable(rtc_clock);

    return 0;
}

static int s3c_rtc_settime(struct rtc_device *dev, struct rtc_time *tm)
{
    void __iomem *base = s3c_rtc_base;
    int           year = tm->tm_year - 100;

    /* we get around y2k by simply not supporting it */

    if (year < 0 || year >= 100) {
        dev_err(&dev->dev, "rtc only supports 100 years\n");
        return -EINVAL;
    }

    clock_enable(rtc_clock);
    writeb(bin2bcd(tm->tm_second), base + S3C2410_RTCSEC);
    writeb(bin2bcd(tm->tm_minute), base + S3C2410_RTCMIN);
    writeb(bin2bcd(tm->tm_hour), base + S3C2410_RTCHOUR);
    writeb(bin2bcd(tm->tm_month_of_day), base + S3C2410_RTCDATE);
    writeb(bin2bcd(tm->tm_month + 1), base + S3C2410_RTCMON);
    writeb(bin2bcd(year), base + S3C2410_RTCYEAR);
    clock_disable(rtc_clock);

    return 0;
}

static int s3c_rtc_getalarm(struct rtc_device *dev, struct rtc_wkalrm *alrm)
{
    struct rtc_time *alm_tm = &alrm->time;
    void __iomem    *base   = s3c_rtc_base;
    uint32_t         alm_en;

    clock_enable(rtc_clock);
    alm_tm->tm_second       = readb(base + S3C2410_ALMSEC);
    alm_tm->tm_minute       = readb(base + S3C2410_ALMMIN);
    alm_tm->tm_hour         = readb(base + S3C2410_ALMHOUR);
    alm_tm->tm_month        = readb(base + S3C2410_ALMMON);
    alm_tm->tm_month_of_day = readb(base + S3C2410_ALMDATE);
    alm_tm->tm_year         = readb(base + S3C2410_ALMYEAR);

    alm_en                  = readb(base + S3C2410_RTCALM);

    alrm->enabled           = (alm_en & S3C2410_RTCALM_ALMEN) ? 1 : 0;

    /* decode the alarm enable field */

    if (alm_en & S3C2410_RTCALM_SECEN) {
        alm_tm->tm_second = bcd2bin(alm_tm->tm_second);
    } else {
        alm_tm->tm_second = -1;
    }

    if (alm_en & S3C2410_RTCALM_MINEN) {
        alm_tm->tm_minute = bcd2bin(alm_tm->tm_minute);
    } else {
        alm_tm->tm_minute = -1;
    }

    if (alm_en & S3C2410_RTCALM_HOUREN) {
        alm_tm->tm_hour = bcd2bin(alm_tm->tm_hour);
    } else {
        alm_tm->tm_hour = -1;
    }

    if (alm_en & S3C2410_RTCALM_DAYEN) {
        alm_tm->tm_month_of_day = bcd2bin(alm_tm->tm_month_of_day);
    } else {
        alm_tm->tm_month_of_day = -1;
    }

    if (alm_en & S3C2410_RTCALM_MONEN) {
        alm_tm->tm_month = bcd2bin(alm_tm->tm_month);
        alm_tm->tm_month -= 1;
    } else {
        alm_tm->tm_month = -1;
    }

    if (alm_en & S3C2410_RTCALM_YEAREN) {
        alm_tm->tm_year = bcd2bin(alm_tm->tm_year);
    } else {
        alm_tm->tm_year = -1;
    }

    clock_disable(rtc_clock);
    return 0;
}

static int s3c_rtc_setalarm(struct rtc_device *dev, struct rtc_wkalrm *alrm)
{
    struct rtc_time *tm   = &alrm->time;
    void __iomem    *base = s3c_rtc_base;
    uint32_t         alrm_en;

    clock_enable(rtc_clock);

    alrm_en = readb(base + S3C2410_RTCALM) & S3C2410_RTCALM_ALMEN;
    writeb(0x00, base + S3C2410_RTCALM);

    if (tm->tm_second < 60 && tm->tm_second >= 0) {
        alrm_en |= S3C2410_RTCALM_SECEN;
        writeb(bin2bcd(tm->tm_second), base + S3C2410_ALMSEC);
    }

    if (tm->tm_minute < 60 && tm->tm_minute >= 0) {
        alrm_en |= S3C2410_RTCALM_MINEN;
        writeb(bin2bcd(tm->tm_minute), base + S3C2410_ALMMIN);
    }

    if (tm->tm_hour < 24 && tm->tm_hour >= 0) {
        alrm_en |= S3C2410_RTCALM_HOUREN;
        writeb(bin2bcd(tm->tm_hour), base + S3C2410_ALMHOUR);
    }

    writeb(alrm_en, base + S3C2410_RTCALM);

    s3c_rtc_setaie(dev, alrm->enabled);

    clock_disable(rtc_clock);
    return 0;
}

static struct rtc_class_ops s3c_rtc_ops = {
    .read_time        = s3c_rtc_gettime,
    .set_time         = s3c_rtc_settime,
    .read_alarm       = s3c_rtc_getalarm,
    .set_alarm        = s3c_rtc_setalarm,
    .alarm_irq_enable = s3c_rtc_setaie,
};

static void s3c_rtc_enable(vmm_device_t *pdev, int en)
{
    void __iomem *base = s3c_rtc_base;
    uint32_t      tmp;

    if (s3c_rtc_base == NULL) {
        return;
    }

    clock_enable(rtc_clock);

    if (!en) {
        tmp = readw(base + S3C2410_RTCCON);

        if (s3c_rtc_cpu_type == TYPE_S3C64XX) {
            tmp &= ~S3C64XX_RTCCON_TICEN;
        }

        tmp &= ~S3C2410_RTCCON_RTCEN;
        writew(tmp, base + S3C2410_RTCCON);

        if (s3c_rtc_cpu_type != TYPE_S3C64XX) {
            tmp = readb(base + S3C2410_TICNT);
            tmp &= ~S3C2410_TICNT_ENABLE;
            writeb(tmp, base + S3C2410_TICNT);
        }
    } else {
        /* re-enable the device, and check it is ok */

        if ((readw(base + S3C2410_RTCCON) & S3C2410_RTCCON_RTCEN) == 0) {
            tmp = readw(base + S3C2410_RTCCON);
            writew(tmp | S3C2410_RTCCON_RTCEN, base + S3C2410_RTCCON);
        }

        if ((readw(base + S3C2410_RTCCON) & S3C2410_RTCCON_CNTSEL)) {
            tmp = readw(base + S3C2410_RTCCON);
            writew(tmp & ~S3C2410_RTCCON_CNTSEL, base + S3C2410_RTCCON);
        }

        if ((readw(base + S3C2410_RTCCON) & S3C2410_RTCCON_CLKRST)) {
            tmp = readw(base + S3C2410_RTCCON);
            writew(tmp & ~S3C2410_RTCCON_CLKRST, base + S3C2410_RTCCON);
        }
    }

    clock_disable(rtc_clock);
}

static int s3c_rtc_driver_remove(vmm_device_t *dev)
{
    struct rtc_device *rtc = dev->private;

    s3c_rtc_setaie(rtc, 0);

    vmm_host_irq_unregister(s3c_rtc_alarmno, rtc);
    vmm_host_irq_unregister(s3c_rtc_tickno, rtc);

    rtc_device_unregister(rtc);
    dev->private = NULL;

    clock_put(rtc_clock);
    rtc_clock = NULL;

    vmm_device_tree_regunmap_release(dev->of_node, (virtual_addr_t)s3c_rtc_base, 0);

    return 0;
}

static int s3c_rtc_driver_probe(vmm_device_t *pdev, const struct vmm_device_tree_nodeid *devid)
{
    uint32_t           alarmno, tickno;
    struct rtc_time    rtc_tm;
    struct rtc_device *rtc;
    int                ret = VMM_OK, tmp, rc;

    /* find the IRQs */

    alarmno                = vmm_device_tree_irq_parse_map(pdev->of_node, 0);

    if (!alarmno) {
        rc = VMM_ERR_NODEV;
        return rc;
    }

    s3c_rtc_alarmno = alarmno;
    tickno          = vmm_device_tree_irq_parse_map(pdev->of_node, 1);

    if (!tickno) {
        rc = VMM_ERR_NODEV;
        return rc;
    }

    s3c_rtc_tickno = tickno;

    /* get the memory region */

    rc             = vmm_device_tree_request_regmap(pdev->of_node, (virtual_addr_t *)&s3c_rtc_base, 0, "S3C RTC");

    if (rc) {
        dev_err(pdev, "failed ioremap()\n");
        ret = rc;
        goto err_nomap;
    }

    rtc_clock = clock_get(pdev, "rtc");

    if (rtc_clock == NULL) {
        dev_err(pdev, "failed to find rtc clock source\n");
        ret = -ENODEV;
        goto err_clock;
    }

    clock_enable(rtc_clock);

    /* check to see if everything is setup correctly */

    s3c_rtc_enable(pdev, 1);

    // device_init_wakeup(pdev, 1);

    /* register RTC and exit */

    rtc = rtc_device_register(pdev, pdev->name, &s3c_rtc_ops, NULL);

    if (VMM_IS_ERR(rtc)) {
        dev_err(pdev, "cannot attach rtc\n");
        ret = VMM_PTR_ERR(rtc);
        goto err_nortc;
    }

    s3c_rtc_cpu_type = (enum s3c_cpu_type)devid->data;

    /* Check RTC Time */

    s3c_rtc_gettime(NULL, &rtc_tm);

    if (!rtc_valid_tm(&rtc_tm)) {
        dev_warn(pdev, "warning: invalid RTC value so initializing it\n");

        rtc_tm.tm_year         = 100;
        rtc_tm.tm_month        = 0;
        rtc_tm.tm_month_of_day = 1;
        rtc_tm.tm_hour         = 0;
        rtc_tm.tm_minute       = 0;
        rtc_tm.tm_second       = 0;

        s3c_rtc_settime(NULL, &rtc_tm);
    }

    if (s3c_rtc_cpu_type != TYPE_S3C2410) {
        max_user_freq = 32768;
    } else {
        max_user_freq = 128;
    }

    if (s3c_rtc_cpu_type == TYPE_S3C2416 || s3c_rtc_cpu_type == TYPE_S3C2443) {
        tmp = readw(s3c_rtc_base + S3C2410_RTCCON);
        tmp |= S3C2443_RTCCON_TICSEL;
        writew(tmp, s3c_rtc_base + S3C2410_RTCCON);
    }

    s3c_rtc_setfreq(rtc, 1);

    if ((rc = vmm_host_irq_register(s3c_rtc_alarmno, "s3c_rtc_alarm", s3c_rtc_alarmirq, rtc))) {
        dev_err(pdev, "IRQ%d error %d\n", s3c_rtc_alarmno, rc);
        goto err_alarm_irq;
    }

    if ((rc = vmm_host_irq_register(s3c_rtc_tickno, "s3c_rtc_tick", s3c_rtc_tickirq, rtc))) {
        dev_err(pdev, "IRQ%d error %d\n", s3c_rtc_tickno, rc);
        goto err_tick_irq;
    }

    clock_disable(rtc_clock);

    return 0;

err_tick_irq:
    vmm_host_irq_unregister(s3c_rtc_alarmno, rtc);

err_alarm_irq:
    pdev->private = NULL;
    rtc_device_unregister(rtc);

err_nortc:
    s3c_rtc_enable(pdev, 0);
    clock_disable(rtc_clock);
    clock_put(rtc_clock);

err_clock:
    vmm_device_tree_regunmap_release(pdev->of_node, (virtual_addr_t)s3c_rtc_base, 0);

err_nomap:
    return ret;
}

static struct vmm_device_tree_nodeid s3c_devid_table[] = {
    {.compatible = "samsung,s3c2410-rtc", .data = (void *)TYPE_S3C2410},
    {.compatible = "samsung,s3c2416-rtc", .data = (void *)TYPE_S3C2416},
    {.compatible = "samsung,s3c2443-rtc", .data = (void *)TYPE_S3C2443},
    {.compatible = "samsung,s3c6410-rtc", .data = (void *)TYPE_S3C64XX},
    {/* end of list */},
};

static vmm_driver_t s3c_rtc_driver = {
    .name        = "s3c_rtc",
    .match_table = s3c_devid_table,
    .probe       = s3c_rtc_driver_probe,
    .remove      = s3c_rtc_driver_remove,
};

static int __init s3c_rtc_driver_init(void)
{
    return vmm_device_driver_register_driver(&s3c_rtc_driver);
}

static void __exit s3c_rtc_driver_exit(void)
{
    vmm_device_driver_unregister_driver(&s3c_rtc_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
