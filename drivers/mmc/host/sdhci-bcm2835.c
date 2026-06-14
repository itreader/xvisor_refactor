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
 * @file sdhci-bcm2835.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Support for SDHCI device on BCM2835
 *
 * The source has been largely adapted from u-boot:
 * drivers/mmc/bcm2835_sdhci.c
 *
 * This u-boot code was extracted from:
 * git://github.com/gonzoua/u-boot-pi.git master
 * and hence presumably (C) 2012 Oleksandr Tymoshenko
 *
 * The original code is licensed under the GPL.
 */

#include <drv/clk.h>
#include <drv/mmc/sdhci.h>
#include <libs/mathlib.h>
#include <vmm_delay.h>
#include <vmm_device_driver.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_host_io.h>
#include <vmm_modules.h>

#define MODULE_DESC      "BCM2835 SDHCI Driver"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (SDHCI_IPRIORITY + 1)
#define MODULE_INIT      bcm2835_sdhci_driver_init
#define MODULE_EXIT      bcm2835_sdhci_driver_exit

/* 400KHz is max freq for card ID etc. Use that as min */
#define MIN_FREQ         400000

struct bcm2835_sdhci_host {
    struct clk    *clk;
    uint32_t       irq;
    uint32_t       clock_freq;
    virtual_addr_t base;
};

/*
 * The Arasan has a bugette whereby it may lose the content of successive
 * writes to registers that are within two SD-card clock cycles of each other
 * (a clock domain crossing problem). It seems, however, that the data
 * register does not have this problem, which is just as well - otherwise we'd
 * have to nobble the DMA engine too.
 *
 * This should probably be dynamically calculated based on the actual card
 * frequency. However, this is the longest we'll have to wait, and doesn't
 * seem to slow access down too much, so the added complexity doesn't seem
 * worth it for now.
 *
 * 1/MIN_FREQ is (max) time per tick of eMMC clock.
 * 2/MIN_FREQ is time for two ticks.
 * Multiply by 1000000 to get uS per two ticks.
 * *1000000 for uSecs.
 * +1 for hack rounding.
 */
#define BCM2835_SDHCI_WRITE_DELAY (((2 * 1000000) / MIN_FREQ) + 1)

static inline void bcm2835_sdhci_raw_writel(struct sdhci_host *host, uint32_t val, int reg)
{
    vmm_writel(val, host->ioaddr + reg);

    vmm_udelay(BCM2835_SDHCI_WRITE_DELAY);
}

static inline uint32_t bcm2835_sdhci_raw_readl(struct sdhci_host *host, int reg)
{
    return vmm_readl(host->ioaddr + reg);
}

static void bcm2835_sdhci_writel(struct sdhci_host *host, uint32_t val, int reg)
{
    bcm2835_sdhci_raw_writel(host, val, reg);
}

static void bcm2835_sdhci_writew(struct sdhci_host *host, uint16_t val, int reg)
{
    static uint32_t shadow;
    uint32_t        oldval     = (reg == SDHCI_COMMAND) ? shadow : bcm2835_sdhci_raw_readl(host, reg & ~3);
    uint32_t        word_num   = (reg >> 1) & 1;
    uint32_t        word_shift = word_num * 16;
    uint32_t        mask       = 0xffff << word_shift;
    uint32_t        newval     = (oldval & ~mask) | (val << word_shift);

    if (reg == SDHCI_TRANSFER_MODE) {
        shadow = newval;
    } else {
        bcm2835_sdhci_raw_writel(host, newval, reg & ~3);
    }
}

static void bcm2835_sdhci_writeb(struct sdhci_host *host, uint8_t val, int reg)
{
    uint32_t oldval     = bcm2835_sdhci_raw_readl(host, reg & ~3);
    uint32_t byte_num   = reg & 3;
    uint32_t byte_shift = byte_num * 8;
    uint32_t mask       = 0xff << byte_shift;
    uint32_t newval     = (oldval & ~mask) | (val << byte_shift);

    bcm2835_sdhci_raw_writel(host, newval, reg & ~3);
}

static uint32_t bcm2835_sdhci_readl(struct sdhci_host *host, int reg)
{
    return bcm2835_sdhci_raw_readl(host, reg);
}

static uint16_t bcm2835_sdhci_readw(struct sdhci_host *host, int reg)
{
    uint32_t val        = bcm2835_sdhci_raw_readl(host, (reg & ~3));
    uint32_t word_num   = (reg >> 1) & 1;
    uint32_t word_shift = word_num * 16;
    uint32_t word       = (val >> word_shift) & 0xffff;

    return word;
}

static uint8_t bcm2835_sdhci_readb(struct sdhci_host *host, int reg)
{
    uint32_t val        = bcm2835_sdhci_raw_readl(host, (reg & ~3));
    uint32_t byte_num   = reg & 3;
    uint32_t byte_shift = byte_num * 8;
    uint32_t byte       = (val >> byte_shift) & 0xff;

    return byte;
}

static int bcm2835_sdhci_driver_probe(vmm_device_t *dev)
{
    int                        rc;
    struct sdhci_host         *host;
    struct bcm2835_sdhci_host *bcm_host;

    host = sdhci_alloc_host(dev, sizeof(struct bcm2835_sdhci_host));

    if (!host) {
        rc = VMM_ERR_NOMEM;
        goto free_nothing;
    }

    bcm_host = sdhci_private(host);

    rc       = vmm_device_tree_request_regmap(dev->of_node, &bcm_host->base, 0, "BCM2835 SDHCI");

    if (rc) {
        goto free_host;
    }

    bcm_host->irq = vmm_device_tree_irq_parse_map(dev->of_node, 0);

    if (!bcm_host->irq) {
        rc = VMM_ERR_NODEV;
        goto free_reg;
    }

    bcm_host->clk = clock_get(dev, NULL);

    if (VMM_IS_ERR_OR_NULL(bcm_host->clk)) {
        rc = VMM_PTR_ERR(bcm_host->clk);
        goto free_reg;
    }

    bcm_host->clock_freq = clock_get_rate(bcm_host->clk);

    host->hw_name        = dev->name;
    host->irq            = (bcm_host->irq) ? bcm_host->irq : -1;
    host->ioaddr         = (void *)bcm_host->base;
    host->quirks         = SDHCI_QUIRK_BROKEN_VOLTAGE | SDHCI_QUIRK_BROKEN_CARD_DETECTION | SDHCI_QUIRK_BROKEN_R1B | SDHCI_QUIRK_WAIT_SEND_CMD |
                   SDHCI_QUIRK_NO_HISPD_BIT;
    host->voltages    = MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195;
    host->max_clock   = bcm_host->clock_freq;
    host->min_clock   = MIN_FREQ;

    host->ops.write_l = bcm2835_sdhci_writel;
    host->ops.write_w = bcm2835_sdhci_writew;
    host->ops.write_b = bcm2835_sdhci_writeb;
    host->ops.read_l  = bcm2835_sdhci_readl;
    host->ops.read_w  = bcm2835_sdhci_readw;
    host->ops.read_b  = bcm2835_sdhci_readb;

    rc                = sdhci_add_host(host);

    if (rc) {
        goto free_clock;
    }

    dev->private = host;

    return VMM_OK;

free_clock:
    clock_put(bcm_host->clk);
free_reg:
    vmm_device_tree_regunmap_release(dev->of_node, bcm_host->base, 0);
free_host:
    sdhci_free_host(host);
free_nothing:
    return rc;
}

static int bcm2835_sdhci_driver_remove(vmm_device_t *dev)
{
    struct sdhci_host         *host     = dev->private;
    struct bcm2835_sdhci_host *bcm_host = sdhci_private(host);

    if (host && bcm_host) {
        sdhci_remove_host(host, 1);

        vmm_device_tree_regunmap_release(dev->of_node, bcm_host->base, 0);

        sdhci_free_host(host);

        dev->private = NULL;
    }

    return VMM_OK;
}

static struct vmm_device_tree_nodeid bcm2835_sdhci_devid_table[] = {
    {.compatible = "brcm,bcm2835-sdhci"},
    {/* end of list */},
};

static vmm_driver_t bcm2835_sdhci_driver = {
    .name        = "bcm2835_sdhci",
    .match_table = bcm2835_sdhci_devid_table,
    .probe       = bcm2835_sdhci_driver_probe,
    .remove      = bcm2835_sdhci_driver_remove,
};

static int __init bcm2835_sdhci_driver_init(void)
{
    return vmm_device_driver_register_driver(&bcm2835_sdhci_driver);
}

static void __exit bcm2835_sdhci_driver_exit(void)
{
    vmm_device_driver_unregister_driver(&bcm2835_sdhci_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
