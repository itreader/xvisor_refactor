/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file 8250-uart.c
 * @author Anup Patel (anup@brainfault.org)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief source file for UART serial port driver.
 */

#include <drv/clk.h>
#include <drv/serial.h>
#include <drv/serial/8250-uart.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_device_driver.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_modules.h>

#define MODULE_DESC      "8250 UART Driver"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (SERIAL_IPRIORITY + 1)
#define MODULE_INIT      uart_8250_driver_init
#define MODULE_EXIT      uart_8250_driver_exit

static uint8_t uart_8250_in(struct uart_8250_port *port, uint32_t offset)
{
    uint8_t ret;

    switch (port->reg_width) {
        case 4:
            ret = vmm_inl(port->base + (offset << port->reg_shift));
            break;

        case 2:
            ret = vmm_inw(port->base + (offset << port->reg_shift));
            break;

        default:
            ret = vmm_inb(port->base + (offset << port->reg_shift));
            break;
    };

    return ret;
}

static void uart_8250_out(struct uart_8250_port *port, uint32_t offset, uint8_t val)
{
    switch (port->reg_width) {
        case 4:
            vmm_outl(val, port->base + (offset << port->reg_shift));
            break;

        case 2:
            vmm_outw(val, port->base + (offset << port->reg_shift));
            break;

        default:
            vmm_outb(val, port->base + (offset << port->reg_shift));
            break;
    };

    if (offset == UART_LCR_OFFSET) {
        port->lcr_last = val;
    }
}

static void uart_8250_clear_errors(struct uart_8250_port *port)
{
    /* If there was a RX FIFO error (because of framing, parity,
     * break error) keep removing entries from RX FIFO until
     * LSR does not show this bit set
     */
    while (uart_8250_in(port, UART_LSR_OFFSET) & UART_LSR_BRK_ERROR_BITS) {
        uart_8250_in(port, UART_RBR_OFFSET);
    };
}

bool uart_8250_lowlevel_can_getc(struct uart_8250_port *port)
{
    if (uart_8250_in(port, UART_LSR_OFFSET) & UART_LSR_DR) {
        return TRUE;
    }

    return FALSE;
}

uint8_t uart_8250_lowlevel_getc(struct uart_8250_port *port)
{
    if (uart_8250_in(port, UART_LSR_OFFSET) & UART_LSR_DR) {
        return uart_8250_in(port, UART_RBR_OFFSET);
    }

    return 0;
}

bool uart_8250_lowlevel_can_putc(struct uart_8250_port *port)
{
    if (uart_8250_in(port, UART_LSR_OFFSET) & UART_LSR_THRE) {
        return TRUE;
    }

    return FALSE;
}

void uart_8250_lowlevel_putc(struct uart_8250_port *port, uint8_t ch)
{
    if (uart_8250_in(port, UART_LSR_OFFSET) & UART_LSR_THRE) {
        uart_8250_out(port, UART_THR_OFFSET, ch);
    }
}

void uart_8250_lowlevel_init(struct uart_8250_port *port)
{
    uint16_t bdiv;

    /* set DLAB bit */
    uart_8250_out(port, UART_LCR_OFFSET, 0x80);

    if (!port->skip_baudrate_config) {
        bdiv = udiv32(port->input_clock, (16 * port->baudrate));
        /* set baudrate divisor */
        uart_8250_out(port, UART_DLL_OFFSET, bdiv & 0xFF);
        /* set baudrate divisor */
        uart_8250_out(port, UART_DLM_OFFSET, (bdiv >> 8) & 0xFF);
    }

    /* clear DLAB; set 8 bits, no parity */
    uart_8250_out(port, UART_LCR_OFFSET, 0x03);
    /* enable FIFO */
    uart_8250_out(port, UART_FCR_OFFSET, 0x01);
    /* no modem control DTR RTS */
    uart_8250_out(port, UART_MCR_OFFSET, 0x00);
    /* clear line status */
    uart_8250_in(port, UART_LSR_OFFSET);
    /* read receive buffer */
    uart_8250_in(port, UART_RBR_OFFSET);
    /* set scratchpad */
    uart_8250_out(port, UART_SCR_OFFSET, 0x00);
    /* set interrupt enable reg */
    port->ier = 0x00;
    uart_8250_out(port, UART_IER_OFFSET, 0x00);
}

static vmm_irq_return_t uart_8250_irq_handler(int irq_no, void *dev)
{
    uint16_t               iir, lsr;
    struct uart_8250_port *port = (struct uart_8250_port *)dev;

    iir                         = uart_8250_in(port, UART_IIR_OFFSET);
    lsr                         = uart_8250_in(port, UART_LSR_OFFSET);

    switch (iir & 0xf) {
        case UART_IIR_NOINT:
            return VMM_IRQ_NONE;

        case UART_IIR_RLSI:
        case UART_IIR_RTO:
        case UART_IIR_RDI:
            if (lsr & UART_LSR_BRK_ERROR_BITS) {
                uart_8250_clear_errors(port);
            }

            if (lsr & UART_LSR_DR) {
                do {
                    uint8_t ch = uart_8250_in(port, UART_RBR_OFFSET);
                    serial_rx(port->p, &ch, 1);
                } while (uart_8250_in(port, UART_LSR_OFFSET) & (UART_LSR_DR | UART_LSR_OE));
            } else {
                while (1)
                    ;
            }

            break;

        case UART_IIR_BUSY:
            /* This is unallocated IIR value as per generic UART but is
             * used by Designware UARTs, we do not expect other UART IPs
             * to hit this case
             */
            uart_8250_in(port, 0x1f);
            uart_8250_out(port, UART_LCR_OFFSET, port->lcr_last);
            break;

        default:
            break;
    };

    return VMM_IRQ_HANDLED;
}

static uint32_t uart_8250_tx(struct serial *p, uint8_t *src, size_t len)
{
    uint32_t               i;
    struct uart_8250_port *port = serial_tx_private(p);

    for (i = 0; i < len; i++) {
        if (!uart_8250_lowlevel_can_putc(port)) {
            break;
        }

        uart_8250_lowlevel_putc(port, src[i]);
    }

    return i;
}

static int uart_8250_driver_probe(vmm_device_t *dev)
{
    int                    rc;
    struct uart_8250_port *port;
    physical_addr_t        ioport;
    const char            *aval;
    struct clk            *uartclk;

    port = vmm_zalloc(sizeof(struct uart_8250_port));

    if (!port) {
        rc = VMM_ERR_NOMEM;
        goto free_nothing;
    }

    if (vmm_device_tree_read_string(dev->of_node, VMM_DEVICE_TREE_ADDRESS_TYPE_ATTR_NAME, &aval)) {
        aval = NULL;
    }

    if (aval && !strcmp(aval, VMM_DEVICE_TREE_ADDRESS_TYPE_VAL_IO)) {
        port->use_ioport = TRUE;
    } else {
        port->use_ioport = FALSE;
    }

    if (port->use_ioport) {
        rc = vmm_device_tree_regaddr(dev->of_node, &ioport, 0);

        if (rc) {
            goto free_port;
        }

        port->base = ioport;
    } else {
        rc = vmm_device_tree_request_regmap(dev->of_node, &port->base, 0, "UART 8250");

        if (rc) {
            goto free_port;
        }
    }

    if (vmm_device_tree_read_u32(dev->of_node, "reg-shift", &port->reg_shift)) {
        port->reg_shift = 0;
    }

    if (vmm_device_tree_read_u32(dev->of_node, "reg-io-width", &port->reg_width)) {
        port->reg_width = 1;
    }

    if (vmm_device_tree_read_u32(dev->of_node, "baudrate", &port->baudrate)) {
        port->baudrate = 115200;
    }

    rc = vmm_device_tree_clock_frequency(dev->of_node, &port->input_clock);

    if (rc) {
        port->skip_baudrate_config = TRUE;
    } else {
        port->skip_baudrate_config = FALSE;
    }

    uartclk = devm_clock_get(dev, NULL);

    if (!VMM_IS_ERR_OR_NULL(uartclk)) {
        clock_prepare_enable(uartclk);
    }

    /* Call low-level init function
     * Note: low-level init will make sure that
     * interrupts are disabled in IER register.
     */
    uart_8250_lowlevel_init(port);

    /* Setup interrupt handler */
    port->irq = vmm_device_tree_irq_parse_map(dev->of_node, 0);

    if (!port->irq) {
        rc = VMM_ERR_NODEV;
        goto free_reg;
    }

    if ((rc = vmm_host_irq_register(port->irq, dev->name, uart_8250_irq_handler, port))) {
        goto free_reg;
    }

    /* Create Serial Port */
    port->p = serial_create(dev, 256, uart_8250_tx, port);

    if (VMM_IS_ERR_OR_NULL(port->p)) {
        rc = VMM_PTR_ERR(port->p);
        goto free_irq;
    }

    /* Save port pointer */
    dev->private = port;

    /* Unmask Rx interrupt */
    port->ier |= (UART_IER_RLSI | UART_IER_RDI);
    uart_8250_out(port, UART_IER_OFFSET, port->ier);

    return VMM_OK;

free_irq:
    vmm_host_irq_unregister(port->irq, port);
free_reg:

    if (!port->use_ioport) {
        vmm_device_tree_regunmap_release(dev->of_node, port->base, 0);
    }

free_port:
    vmm_free(port);
free_nothing:
    return rc;
}

static int uart_8250_driver_remove(vmm_device_t *dev)
{
    struct uart_8250_port *port = dev->private;

    if (!port) {
        return VMM_OK;
    }

    /* Mask Rx interrupt */
    port->ier &= ~(UART_IER_RLSI | UART_IER_RDI);
    uart_8250_out(port, UART_IER_OFFSET, port->ier);

    /* Free-up resources */
    serial_destroy(port->p);
    vmm_host_irq_unregister(port->irq, port);

    if (!port->use_ioport) {
        vmm_device_tree_regunmap_release(dev->of_node, port->base, 0);
    }

    vmm_free(port);
    dev->private = NULL;

    return VMM_OK;
}

static struct vmm_device_tree_nodeid uart_8250_devid_table[] = {
    {.compatible = "ns8250"},  {.compatible = "ns16450"}, {.compatible = "ns16550a"},         {.compatible = "ns16550"},
    {.compatible = "ns16750"}, {.compatible = "ns16850"}, {.compatible = "snps,dw-apb-uart"}, {/* end of list */},
};

static vmm_driver_t uart_8250_driver = {
    .name        = "uart_8250_serial",
    .match_table = uart_8250_devid_table,
    .probe       = uart_8250_driver_probe,
    .remove      = uart_8250_driver_remove,
};

static int __init uart_8250_driver_init(void)
{
    return vmm_device_driver_register_driver(&uart_8250_driver);
}

static void __exit uart_8250_driver_exit(void)
{
    vmm_device_driver_unregister_driver(&uart_8250_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
