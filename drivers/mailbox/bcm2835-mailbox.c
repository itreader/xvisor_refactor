/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file bcm2835-mailbox.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief BCM2835 Mailbox controller driver.
 *
 * The source has been largely adapted from Linux
 * drivers/mailbox/bcm2835-mailbox.c
 *
 *  Copyright (C) 2010,2015 Broadcom
 *  Copyright (C) 2013-2014 Lubomir Rintel
 *  Copyright (C) 2013 Craig McGeachie
 *
 * This device provides a mechanism for writing to the mailboxes,
 * that are shared between the ARM and the VideoCore processor
 *
 * Parts of the driver are based on:
 *  - arch/arm/mach-bcm2708/vcio.c file written by Gray Girling that was
 *    obtained from branch "rpi-3.6.y" of git://github.com/raspberrypi/
 *    linux.git
 *  - drivers/mailbox/bcm2835-ipc.c by Lubomir Rintel at
 *    https://github.com/hackerspace/rpi-linux/blob/lr-raspberry-pi/drivers/
 *    mailbox/bcm2835-ipc.c
 *  - documentation available on the following web site:
 *    https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
 *
 * The original code is licensed under the GPL.
 */

#include <drv/mailbox_controller.h>
#include <vmm_device_driver.h>
#include <vmm_device_resource.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(dev, msg...) vmm_linfo(dev->name, msg)
#else
#define DPRINTF(dev, msg...)
#endif

#define MODULE_DESC           "BCM2835 Mailbox Driver"
#define MODULE_AUTHOR         "Anup Patel"
#define MODULE_LICENSE        "GPL"
#define MODULE_IPRIORITY      (0)
#define MODULE_INIT           bcm2835_mbox_init
#define MODULE_EXIT           bcm2835_mbox_exit

/* Mailboxes */
#define ARM_0_MAIL0           0x00
#define ARM_0_MAIL1           0x20

/*
 * Mailbox registers. We basically only support mailbox 0 & 1. We
 * deliver to the VC in mailbox 1, it delivers to us in mailbox 0. See
 * BCM2835-ARM-Peripherals.pdf section 1.3 for an explanation about
 * the placement of memory barriers.
 */
#define MAIL0_RD              (ARM_0_MAIL0 + 0x00)
#define MAIL0_POL             (ARM_0_MAIL0 + 0x10)
#define MAIL0_STA             (ARM_0_MAIL0 + 0x18)
#define MAIL0_CNF             (ARM_0_MAIL0 + 0x1C)
#define MAIL1_WRT             (ARM_0_MAIL1 + 0x00)
#define MAIL1_STA             (ARM_0_MAIL1 + 0x18)

/* Status register: FIFO state. */
#define ARM_MS_FULL           BIT(31)
#define ARM_MS_EMPTY          BIT(30)

/* Configuration register: Enable interrupts. */
#define ARM_MC_IHAVEDATAIRQEN BIT(0)

struct bcm2835_mbox {
    uint32_t               irq;
    void                  *regs;
    vmm_spinlock_t         lock;
    struct mbox_controller controller;
};

static struct bcm2835_mbox *bcm2835_link_mbox(struct mbox_chan *link)
{
    return container_of(link->mbox, struct bcm2835_mbox, controller);
}

static vmm_irq_return_t bcm2835_mbox_irq(int irq, void *dev_id)
{
    struct bcm2835_mbox *mbox = dev_id;
    struct mbox_chan    *link = &mbox->controller.chans[0];

    while (!(vmm_readl(mbox->regs + MAIL0_STA) & ARM_MS_EMPTY)) {
        uint32_t msg = vmm_readl(mbox->regs + MAIL0_RD);
        DPRINTF(mbox->controller.dev, "reply 0x%08X\n", msg);
        mbox_chan_received_data(link, &msg);
    }

    return VMM_IRQ_HANDLED;
}

static int bcm2835_send_data(struct mbox_chan *link, void *data)
{
    struct bcm2835_mbox *mbox = bcm2835_link_mbox(link);
    uint32_t             msg  = *(uint32_t *)data;

    vmm_spin_lock(&mbox->lock);
    vmm_writel(msg, mbox->regs + MAIL1_WRT);
    DPRINTF(mbox->controller.dev, "request 0x%08X\n", msg);
    vmm_spin_unlock(&mbox->lock);

    return 0;
}

static int bcm2835_startup(struct mbox_chan *link)
{
    struct bcm2835_mbox *mbox = bcm2835_link_mbox(link);

    /* Enable the interrupt on data reception */
    vmm_writel(ARM_MC_IHAVEDATAIRQEN, mbox->regs + MAIL0_CNF);

    return 0;
}

static void bcm2835_shutdown(struct mbox_chan *link)
{
    struct bcm2835_mbox *mbox = bcm2835_link_mbox(link);

    vmm_writel(0, mbox->regs + MAIL0_CNF);
}

static bool bcm2835_last_tx_done(struct mbox_chan *link)
{
    struct bcm2835_mbox *mbox = bcm2835_link_mbox(link);
    bool                 ret;

    vmm_spin_lock(&mbox->lock);
    ret = !(vmm_readl(mbox->regs + MAIL1_STA) & ARM_MS_FULL);
    vmm_spin_unlock(&mbox->lock);

    return ret;
}

static const struct mbox_chan_ops bcm2835_mbox_chan_ops = {
    .send_data = bcm2835_send_data, .startup = bcm2835_startup, .shutdown = bcm2835_shutdown, .last_tx_done = bcm2835_last_tx_done};

static struct mbox_chan *bcm2835_mbox_index_xlate(struct mbox_controller *mbox, const struct vmm_device_tree_phandle_args *sp)
{
    if (sp->args_count != 0) {
        return NULL;
    }

    return &mbox->chans[0];
}

static int bcm2835_mbox_probe(vmm_device_t *dev)
{
    int                  ret = 0;
    virtual_addr_t       base;
    struct bcm2835_mbox *mbox;

    mbox = vmm_devm_zalloc(dev, sizeof(*mbox));

    if (mbox == NULL) {
        return VMM_ERR_NOMEM;
    }

    INIT_SPIN_LOCK(&mbox->lock);

    mbox->irq = vmm_device_tree_irq_parse_map(dev->of_node, 0);

    if (!mbox->irq) {
        vmm_lerror(dev->name, "Failed to parse and map IRQ\n");
        return VMM_ERR_NODEV;
    }

    if ((ret = vmm_host_irq_register(mbox->irq, dev->name, bcm2835_mbox_irq, mbox))) {
        vmm_lerror(dev->name, "Failed to register mailbox IRQ handler: %d\n", ret);
        return ret;
    }

    ret = vmm_device_tree_request_regmap(dev->of_node, &base, 0, "BCM2835_MBOX");

    if (ret) {
        vmm_lerror(dev->name, "Failed to map mailbox regs: %d\n", ret);
        vmm_host_irq_unregister(mbox->irq, mbox);
        return ret;
    }

    mbox->regs                     = (void *)base;

    mbox->controller.txdone_poll   = TRUE;
    mbox->controller.txpoll_period = 5;
    mbox->controller.ops           = &bcm2835_mbox_chan_ops;
    mbox->controller.of_xlate      = &bcm2835_mbox_index_xlate;
    mbox->controller.dev           = dev;
    mbox->controller.num_chans     = 1;
    mbox->controller.chans         = vmm_devm_zalloc(dev, sizeof(*mbox->controller.chans));

    if (!mbox->controller.chans) {
        vmm_lerror(dev->name, "Failed to allocate mailbox channels\n");
        vmm_device_tree_regunmap_release(dev->of_node, (virtual_addr_t)mbox->regs, 0);
        vmm_host_irq_unregister(mbox->irq, mbox);
        return VMM_ERR_NOMEM;
    }

    ret = mbox_controller_register(&mbox->controller);

    if (ret) {
        vmm_lerror(dev->name, "Failed to register mailbox controller: %d\n", ret);
        vmm_device_tree_regunmap_release(dev->of_node, (virtual_addr_t)mbox->regs, 0);
        vmm_host_irq_unregister(mbox->irq, mbox);
        return ret;
    }

    vmm_device_driver_set_data(dev, mbox);
    vmm_linfo(dev->name, "mailbox enabled\n");

    return ret;
}

static int bcm2835_mbox_remove(vmm_device_t *dev)
{
    struct bcm2835_mbox *mbox = vmm_device_driver_get_data(dev);

    mbox_controller_unregister(&mbox->controller);
    vmm_device_tree_regunmap_release(dev->of_node, (virtual_addr_t)mbox->regs, 0);
    vmm_host_irq_unregister(mbox->irq, mbox);

    return 0;
}

static struct vmm_device_tree_nodeid bcm2835_mbox_devid_table[] = {
    {.compatible = "brcm,bcm2835-mbox"},
    {/* end of list */},
};

static vmm_driver_t bcm2835_mbox_driver = {
    .name        = "bcm2835-mbox",
    .match_table = bcm2835_mbox_devid_table,
    .probe       = bcm2835_mbox_probe,
    .remove      = bcm2835_mbox_remove,
};

static int __init bcm2835_mbox_init(void)
{
    return vmm_device_driver_register_driver(&bcm2835_mbox_driver);
}

static void __exit bcm2835_mbox_exit(void)
{
    vmm_device_driver_unregister_driver(&bcm2835_mbox_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
