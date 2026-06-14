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
 * @file generic_default_terminal.c
 * @author Anup Patel (anup@brainfault.org)
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief generic arch default terminal (default_terminal) functions using drivers
 */

#include <arch_default_terminal.h>
#include <generic_default_terminal.h>
#include <vmm_compiler.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_host_address_space.h>
#include <vmm_types.h>

static int unknown_default_terminal_putc(uint8_t ch)
{
    return VMM_ERR_FAIL;
}

static int unknown_default_terminal_getc(uint8_t *ch)
{
    return VMM_ERR_FAIL;
}

static int __init unknown_default_terminal_init(vmm_device_tree_node_t *node)
{
    return VMM_ERR_NODEV;
}

static struct default_terminal_ops unknown_ops = {
    .putc = unknown_default_terminal_putc, .getc = unknown_default_terminal_getc, .init = unknown_default_terminal_init};

#if defined(CONFIG_SERIAL_PL01X)

#include <drv/serial/pl011.h>

static virtual_addr_t pl011_default_terminal_base;
static bool           pl011_default_terminal_skip_baud_config;
static uint32_t       pl011_default_terminal_inclk;
static uint32_t       pl011_default_terminal_baud;

static int pl011_default_terminal_putc(uint8_t ch)
{
    if (!pl011_lowlevel_can_putc(pl011_default_terminal_base)) {
        return VMM_ERR_FAIL;
    }

    pl011_lowlevel_putc(pl011_default_terminal_base, ch);
    return VMM_OK;
}

static int pl011_default_terminal_getc(uint8_t *ch)
{
    if (!pl011_lowlevel_can_getc(pl011_default_terminal_base)) {
        return VMM_ERR_FAIL;
    }

    *ch = pl011_lowlevel_getc(pl011_default_terminal_base);
    return VMM_OK;
}

static int __init pl011_default_terminal_init(vmm_device_tree_node_t *node)
{
    int rc;

    rc = vmm_device_tree_regmap(node, &pl011_default_terminal_base, 0);

    if (rc) {
        return rc;
    }

    rc = vmm_device_tree_clock_frequency(node, &pl011_default_terminal_inclk);

    if (rc) {
        pl011_default_terminal_skip_baud_config = TRUE;
    } else {
        pl011_default_terminal_skip_baud_config = FALSE;
    }

    if (vmm_device_tree_read_u32(node, "baudrate", &pl011_default_terminal_baud)) {
        pl011_default_terminal_baud = 115200;
    }

    pl011_lowlevel_init(
        pl011_default_terminal_base, pl011_default_terminal_skip_baud_config, pl011_default_terminal_baud, pl011_default_terminal_inclk);

    return VMM_OK;
}

static struct default_terminal_ops pl011_ops = {
    .putc = pl011_default_terminal_putc, .getc = pl011_default_terminal_getc, .init = pl011_default_terminal_init};

#else

#define pl011_ops unknown_ops

#endif

#if defined(CONFIG_SERIAL_8250_UART)

#include <drv/serial/8250-uart.h>

static struct uart_8250_port uart8250_port;

static int uart8250_default_terminal_putc(uint8_t ch)
{
    if (!uart_8250_lowlevel_can_putc(&uart8250_port)) {
        return VMM_ERR_FAIL;
    }

    uart_8250_lowlevel_putc(&uart8250_port, ch);
    return VMM_OK;
}

static int uart8250_default_terminal_getc(uint8_t *ch)
{
    if (!uart_8250_lowlevel_can_getc(&uart8250_port)) {
        return VMM_ERR_FAIL;
    }

    *ch = uart_8250_lowlevel_getc(&uart8250_port);
    return VMM_OK;
}

static int __init uart8250_default_terminal_init(vmm_device_tree_node_t *node)
{
    int rc;

    rc = vmm_device_tree_regmap(node, &uart8250_port.base, 0);

    if (rc) {
        return rc;
    }

    rc = vmm_device_tree_clock_frequency(node, &uart8250_port.input_clock);

    if (rc) {
        uart8250_port.skip_baudrate_config = TRUE;
    } else {
        uart8250_port.skip_baudrate_config = FALSE;
    }

    if (vmm_device_tree_read_u32(node, "baudrate", &uart8250_port.baudrate)) {
        uart8250_port.baudrate = 115200;
    }

    if (vmm_device_tree_read_u32(node, "reg-shift", &uart8250_port.reg_shift)) {
        uart8250_port.reg_shift = 0;
    }

    if (vmm_device_tree_read_u32(node, "reg-io-width", &uart8250_port.reg_width)) {
        uart8250_port.reg_width = 1;
    }

    uart_8250_lowlevel_init(&uart8250_port);

    return VMM_OK;
}

static struct default_terminal_ops uart8250_ops = {
    .putc = uart8250_default_terminal_putc, .getc = uart8250_default_terminal_getc, .init = uart8250_default_terminal_init};

#else

#define uart8250_ops unknown_ops

#endif

#if defined(CONFIG_SERIAL_OMAP_UART)

#include <drv/serial/omap-uart.h>

static virtual_addr_t omap_default_terminal_base;
static bool           omap_default_terminal_skip_baud_config;
static uint32_t       omap_default_terminal_inclk;
static uint32_t       omap_default_terminal_baud;

static int omap_default_terminal_putc(uint8_t ch)
{
    if (!omap_uart_lowlevel_can_putc(omap_default_terminal_base, 2)) {
        return VMM_ERR_FAIL;
    }

    omap_uart_lowlevel_putc(omap_default_terminal_base, 2, ch);
    return VMM_OK;
}

static int omap_default_terminal_getc(uint8_t *ch)
{
    if (!omap_uart_lowlevel_can_getc(omap_default_terminal_base, 2)) {
        return VMM_ERR_FAIL;
    }

    *ch = omap_uart_lowlevel_getc(omap_default_terminal_base, 2);
    return VMM_OK;
}

static int __init omap_default_terminal_init(vmm_device_tree_node_t *node)
{
    int rc;

    rc = vmm_device_tree_regmap(node, &omap_default_terminal_base, 0);

    if (rc) {
        return rc;
    }

    rc = vmm_device_tree_clock_frequency(node, &omap_default_terminal_inclk);

    if (rc) {
        omap_default_terminal_skip_baud_config = TRUE;
    } else {
        omap_default_terminal_skip_baud_config = FALSE;
    }

    if (vmm_device_tree_read_u32(node, "baudrate", &omap_default_terminal_baud)) {
        omap_default_terminal_baud = 115200;
    }

    omap_uart_lowlevel_init(
        omap_default_terminal_base, 2, omap_default_terminal_skip_baud_config, omap_default_terminal_baud, omap_default_terminal_inclk);

    return VMM_OK;
}

static struct default_terminal_ops omapuart_ops = {
    .putc = omap_default_terminal_putc, .getc = omap_default_terminal_getc, .init = omap_default_terminal_init};

#else

#define omapuart_ops unknown_ops

#endif

#if defined(CONFIG_SERIAL_IMX)

#include <drv/serial/imx-uart.h>

static virtual_addr_t imx_default_terminal_base;
static bool           imx_default_terminal_skip_baudrate_config;
static uint32_t       imx_default_terminal_inclk;
static uint32_t       imx_default_terminal_baud;

static int imx_default_terminal_putc(uint8_t ch)
{
    if (!imx_lowlevel_can_putc(imx_default_terminal_base)) {
        return VMM_ERR_FAIL;
    }

    imx_lowlevel_putc(imx_default_terminal_base, ch);
    return VMM_OK;
}

static int imx_default_terminal_getc(uint8_t *ch)
{
    if (!imx_lowlevel_can_getc(imx_default_terminal_base)) {
        return VMM_ERR_FAIL;
    }

    *ch = imx_lowlevel_getc(imx_default_terminal_base);
    return VMM_OK;
}

static int __init imx_default_terminal_init(vmm_device_tree_node_t *node)
{
    int rc;

    rc = vmm_device_tree_regmap(node, &imx_default_terminal_base, 0);

    if (rc) {
        return rc;
    }

    rc = vmm_device_tree_clock_frequency(node, &imx_default_terminal_inclk);

    if (rc) {
        imx_default_terminal_skip_baudrate_config = TRUE;
    } else {
        imx_default_terminal_skip_baudrate_config = FALSE;
    }

    if (vmm_device_tree_read_u32(node, "baudrate", &imx_default_terminal_baud)) {
        imx_default_terminal_baud = 115200;
    }

    imx_lowlevel_init(imx_default_terminal_base, imx_default_terminal_skip_baudrate_config, imx_default_terminal_baud, imx_default_terminal_inclk);

    return VMM_OK;
}

static struct default_terminal_ops imx_ops = {
    .putc = imx_default_terminal_putc, .getc = imx_default_terminal_getc, .init = imx_default_terminal_init};

#else

#define imx_ops unknown_ops

#endif

#if defined(CONFIG_SERIAL_SAMSUNG)

#include <drv/serial/samsung-uart.h>

static virtual_addr_t samsung_default_terminal_base;
static bool           samsung_default_terminal_skip_baud_config;
static uint32_t       samsung_default_terminal_inclk;
static uint32_t       samsung_default_terminal_baud;

static int samsung_default_terminal_putc(uint8_t ch)
{
    if (!samsung_lowlevel_can_putc(samsung_default_terminal_base)) {
        return VMM_ERR_FAIL;
    }

    samsung_lowlevel_putc(samsung_default_terminal_base, ch);
    return VMM_OK;
}

static int samsung_default_terminal_getc(uint8_t *ch)
{
    if (!samsung_lowlevel_can_getc(samsung_default_terminal_base)) {
        return VMM_ERR_FAIL;
    }

    *ch = samsung_lowlevel_getc(samsung_default_terminal_base);
    return VMM_OK;
}

static int __init samsung_default_terminal_init(vmm_device_tree_node_t *node)
{
    int rc;

    /* map this console device */
    rc = vmm_device_tree_regmap(node, &samsung_default_terminal_base, 0);

    if (rc) {
        return rc;
    }

    /* retrieve clock frequency */
    rc = vmm_device_tree_clock_frequency(node, &samsung_default_terminal_inclk);

    if (rc) {
        samsung_default_terminal_skip_baud_config = TRUE;
    } else {
        samsung_default_terminal_skip_baud_config = FALSE;
    }

    /* retrieve baud rate */
    if (vmm_device_tree_read_u32(node, "baudrate", &samsung_default_terminal_baud)) {
        samsung_default_terminal_baud = 115200;
    }

    /* initialize the console port */
    samsung_lowlevel_init(
        samsung_default_terminal_base, samsung_default_terminal_skip_baud_config, samsung_default_terminal_baud, samsung_default_terminal_inclk);

    return VMM_OK;
}

static struct default_terminal_ops samsung_ops = {
    .putc = samsung_default_terminal_putc, .getc = samsung_default_terminal_getc, .init = samsung_default_terminal_init};

#else

#define samsung_ops unknown_ops

#endif

#if defined(CONFIG_SERIAL_SCIF)

#include <drv/serial/scif.h>

static virtual_addr_t scif_default_terminal_base;
static bool           scif_default_terminal_skip_baud_config;
static uint32_t       scif_default_terminal_inclk;
static uint32_t       scif_default_terminal_baud;
static uint64_t       scif_regtype = SCIx_SH4_SCIF_BRG_REGTYPE;
static bool           scif_default_terminal_use_intclk;

static int scif_default_terminal_putc(uint8_t ch)
{
    if (!scif_lowlevel_can_putc(scif_default_terminal_base, scif_regtype)) {
        return VMM_ERR_FAIL;
    }

    scif_lowlevel_putc(scif_default_terminal_base, scif_regtype, ch);
    return VMM_OK;
}

static int scif_default_terminal_getc(uint8_t *ch)
{
    if (!scif_lowlevel_can_getc(scif_default_terminal_base, scif_regtype)) {
        return VMM_ERR_FAIL;
    }

    *ch = scif_lowlevel_getc(scif_default_terminal_base, scif_regtype);
    return VMM_OK;
}

static int __init scif_default_terminal_init(vmm_device_tree_node_t *node)
{
    int rc;

    rc = vmm_device_tree_regmap(node, &scif_default_terminal_base, 0);

    if (rc) {
        return rc;
    }

    rc = vmm_device_tree_clock_frequency(node, &scif_default_terminal_inclk);

    if (rc) {
        scif_default_terminal_skip_baud_config = TRUE;
    } else {
        scif_default_terminal_skip_baud_config = FALSE;
    }

    if (vmm_device_tree_read_u32(node, "baudrate", &scif_default_terminal_baud)) {
        scif_default_terminal_baud = 115200;
    }

    if (vmm_device_tree_getattr(node, "clock-internal")) {
        scif_default_terminal_use_intclk = TRUE;
    } else {
        scif_default_terminal_use_intclk = FALSE;
    }

    scif_lowlevel_init(
        scif_default_terminal_base, scif_regtype, scif_default_terminal_skip_baud_config, scif_default_terminal_baud, scif_default_terminal_inclk,
        scif_default_terminal_use_intclk);

    return VMM_OK;
}

static int __init scifa_default_terminal_init(vmm_device_tree_node_t *node)
{
    scif_regtype = SCIx_SCIFA_REGTYPE;
    scif_default_terminal_init(node);

    return VMM_OK;
}

static struct default_terminal_ops scif_ops = {
    .putc = scif_default_terminal_putc, .getc = scif_default_terminal_getc, .init = scif_default_terminal_init};

static struct default_terminal_ops scifa_ops = {
    .putc = scif_default_terminal_putc, .getc = scif_default_terminal_getc, .init = scifa_default_terminal_init};

#else

#define scif_ops  unknown_ops
#define scifa_ops unknown_ops

#endif

#if defined(CONFIG_SERIAL_BCM283X_MU)

#include <drv/serial/bcm283x_mu.h>

static virtual_addr_t bcm283x_mu_default_terminal_base;
static uint32_t       bcm283x_mu_default_terminal_inclk;
static uint32_t       bcm283x_mu_default_terminal_baud;
static bool           bcm283x_mu_default_terminal_skip_baud_config;

static int bcm283x_mu_default_terminal_putc(uint8_t ch)
{
    if (!bcm283x_mu_lowlevel_can_putc(bcm283x_mu_default_terminal_base)) {
        return VMM_ERR_FAIL;
    }

    bcm283x_mu_lowlevel_putc(bcm283x_mu_default_terminal_base, ch);
    return VMM_OK;
}

static int bcm283x_mu_default_terminal_getc(uint8_t *ch)
{
    if (!bcm283x_mu_lowlevel_can_getc(bcm283x_mu_default_terminal_base)) {
        return VMM_ERR_FAIL;
    }

    *ch = bcm283x_mu_lowlevel_getc(bcm283x_mu_default_terminal_base);
    return VMM_OK;
}

static int __init bcm283x_mu_default_terminal_init(vmm_device_tree_node_t *node)
{
    int rc;

    rc = vmm_device_tree_regmap(node, &bcm283x_mu_default_terminal_base, 0);

    if (rc) {
        return rc;
    }

    rc = vmm_device_tree_clock_frequency(node, &bcm283x_mu_default_terminal_inclk);

    if (rc) {
        bcm283x_mu_default_terminal_skip_baud_config = TRUE;
    } else {
        bcm283x_mu_default_terminal_skip_baud_config = FALSE;
    }

    if (vmm_device_tree_read_u32(node, "baudrate", &bcm283x_mu_default_terminal_baud)) {
        bcm283x_mu_default_terminal_baud = 115200;
    }

    bcm283x_mu_lowlevel_init(
        bcm283x_mu_default_terminal_base, bcm283x_mu_default_terminal_skip_baud_config, bcm283x_mu_default_terminal_baud,
        bcm283x_mu_default_terminal_inclk);

    return VMM_OK;
}

static struct default_terminal_ops bcm283x_mu_ops = {
    .putc = bcm283x_mu_default_terminal_putc, .getc = bcm283x_mu_default_terminal_getc, .init = bcm283x_mu_default_terminal_init};

#else

#define bcm283x_mu_ops unknown_ops

#endif

#if defined(CONFIG_SERIAL_ZYNQ_UART)

#include <drv/serial/zynq-uart.h>

struct zynq_uart_private uart_port;

static int zynq_uart_default_terminal_getc(uint8_t *ch)
{
    if (!zynq_uart_lowlevel_can_getc(uart_port.regs)) {
        return VMM_ERR_FAIL;
    }

    *ch = zynq_uart_lowlevel_getc(uart_port.regs);
    return VMM_OK;
}

static int zynq_uart_default_terminal_putc(uint8_t ch)
{
    if (!zynq_uart_lowlevel_can_putc(uart_port.regs)) {
        return VMM_ERR_FAIL;
    }

    zynq_uart_lowlevel_putc(uart_port.regs, ch);
    return VMM_OK;
}

static int __init zynq_uart_default_terminal_init(vmm_device_tree_node_t *node)
{
    int rc;

    rc = vmm_device_tree_regmap(node, (virtual_addr_t *)&uart_port.regs, 0);

    if (rc) {
        return rc;
    }

    rc = vmm_device_tree_clock_frequency(node, &uart_port.input_clock);

    if (rc) {
        uart_port.skip_baudrate_config = TRUE;
    } else {
        uart_port.skip_baudrate_config = FALSE;
    }

    if (vmm_device_tree_read_u32(node, "baudrate", &uart_port.baudrate)) {
        uart_port.baudrate = 115200;
    }

    zynq_uart_lowlevel_init(&uart_port);

    return VMM_OK;
}

static struct default_terminal_ops zynq_uart_ops = {
    .putc = zynq_uart_default_terminal_putc, .getc = zynq_uart_default_terminal_getc, .init = zynq_uart_default_terminal_init};
#else

#define zynq_uart_ops unknown_ops

#endif

static struct vmm_device_tree_nodeid default_terminal_devid_table[] = {
    {.compatible = "arm,pl011", .data = &pl011_ops},
    {.compatible = "ns8250", .data = &uart8250_ops},
    {.compatible = "ns16450", .data = &uart8250_ops},
    {.compatible = "ns16550a", .data = &uart8250_ops},
    {.compatible = "ns16550", .data = &uart8250_ops},
    {.compatible = "ns16750", .data = &uart8250_ops},
    {.compatible = "ns16850", .data = &uart8250_ops},
    {.compatible = "snps,dw-apb-uart", .data = &uart8250_ops},
    {.compatible = "st16654", .data = &omapuart_ops},
    {.compatible = "freescale", .data = &imx_ops},
    {.compatible = "imx-uart", .data = &imx_ops},
    {.compatible = "freescale,imx-uart", .data = &imx_ops},
    {.compatible = "samsung", .data = &samsung_ops},
    {.compatible = "exynos4210-uart", .data = &samsung_ops},
    {.compatible = "samsung,exynos4210-uart", .data = &samsung_ops},
    {.compatible = "renesas,scif", .data = &scif_ops},
    {.compatible = "renesas,scifa", .data = &scifa_ops},
    {.compatible = "brcm,bcm283x-mu", .data = &bcm283x_mu_ops},
    {.compatible = "cdns,uart-r1p12", .data = &zynq_uart_ops},
    {.compatible = "xlnx,xuartps", .data = &zynq_uart_ops},
    {/* end of list */},
};

static const struct default_terminal_ops *ops = &unknown_ops;

void default_terminal_set_initial_ops(struct default_terminal_ops *initial_ops)
{
    if (initial_ops) {
        ops = initial_ops;
    }
}

int arch_default_terminal_putc(uint8_t ch)
{
    return ops->putc(ch);
}

int arch_default_terminal_getc(uint8_t *ch)
{
    return ops->getc(ch);
}

int __init arch_default_terminal_init(void)
{
    int                                  rc;
    const char                          *attr;
    vmm_device_tree_node_t              *node;
    const struct vmm_device_tree_nodeid *nodeid;

    /* Find chosen node */
    node = vmm_device_tree_getnode(VMM_DEVICE_TREE_PATH_SEPARATOR_STRING VMM_DEVICE_TREE_CHOSEN_NODE_NAME);

    if (!node || !vmm_device_tree_is_available(node)) {
        vmm_device_tree_dref_node(node);
        return VMM_ERR_NODEV;
    }

    /* Find console from chosen node */
    rc = vmm_device_tree_read_string(node, VMM_DEVICE_TREE_CONSOLE_ATTR_NAME, &attr);

    if (rc) {
        rc = vmm_device_tree_read_string(node, VMM_DEVICE_TREE_STDOUT_ATTR_NAME, &attr);
    }

    /* De-reference chosen node because we don't need it anymore. */
    vmm_device_tree_dref_node(node);

    /* If we did not find console */
    if (rc) {
        /* Use initial default_terminal ops */
        return ops->init(NULL);
    }

    /* Find console node */
    node = vmm_device_tree_getnode(attr);

    if (!node) {
        /* Use initial default_terminal ops */
        return ops->init(NULL);
    }

    /* Find matching default_terminal ops based on console node */
    nodeid = vmm_device_tree_match_node(default_terminal_devid_table, node);

    if (nodeid) {
        ops = nodeid->data;
    }

    /* Use matching console default_terminal ops */
    rc = ops->init(node);
    vmm_device_tree_dref_node(node);

    return rc;
}
