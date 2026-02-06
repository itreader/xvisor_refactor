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
 * @file pci_emu_core.h
 * @author Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief Interface for PCI emulation core.
 */
#ifndef __PCI_EMU_CORE_H
#define __PCI_EMU_CORE_H

#include <vmm_manager.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#define PCI_EMU_CORE_IPRIORITY              1

#define PCI_CONFIG_HEADER_END               0x3f

/* PCI HEADER_TYPE */
#define PCI_HEADER_TYPE_MULTI_FUNCTION      0x80

/* Size of the standard PCI config header */
#define PCI_CONFIG_HEADER_SIZE              0x40
/* Size of the standard PCI config space */
#define PCI_CONFIG_SPACE_SIZE               0x100
/* Size of the standard PCIe config space: 4KB */
#define PCIE_CONFIG_SPACE_SIZE              0x1000

#define PCI_CONFIG_VENDOR_ID_OFFS           0
#define PCI_CONFIG_DEVICE_ID_OFFS           2
#define PCI_CONFIG_COMMAND_REG_OFFS         4
#define PCI_CONFIG_STATUS_REG_OFFS          6
#define PCI_CONFIG_REVISION_ID_OFFS         8
#define PCI_CONFIG_CLASS_CODE_OFFS          9
#define PCI_CONFIG_SUBCLASS_CODE_OFFS       10
#define PCI_CONFIG_PROG_IF_OFFS             11
#define PCI_CONFIG_CACHE_LINE_OFFS          12
#define PCI_CONFIG_LATENCY_TMR_OFFS         13
#define PCI_CONFIG_HEADER_TYPE_OFFS         14
#define PCI_CONFIG_BIST_OFFS                15
#define PCI_CONFIG_BAR0_OFFS                16
#define PCI_CONFIG_BAR1_OFFS                20
#define PCI_CONFIG_BAR2_OFFS                24
#define PCI_CONFIG_BAR3_OFFS                28
#define PCI_CONFIG_BAR4_OFFS                32
#define PCI_CONFIG_BAR5_OFFS                36
#define PCI_CONFIG_CARD_BUS_PTR_OFFS        40
#define PCI_CONFIG_SUBSYS_VID               44
#define PCI_CONFIG_SUBSYS_DID               46
#define PCI_CONFIG_EROM_OFFS                48
#define PCI_CONFIG_CAP_PTR_OFFS             52
#define PCI_CONFIG_INT_LINE_OFFS            60
#define PCI_CONFIG_INT_PIN_OFFS             61
#define PCI_CONFIG_MIN_GNT_OFFS             62
#define PCI_CONFIG_MAX_LAT_OFFS             63

#define PCI_CONTROLLER_TO_CLASS(controller) (&(controller)->class)

#define PCI_DEVICE_TO_CLASS(pdev)           (&(pdev)->class)

struct pci_device;
struct pci_bus;
struct pci_host_controller;
struct pci_conf_header;
struct pci_class;

typedef uint32_t (*pci_config_read_t)(struct pci_class *pci_class, uint16_t reg_offset);
typedef int (*pci_config_write_t)(struct pci_class *pci_class, uint16_t reg_offset, uint32_t data);

struct pci_conf_header {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t  revision;
    uint8_t  prog_if;
    uint8_t  sub_class;
    uint8_t class;
    uint8_t  cache_line_sz;
    uint8_t  latency_timer;
    uint8_t  header_type;
    uint8_t  bist;
    uint32_t bars[6];
    uint32_t card_bus_ptr;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_device_id;
    uint32_t expansion_rom_base;
    uint8_t  cap_pointer;
    uint8_t  resv1;
    uint16_t resv2;
    uint32_t rev3;
    uint8_t  int_line;
    uint8_t  int_pin;
    uint8_t  min_gnt;
    uint8_t  max_lat;
} __packed;

struct pci_class {
    struct pci_conf_header conf_header;
    vmm_spinlock_t         lock;
    pci_config_read_t      config_read;
    pci_config_write_t     config_write;
};

struct pci_host_controller {
    struct pci_class class;
    uint8_t             name[VMM_FIELD_NAME_SIZE];
    uint32_t            nr_buses;
    uint32_t            bus_start;
    struct vmm_spinlock lock;
    double_list_t       attached_buses;
    double_list_t       head;
    struct vmm_guest   *guest;
};

struct pci_bus {
    double_list_t               head;
    uint16_t                    bus_id;
    struct vmm_spinlock         lock;
    struct pci_host_controller *host_controller;
    double_list_t               attached_devices;
};

struct pci_device {
    struct pci_class class;
    uint32_t                device_id; /* ID for responding to BDF */
    double_list_t           head;
    struct pci_bus         *pci_bus;
    struct vmm_guest       *guest;
    vmm_device_tree_node_t *node;
    struct vmm_spinlock     lock;
    void *private;
};

struct pci_dev_emulator {
    double_list_t                        head;
    char                                 name[VMM_FIELD_NAME_SIZE];
    const struct vmm_device_tree_nodeid *match_table;
    int (*probe)(struct pci_device *pdev, struct vmm_guest *guest, const struct vmm_device_tree_nodeid *nodeid);
    int (*reset)(struct pci_device *pdev);
    int (*remove)(struct pci_device *pdev);
};

int                      pci_emu_register_device(struct pci_dev_emulator *emu);
int                      pci_emu_unregister_device(struct pci_dev_emulator *emu);
struct pci_bus          *pci_find_bus_by_id(struct pci_host_controller *controller, uint32_t bus_id);
struct pci_dev_emulator *pci_emu_find_device(const char *name);
int                      pci_emu_find_pci_device(struct pci_host_controller *controller, int bus_id, int dev_id, struct pci_device **pdev);
struct pci_device       *pci_emu_pci_dev_find_by_addr(struct pci_host_controller *controller, uint32_t addr);
int                      pci_emu_probe_devices(struct vmm_guest *guest, struct pci_host_controller *controller, vmm_device_tree_node_t *node);
int                      pci_emu_register_controller(vmm_device_tree_node_t *node, struct vmm_guest *guest, struct pci_host_controller *controller);
int                      pci_emu_attach_new_pci_bus(struct pci_host_controller *controller, uint32_t bus_id);
int                      pci_emu_detach_pci_bus(struct pci_host_controller *controller, uint32_t bus_id);
int                      pci_emu_config_space_write(struct pci_class *class, uint32_t reg_offs, uint32_t val);
uint32_t                 pci_emu_config_space_read(struct pci_class *class, uint32_t reg_offs, uint32_t size);
int __init               pci_device_emulate_init(void);

#endif /* __PCI_EMU_CORE_H */
