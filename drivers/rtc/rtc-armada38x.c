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
 * @file rtc-armada38x.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief RTC driver for the Armada 38x Marvell SoCs
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/rtc/rtc-armada38x.c
 *
 * Copyright (C) 2015 Marvell
 *
 * Gregory Clement <gregory.clement@free-electrons.com>
 *
 * The original code is licensed under the GPL.
 */

#include <drv/rtc.h>
#include <libs/bitops.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_delay.h>
#include <vmm_device_driver.h>
#include <vmm_device_resource.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_modules.h>
#include <vmm_platform.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

#define MODULE_DESC               "ARMADA 38x RTC Driver"
#define MODULE_AUTHOR             "Anup Patel"
#define MODULE_LICENSE            "GPL"
#define MODULE_IPRIORITY          (RTC_DEVICE_CLASS_IPRIORITY + 1)
#define MODULE_INIT               armada38x_rtc_init
#define MODULE_EXIT               armada38x_rtc_exit

#define RTC_STATUS                0x0
#define RTC_STATUS_ALARM1         BIT(0)
#define RTC_STATUS_ALARM2         BIT(1)
#define RTC_IRQ1_CONF             0x4
#define RTC_IRQ2_CONF             0x8
#define RTC_IRQ_AL_EN             BIT(0)
#define RTC_IRQ_FREQ_EN           BIT(1)
#define RTC_IRQ_FREQ_1HZ          BIT(2)
#define RTC_CCR                   0x18
#define RTC_CCR_MODE              BIT(15)

#define RTC_TIME                  0xC
#define RTC_ALARM1                0x10
#define RTC_ALARM2                0x14

/* Armada38x SoC registers  */
#define RTC_38X_BRIDGE_TIMING_CTL 0x0
#define RTC_38X_PERIOD_OFFS       0
#define RTC_38X_PERIOD_MASK       (0x3FF << RTC_38X_PERIOD_OFFS)
#define RTC_38X_READ_DELAY_OFFS   26
#define RTC_38X_READ_DELAY_MASK   (0x1F << RTC_38X_READ_DELAY_OFFS)

/* Armada 7K/8K registers  */
#define RTC_8K_BRIDGE_TIMING_CTL0 0x0
#define RTC_8K_WRCLK_PERIOD_OFFS  0
#define RTC_8K_WRCLK_PERIOD_MASK  (0xFFFF << RTC_8K_WRCLK_PERIOD_OFFS)
#define RTC_8K_WRCLK_SETUP_OFFS   16
#define RTC_8K_WRCLK_SETUP_MASK   (0xFFFF << RTC_8K_WRCLK_SETUP_OFFS)
#define RTC_8K_BRIDGE_TIMING_CTL1 0x4
#define RTC_8K_READ_DELAY_OFFS    0
#define RTC_8K_READ_DELAY_MASK    (0xFFFF << RTC_8K_READ_DELAY_OFFS)

#define RTC_8K_ISR                0x10
#define RTC_8K_IMR                0x14
#define RTC_8K_ALARM2             BIT(0)

#define SOC_RTC_INTERRUPT         0x8
#define SOC_RTC_ALARM1            BIT(0)
#define SOC_RTC_ALARM2            BIT(1)
#define SOC_RTC_ALARM1_MASK       BIT(2)
#define SOC_RTC_ALARM2_MASK       BIT(3)

#define SAMPLE_NR                 100

struct value_to_freq {
    uint32_t value;
    uint8_t  freq;
};

struct armada38x_rtc {
    struct rtc_device         *rtc_dev;
    void                      *regs;
    void                      *regs_soc;
    vmm_spinlock_t             lock;
    int                        irq;
    struct value_to_freq      *val_to_freq;
    struct armada38x_rtc_data *data;
};

#define ALARM1                 0
#define ALARM2                 1

#define ALARM_REG(base, alarm) ((base) + (alarm) * sizeof(uint32_t))

struct armada38x_rtc_data {
    /* Initialize the RTC-MBUS bridge timing */
    void (*update_mbus_timing)(struct armada38x_rtc *rtc);
    uint32_t (*read_rtc_reg)(struct armada38x_rtc *rtc, uint8_t rtc_reg);
    void (*clear_isr)(struct armada38x_rtc *rtc);
    void (*unmask_interrupt)(struct armada38x_rtc *rtc);
    uint32_t alarm;
};

/*
 * According to the datasheet, the OS should wait 5us after every
 * register write to the RTC hard macro so that the required update
 * can occur without holding off the system bus
 * According to errata RES-3124064, Write to any RTC register
 * may fail. As a workaround, before writing to RTC
 * register, issue a dummy write of 0x0 twice to RTC Status
 * register.
 */

static void rtc_delayed_write(uint32_t val, struct armada38x_rtc *rtc, int offset)
{
    vmm_writel(0, rtc->regs + RTC_STATUS);
    vmm_writel(0, rtc->regs + RTC_STATUS);
    vmm_writel(val, rtc->regs + offset);
    vmm_udelay(5);
}

/* Update RTC-MBUS bridge timing parameters */
static void rtc_update_38x_mbus_timing_params(struct armada38x_rtc *rtc)
{
    uint32_t reg;

    reg = vmm_readl(rtc->regs_soc + RTC_38X_BRIDGE_TIMING_CTL);
    reg &= ~RTC_38X_PERIOD_MASK;
    reg |= 0x3FF << RTC_38X_PERIOD_OFFS;    /* Maximum value */
    reg &= ~RTC_38X_READ_DELAY_MASK;
    reg |= 0x1F << RTC_38X_READ_DELAY_OFFS; /* Maximum value */
    vmm_writel(reg, rtc->regs_soc + RTC_38X_BRIDGE_TIMING_CTL);
}

static void rtc_update_8k_mbus_timing_params(struct armada38x_rtc *rtc)
{
    uint32_t reg;

    reg = vmm_readl(rtc->regs_soc + RTC_8K_BRIDGE_TIMING_CTL0);
    reg &= ~RTC_8K_WRCLK_PERIOD_MASK;
    reg |= 0x3FF << RTC_8K_WRCLK_PERIOD_OFFS;
    reg &= ~RTC_8K_WRCLK_SETUP_MASK;
    reg |= 0x29 << RTC_8K_WRCLK_SETUP_OFFS;
    vmm_writel(reg, rtc->regs_soc + RTC_8K_BRIDGE_TIMING_CTL0);

    reg = vmm_readl(rtc->regs_soc + RTC_8K_BRIDGE_TIMING_CTL1);
    reg &= ~RTC_8K_READ_DELAY_MASK;
    reg |= 0x3F << RTC_8K_READ_DELAY_OFFS;
    vmm_writel(reg, rtc->regs_soc + RTC_8K_BRIDGE_TIMING_CTL1);
}

static uint32_t read_rtc_register(struct armada38x_rtc *rtc, uint8_t rtc_reg)
{
    return vmm_readl(rtc->regs + rtc_reg);
}

static uint32_t read_rtc_register_38x_wa(struct armada38x_rtc *rtc, uint8_t rtc_reg)
{
    int i, index_max = 0, max = 0;

    for (i = 0; i < SAMPLE_NR; i++) {
        rtc->val_to_freq[i].value = vmm_readl(rtc->regs + rtc_reg);
        rtc->val_to_freq[i].freq  = 0;
    }

    for (i = 0; i < SAMPLE_NR; i++) {
        int      j     = 0;
        uint32_t value = rtc->val_to_freq[i].value;

        while (rtc->val_to_freq[j].freq) {
            if (rtc->val_to_freq[j].value == value) {
                rtc->val_to_freq[j].freq++;
                break;
            }

            j++;
        }

        if (!rtc->val_to_freq[j].freq) {
            rtc->val_to_freq[j].value = value;
            rtc->val_to_freq[j].freq  = 1;
        }

        if (rtc->val_to_freq[j].freq > max) {
            index_max = j;
            max       = rtc->val_to_freq[j].freq;
        }

        /*
         * If a value already has half of the sample this is the most
         * frequent one and we can stop the research right now
         */
        if (max > SAMPLE_NR / 2) {
            break;
        }
    }

    return rtc->val_to_freq[index_max].value;
}

static void armada38x_clear_isr(struct armada38x_rtc *rtc)
{
    uint32_t val = vmm_readl(rtc->regs_soc + SOC_RTC_INTERRUPT);

    vmm_writel(val & ~SOC_RTC_ALARM1, rtc->regs_soc + SOC_RTC_INTERRUPT);
}

static void armada38x_unmask_interrupt(struct armada38x_rtc *rtc)
{
    uint32_t val = vmm_readl(rtc->regs_soc + SOC_RTC_INTERRUPT);

    vmm_writel(val | SOC_RTC_ALARM1_MASK, rtc->regs_soc + SOC_RTC_INTERRUPT);
}

static void armada8k_clear_isr(struct armada38x_rtc *rtc)
{
    vmm_writel(RTC_8K_ALARM2, rtc->regs_soc + RTC_8K_ISR);
}

static void armada8k_unmask_interrupt(struct armada38x_rtc *rtc)
{
    vmm_writel(RTC_8K_ALARM2, rtc->regs_soc + RTC_8K_IMR);
}

static int armada38x_rtc_read_time(struct rtc_device *dev, struct rtc_time *tm)
{
    struct armada38x_rtc *rtc = dev->private;
    uint64_t              time;
    irq_flags_t           flags;

    vmm_spin_lock_irq_save(&rtc->lock, flags);
    time = rtc->data->read_rtc_reg(rtc, RTC_TIME);
    vmm_spin_unlock_irq_restore(&rtc->lock, flags);

    rtc_time_to_tm(time, tm);

    return 0;
}

static int armada38x_rtc_set_time(struct rtc_device *dev, struct rtc_time *tm)
{
    struct armada38x_rtc *rtc = dev->private;
    int                   ret = 0;
    uint64_t              time;
    irq_flags_t           flags;

    ret = rtc_tm_to_time(tm, &time);

    if (ret) {
        goto out;
    }

    vmm_spin_lock_irq_save(&rtc->lock, flags);
    rtc_delayed_write(time, rtc, RTC_TIME);
    vmm_spin_unlock_irq_restore(&rtc->lock, flags);

out:
    return ret;
}

static int armada38x_rtc_read_alarm(struct rtc_device *dev, struct rtc_wkalrm *alrm)
{
    struct armada38x_rtc *rtc = dev->private;
    irq_flags_t           flags;
    uint64_t              time;
    uint32_t              reg     = ALARM_REG(RTC_ALARM1, rtc->data->alarm);
    uint32_t              reg_irq = ALARM_REG(RTC_IRQ1_CONF, rtc->data->alarm);
    uint32_t              val;

    vmm_spin_lock_irq_save(&rtc->lock, flags);

    time = rtc->data->read_rtc_reg(rtc, reg);
    val  = rtc->data->read_rtc_reg(rtc, reg_irq) & RTC_IRQ_AL_EN;

    vmm_spin_unlock_irq_restore(&rtc->lock, flags);

    alrm->enabled = val ? 1 : 0;
    rtc_time_to_tm(time, &alrm->time);

    return 0;
}

static int armada38x_rtc_set_alarm(struct rtc_device *dev, struct rtc_wkalrm *alrm)
{
    struct armada38x_rtc *rtc     = dev->private;
    uint32_t              reg     = ALARM_REG(RTC_ALARM1, rtc->data->alarm);
    uint32_t              reg_irq = ALARM_REG(RTC_IRQ1_CONF, rtc->data->alarm);
    irq_flags_t           flags;
    uint64_t              time;
    int                   ret = 0;

    ret                       = rtc_tm_to_time(&alrm->time, &time);

    if (ret) {
        goto out;
    }

    vmm_spin_lock_irq_save(&rtc->lock, flags);

    rtc_delayed_write(time, rtc, reg);

    if (alrm->enabled) {
        rtc_delayed_write(RTC_IRQ_AL_EN, rtc, reg_irq);
        rtc->data->unmask_interrupt(rtc);
    }

    vmm_spin_unlock_irq_restore(&rtc->lock, flags);

out:
    return ret;
}

static int armada38x_rtc_alarm_irq_enable(struct rtc_device *dev, uint32_t enabled)
{
    struct armada38x_rtc *rtc     = dev->private;
    uint32_t              reg_irq = ALARM_REG(RTC_IRQ1_CONF, rtc->data->alarm);
    irq_flags_t           flags;

    vmm_spin_lock_irq_save(&rtc->lock, flags);

    if (enabled) {
        rtc_delayed_write(RTC_IRQ_AL_EN, rtc, reg_irq);
    } else {
        rtc_delayed_write(0, rtc, reg_irq);
    }

    vmm_spin_unlock_irq_restore(&rtc->lock, flags);

    return 0;
}

static vmm_irq_return_t armada38x_rtc_alarm_irq(int irq, void *data)
{
    struct armada38x_rtc *rtc = data;
    uint32_t              val;
    int                   event   = RTC_IRQF | RTC_AF;
    uint32_t              reg_irq = ALARM_REG(RTC_IRQ1_CONF, rtc->data->alarm);

    vmm_spin_lock(&rtc->lock);

    rtc->data->clear_isr(rtc);
    val = rtc->data->read_rtc_reg(rtc, reg_irq);
    /* disable all the interrupts for alarm*/
    rtc_delayed_write(0, rtc, reg_irq);
    /* Ack the event */
    rtc_delayed_write(1 << rtc->data->alarm, rtc, RTC_STATUS);

    vmm_spin_unlock(&rtc->lock);

    if (val & RTC_IRQ_FREQ_EN) {
        if (val & RTC_IRQ_FREQ_1HZ) {
            event |= RTC_UF;
        } else {
            event |= RTC_PF;
        }
    }

    rtc_update_irq(rtc->rtc_dev, 1, event);

    return VMM_IRQ_HANDLED;
}

/*
 * The information given in the Armada 388 functional spec is complex.
 * They give two different formulas for calculating the offset value,
 * but when considering "Offset" as an 8-bit signed integer, they both
 * reduce down to (we shall rename "Offset" as "val" here):
 *
 *   val = (f_ideal / f_measured - 1) / resolution   where f_ideal = 32768
 *
 * Converting to time, f = 1/t:
 *   val = (t_measured / t_ideal - 1) / resolution   where t_ideal = 1/32768
 *
 *   =>  t_measured / t_ideal = val * resolution + 1
 *
 * "offset" in the RTC interface is defined as:
 *   t = t0 * (1 + offset * 1e-9)
 * where t is the desired period, t0 is the measured period with a zero
 * offset, which is t_measured above. With t0 = t_measured and t = t_ideal,
 *   offset = (t_ideal / t_measured - 1) / 1e-9
 *
 *   => t_ideal / t_measured = offset * 1e-9 + 1
 *
 * so:
 *
 *   offset * 1e-9 + 1 = 1 / (val * resolution + 1)
 *
 * We want "resolution" to be an integer, so resolution = R * 1e-9, giving
 *   offset = 1e18 / (val * R + 1e9) - 1e9
 *   val = (1e18 / (offset + 1e9) - 1e9) / R
 * with a common transformation:
 *   f(x) = 1e18 / (x + 1e9) - 1e9
 *   offset = f(val * R)
 *   val = f(offset) / R
 *
 * Armada 38x supports two modes, fine mode (954ppb) and coarse mode (3815ppb).
 */
static long armada38x_ppb_convert(long ppb)
{
    long div = ppb + 1000000000L;

    return sdiv64(1000000000000000000LL + div / 2, div) - 1000000000L;
}

static int armada38x_rtc_read_offset(struct rtc_device *dev, long *offset)
{
    struct armada38x_rtc *rtc = dev->private;
    uint64_t              ccr, flags;
    long                  ppb_cor;

    vmm_spin_lock_irq_save(&rtc->lock, flags);
    ccr = rtc->data->read_rtc_reg(rtc, RTC_CCR);
    vmm_spin_unlock_irq_restore(&rtc->lock, flags);

    ppb_cor = (ccr & RTC_CCR_MODE ? 3815 : 954) * (int8_t)ccr;
    /* ppb_cor + 1000000000L can never be zero */
    *offset = armada38x_ppb_convert(ppb_cor);

    return 0;
}

static int armada38x_rtc_set_offset(struct rtc_device *dev, long offset)
{
    struct armada38x_rtc *rtc = dev->private;
    uint64_t              ccr = 0;
    long                  ppb_cor, off;

    /*
     * The maximum ppb_cor is -128 * 3815 .. 127 * 3815, but we
     * need to clamp the input.  This equates to -484270 .. 488558.
     * Not only is this to stop out of range "off" but also to
     * avoid the division by zero in armada38x_ppb_convert().
     */
    offset  = clamp(offset, -484270L, 488558L);

    ppb_cor = armada38x_ppb_convert(offset);

    /*
     * Use low update mode where possible, which gives a better
     * resolution of correction.
     */
    off     = DIV_ROUND_CLOSEST(ppb_cor, 954);

    if (off > 127 || off < -128) {
        ccr = RTC_CCR_MODE;
        off = DIV_ROUND_CLOSEST(ppb_cor, 3815);
    }

    /*
     * Armada 388 requires a bit pattern in bits 14..8 depending on
     * the sign bit: { 0, ~S, S, S, S, S, S }
     */
    ccr |= (off & 0x3fff) ^ 0x2000;
    rtc_delayed_write(ccr, rtc, RTC_CCR);

    return 0;
}

static const struct rtc_class_ops armada38x_rtc_ops = {
    .read_time        = armada38x_rtc_read_time,
    .set_time         = armada38x_rtc_set_time,
    .read_alarm       = armada38x_rtc_read_alarm,
    .set_alarm        = armada38x_rtc_set_alarm,
    .alarm_irq_enable = armada38x_rtc_alarm_irq_enable,
    .read_offset      = armada38x_rtc_read_offset,
    .set_offset       = armada38x_rtc_set_offset,
};

static const struct rtc_class_ops armada38x_rtc_ops_noirq = {
    .read_time   = armada38x_rtc_read_time,
    .set_time    = armada38x_rtc_set_time,
    .read_alarm  = armada38x_rtc_read_alarm,
    .read_offset = armada38x_rtc_read_offset,
    .set_offset  = armada38x_rtc_set_offset,
};

static const struct armada38x_rtc_data armada38x_data = {
    .update_mbus_timing = rtc_update_38x_mbus_timing_params,
    .read_rtc_reg       = read_rtc_register_38x_wa,
    .clear_isr          = armada38x_clear_isr,
    .unmask_interrupt   = armada38x_unmask_interrupt,
    .alarm              = ALARM1,
};

static const struct armada38x_rtc_data armada8k_data = {
    .update_mbus_timing = rtc_update_8k_mbus_timing_params,
    .read_rtc_reg       = read_rtc_register,
    .clear_isr          = armada8k_clear_isr,
    .unmask_interrupt   = armada8k_unmask_interrupt,
    .alarm              = ALARM2,
};

static struct vmm_device_tree_nodeid armada38x_rtc_devid_table[] = {
    {
     .compatible = "marvell,armada-380-rtc",
     .data       = &armada38x_data,
     },
    {
     .compatible = "marvell,armada-8k-rtc",
     .data       = &armada8k_data,
     },
    {/* end of list */ },
};

static int armada38x_rtc_probe(vmm_device_t *dev)
{
    const struct rtc_class_ops          *ops;
    struct armada38x_rtc                *rtc;
    const struct vmm_device_tree_nodeid *match;
    virtual_addr_t                       va;
    int                                  ret;

    match = vmm_platform_match_nodeid(dev);

    if (!match) {
        return VMM_ERR_NODEV;
    }

    rtc = vmm_devm_zalloc(dev, sizeof(struct armada38x_rtc));

    if (!rtc) {
        return VMM_ERR_NOMEM;
    }

    rtc->val_to_freq = vmm_devm_calloc(dev, SAMPLE_NR, sizeof(struct value_to_freq));

    if (!rtc->val_to_freq) {
        return VMM_ERR_NOMEM;
    }

    INIT_SPIN_LOCK(&rtc->lock);

    ret = vmm_device_tree_regmap_byname(dev->of_node, &va, "rtc");

    if (ret) {
        vmm_lerror(dev->name, "Failed to map regs error %d\n", ret);
        return ret;
    }

    rtc->regs = (void *)va;
    ret       = vmm_device_tree_regmap_byname(dev->of_node, &va, "rtc-soc");

    if (ret) {
        vmm_lerror(dev->name, "Failed to map SOC regs error %d\n", ret);
        vmm_device_tree_regunmap_byname(dev->of_node, (virtual_addr_t)rtc->regs, "rtc");
        return ret;
    }

    rtc->regs_soc = (void *)va;

    rtc->irq      = vmm_device_tree_irq_parse_map(dev->of_node, 0);

    if (rtc->irq) {
        ret = vmm_host_irq_register(rtc->irq, dev->name, armada38x_rtc_alarm_irq, rtc);

        if (ret) {
            vmm_lerror(dev->name, "Failed to register IRQ error %d\n", ret);
            vmm_device_tree_regunmap_byname(dev->of_node, (virtual_addr_t)rtc->regs_soc, "rtc-soc");
            vmm_device_tree_regunmap_byname(dev->of_node, (virtual_addr_t)rtc->regs, "rtc");
            return ret;
        }
    }

    vmm_device_driver_set_data(dev, rtc);

    if (!rtc->irq) {
        ops = &armada38x_rtc_ops;
    } else {
        /*
         * If there is no interrupt available then we can't
         * use the alarm
         */
        ops = &armada38x_rtc_ops_noirq;
    }

    rtc->data = (struct armada38x_rtc_data *)match->data;

    /* Update RTC-MBUS bridge timing parameters */
    rtc->data->update_mbus_timing(rtc);

    rtc->rtc_dev = rtc_device_register(dev, dev->name, ops, rtc);

    if (VMM_IS_ERR(rtc->rtc_dev)) {
        ret = VMM_PTR_ERR(rtc->rtc_dev);
        vmm_lerror(dev->name, "Failed to register RTC error %d\n", ret);

        if (rtc->irq) {
            vmm_host_irq_unregister(rtc->irq, rtc);
        }

        vmm_device_tree_regunmap_byname(dev->of_node, (virtual_addr_t)rtc->regs_soc, "rtc-soc");
        vmm_device_tree_regunmap_byname(dev->of_node, (virtual_addr_t)rtc->regs, "rtc");
        return ret;
    }

    vmm_linfo(dev->name, "registered RTC device\n");

    return 0;
}

static int armada38x_rtc_remove(vmm_device_t *dev)
{
    struct armada38x_rtc *rtc = vmm_device_driver_get_data(dev);

    rtc_device_unregister(rtc->rtc_dev);

    if (rtc->irq) {
        vmm_host_irq_unregister(rtc->irq, rtc);
    }

    vmm_device_tree_regunmap_byname(dev->of_node, (virtual_addr_t)rtc->regs_soc, "rtc-soc");
    vmm_device_tree_regunmap_byname(dev->of_node, (virtual_addr_t)rtc->regs, "rtc");

    return VMM_OK;
}

static vmm_driver_t armada38x_rtc_driver = {
    .name        = "armada38x_rtc",
    .match_table = armada38x_rtc_devid_table,
    .probe       = armada38x_rtc_probe,
    .remove      = armada38x_rtc_remove,
};

static int __init armada38x_rtc_init(void)
{
    return vmm_device_driver_register_driver(&armada38x_rtc_driver);
}

static void __exit armada38x_rtc_exit(void)
{
    vmm_device_driver_unregister_driver(&armada38x_rtc_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
