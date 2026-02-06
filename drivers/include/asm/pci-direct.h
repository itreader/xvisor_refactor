/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file pci-direct.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 *
 * All the work under drivers/pci/ is a derived work from Linux's
 * PCI Framework. The following is the commit ID from which it has
 * been derived.
 *
 * commit 97bf6af1f928216fd6c5a66e8a57bfa95a659672
 * Linux 3.19-rc1
 */
#ifndef _ASM_X86_PCI_DIRECT_H
#define _ASM_X86_PCI_DIRECT_H

#include <linux/types.h>

/* Direct PCI access. This is used for PCI accesses in early boot before
   the PCI subsystem works. */

extern uint32_t read_pci_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
extern uint8_t  read_pci_config_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
extern uint16_t read_pci_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
extern void     write_pci_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val);
extern void     write_pci_config_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t val);
extern void     write_pci_config_16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val);

extern int early_pci_allowed(void);

extern uint32_t pci_early_dump_regs;
extern void     early_dump_pci_device(uint8_t bus, uint8_t slot, uint8_t func);
extern void     early_dump_pci_devices(void);
#endif /* _ASM_X86_PCI_DIRECT_H */
