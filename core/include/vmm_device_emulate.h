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
 * @file vmm_device_emulate.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for device emulation framework
 */
#ifndef _VMM_DEVICE_EMULATE_H__
#define _VMM_DEVICE_EMULATE_H__

#include <vmm_device_tree.h>
#include <vmm_limits.h>
#include <vmm_manager.h>
#include <vmm_spinlocks.h>

struct vmm_emulate_device;
typedef struct vmm_emulate_device vmm_emulate_device_t;

enum vmm_device_emulate_endianness {
    VMM_DEVICE_EMULATE_UNKNOWN_ENDIAN = 0,
    VMM_DEVICE_EMULATE_NATIVE_ENDIAN  = 1,
    VMM_DEVICE_EMULATE_LITTLE_ENDIAN  = 2,
    VMM_DEVICE_EMULATE_BIG_ENDIAN     = 3,
    VMM_DEVICE_EMULATE_MAX_ENDIAN     = 4,
};

typedef struct vmm_emulator {
    double_list_t                        head;
    char                                 name[VMM_FIELD_NAME_SIZE];
    const struct vmm_device_tree_nodeid *match_table;
    enum vmm_device_emulate_endianness   endian;
    int (*probe)(struct vmm_guest *guest, vmm_emulate_device_t *edev, const struct vmm_device_tree_nodeid *nodeid);
    int (*remove)(vmm_emulate_device_t *edev);
    int (*reset)(vmm_emulate_device_t *edev);
    int (*sync)(vmm_emulate_device_t *edev, uint64_t val, void *v);
    int (*read8)(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t *dst);
    int (*write8)(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t src);
    int (*read16)(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t *dst);
    int (*write16)(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t src);
    int (*read32)(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst);
    int (*write32)(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src);
    int (*read64)(vmm_emulate_device_t *edev, physical_addr_t offset, uint64_t *dst);
    int (*write64)(vmm_emulate_device_t *edev, physical_addr_t offset, uint64_t src);
    int (*read_simple)(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst, uint32_t size);
    int (*write_simple)(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t regmask, uint32_t regval, uint32_t size);
} vmm_emulator_t;

int vmm_device_emulate_simple_read8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t *dst);
int vmm_device_emulate_simple_read16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t *dst);
int vmm_device_emulate_simple_read32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst);
int vmm_device_emulate_simple_write8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t src);
int vmm_device_emulate_simple_write16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t src);
int vmm_device_emulate_simple_write32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src);

#define VMM_DECLARE_EMULATOR_SIMPLE(EMU, NAME, MATCH, ENDIAN, PROBE, REMOVE, RESET, SYNC, READ, WRITE) \
    static vmm_emulator_t EMU = {                                                                      \
        .name         = NAME,                                                                          \
        .match_table  = MATCH,                                                                         \
        .endian       = ENDIAN,                                                                        \
        .probe        = PROBE,                                                                         \
        .reset        = RESET,                                                                         \
        .sync         = SYNC,                                                                          \
        .remove       = REMOVE,                                                                        \
        .read8        = vmm_device_emulate_simple_read8,                                               \
        .write8       = vmm_device_emulate_simple_write8,                                              \
        .read16       = vmm_device_emulate_simple_read16,                                              \
        .write16      = vmm_device_emulate_simple_write16,                                             \
        .read32       = vmm_device_emulate_simple_read32,                                              \
        .write32      = vmm_device_emulate_simple_write32,                                             \
        .read64       = NULL,                                                                          \
        .write64      = NULL,                                                                          \
        .read_simple  = READ,                                                                          \
        .write_simple = WRITE,                                                                         \
    }

struct vmm_emulate_device {
    vmm_spinlock_t             lock;
    vmm_device_tree_node_t    *node;
    struct vmm_region         *reg;
    vmm_emulator_t            *emu;
    struct vmm_emulate_device *parent;
    double_list_t              head;
    vmm_rwlock_t               child_list_lock;
    double_list_t              child_list;
    void *private;
#ifdef CONFIG_DEVICE_EMULATE_DEBUG
    uint32_t debug_info;
#endif
};

struct vmm_device_emulation_irqchip {
    const char *name;
    void (*handle)(uint32_t irq, int cpu, int level, void *opaque);
    void (*handle2)(uint32_t irq, int cpu, int level0, int level1, void *opaque);
    void (*map_host2guest)(uint32_t irq, uint32_t host_irq, void *opaque);
    void (*unmap_host2guest)(uint32_t irq, void *opaque);
    void (*notify_enabled)(uint32_t irq, int cpu, void *opaque);
    void (*notify_disabled)(uint32_t irq, int cpu, void *opaque);
};

typedef struct vmm_device_emulation_irqchip vmm_device_emulation_irqchip_t;

/** Emulate memory read to virtual device for given VCPU */
int vmm_device_emulate_emulate_read(
    vmm_vcpu_t *vcpu, physical_addr_t guest_physical_addr, void *dst, uint32_t dst_len, enum vmm_device_emulate_endianness dst_endian);

/** Emulate memory write to virtual device for given VCPU */
int vmm_device_emulate_emulate_write(
    vmm_vcpu_t *vcpu, physical_addr_t guest_physical_addr, void *src, uint32_t src_len, enum vmm_device_emulate_endianness src_endian);

/** Emulate IO read to virtual device for given VCPU */
int vmm_device_emulate_emulate_ioread(
    vmm_vcpu_t *vcpu, physical_addr_t guest_physical_addr, void *dst, uint32_t dst_len, enum vmm_device_emulate_endianness dst_endian);

/** Emulate IO write to virtual device for given VCPU */
int vmm_device_emulate_emulate_iowrite(
    vmm_vcpu_t *vcpu, physical_addr_t guest_physical_addr, void *src, uint32_t src_len, enum vmm_device_emulate_endianness src_endian);

/** Internal function to emulate irq (should not be called directly) */
extern int __vmm_device_emulate_emulate_irq(struct vmm_guest *guest, uint32_t irq, int cpu, int level);

/** Internal function to emulate irq (should not be called directly) */
extern int __vmm_device_emulate_emulate_irq2(struct vmm_guest *guest, uint32_t irq, int cpu, int level0, int level1);

/** Emulate single level change in shared irq for guest
 *  Note: This will only work after guest is created.
 */
#define vmm_device_emulate_emulate_irq(guest, irq, level)                        __vmm_device_emulate_emulate_irq(guest, irq, -1, level)

/** Emulate single level change in per_cpu irq for guest
 *  Note: This will only work after guest is created.
 */
#define vmm_device_emulate_emulate_per_cpu_irq(guest, irq, cpu, level)           __vmm_device_emulate_emulate_irq(guest, irq, cpu, level)

/** Emulate two level changes in shared irq for guest
 *  Note: This will only work after guest is created.
 */
#define vmm_device_emulate_emulate_irq2(guest, irq, level0, level1)              __vmm_device_emulate_emulate_irq2(guest, irq, -1, level0, level1)

/** Emulate two level changes in per_cpu irq for guest
 *  Note: This will only work after guest is created.
 */
#define vmm_device_emulate_emulate_per_cpu_irq2(guest, irq, cpu, level0, level1) __vmm_device_emulate_emulate_irq2(guest, irq, cpu, level0, level1)

/** Map host irq to guest irq for guest
 *  Note: This will only work after guest is created.
 */
int vmm_device_emulate_map_host2guest_irq(struct vmm_guest *guest, uint32_t irq, uint32_t host_irq);

/** Unmap host irq to guest irq mapping for guest
 *  Note: This will only work after guest is created.
 */
int vmm_device_emulate_unmap_host2guest_irq(struct vmm_guest *guest, uint32_t irq);

/** Notify guest irq is enabled for guest
 *  Note: This will only work after guest is created.
 */
int vmm_device_emulate_notify_irq_enabled(struct vmm_guest *guest, uint32_t irq, int cpu);

/** Notify guest irq is disabled for guest
 *  Note: This will only work after guest is created.
 */
int vmm_device_emulate_notify_irq_disabled(struct vmm_guest *guest, uint32_t irq, int cpu);

/** Register guest irqchip */
int vmm_device_emulate_register_irqchip(struct vmm_guest *guest, uint32_t irq, struct vmm_device_emulation_irqchip *chip, void *opaque);

/** Unregister guest irqchip */
int vmm_device_emulate_unregister_irqchip(struct vmm_guest *guest, uint32_t irq, struct vmm_device_emulation_irqchip *chip, void *opaque);

/** Count available irqs of a guest */
uint32_t vmm_device_emulate_count_irqs(struct vmm_guest *guest);

/** Register emulator */
int vmm_device_emulate_register_emulator(vmm_emulator_t *emu);

/** Unregister emulator */
int vmm_device_emulate_unregister_emulator(vmm_emulator_t *emu);

/** Find a registered emulator */
vmm_emulator_t *vmm_device_emulate_find_emulator(const char *name);

/** Get a registered emulator */
vmm_emulator_t *vmm_device_emulate_emulator(int index);

/** Count available emulators */
uint32_t vmm_device_emulate_emulator_count(void);

/** Sync children of given emulated device */
int vmm_device_emulate_sync_children(struct vmm_guest *guest, vmm_emulate_device_t *edev, uint64_t val, void *v);

/** Sync parent of given emulated device */
int vmm_device_emulate_sync_parent(struct vmm_guest *guest, vmm_emulate_device_t *edev, uint64_t val, void *v);

/** Reset context for given guest */
int vmm_device_emulate_reset_context(struct vmm_guest *guest);

/** Reset emulators for given region */
int vmm_device_emulate_reset_region(struct vmm_guest *guest, struct vmm_region *reg);

/** Remove emulator for given region */
int vmm_device_emulate_remove_region(struct vmm_guest *guest, struct vmm_region *reg);

/** Probe emulators for given region */
int vmm_device_emulate_probe_region(struct vmm_guest *guest, struct vmm_region *reg);

/** Initialize context for given guest */
int vmm_device_emulate_init_context(struct vmm_guest *guest);

/** DeInitialize context for given guest */
int vmm_device_emulate_deinit_context(struct vmm_guest *guest);

/** Initialize device emulation framework */
int vmm_device_emulate_init(void);

#endif
