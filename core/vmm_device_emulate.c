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
 * @file vmm_device_emulate.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for device emulation framework
 */

#include <libs/stringlib.h>
#include <vmm_device_emulate.h>
#include <vmm_device_emulate_debug.h>
#include <vmm_error.h>
#include <vmm_guest_address_space.h>
#include <vmm_heap.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_mutex.h>
#include <vmm_stdio.h>

struct vmm_device_emulate_guest_irq {
    double_list_t                        head;
    struct vmm_device_emulation_irqchip *chip;
    void                                *opaque;
};

struct vmm_device_emulate_guest_context {
    uint32_t       g_irq_count;
    double_list_t *g_irq;
};

struct vmm_device_emulate_ctrl {
    enum vmm_device_emulate_endianness host_endian;
    vmm_mutex_t                        emu_lock;
    double_list_t                      emu_list;
};

static struct vmm_device_emulate_ctrl dectrl;

static inline const char *get_guest_name(const vmm_emulate_device_t *edev)
{
    return edev->reg->aspace->guest->name;
}

/*
 * Debug interface
 */

static inline void debug_probe(const vmm_emulate_device_t *edev)
{
    if (vmm_device_emulate_debug_probe(edev)) {
        vmm_linfo(NULL, "[%s/%s] Probing device emulator\n", get_guest_name(edev), edev->node->name);
    }
}

static inline void debug_reset(const vmm_emulate_device_t *edev)
{
    if (vmm_device_emulate_debug_reset(edev)) {
        vmm_linfo(NULL, "[%s/%s] Resetting device emulator\n", get_guest_name(edev), edev->node->name);
    }
}

static inline void debug_sync(const vmm_emulate_device_t *edev)
{
    if (vmm_device_emulate_debug_sync(edev)) {
        vmm_linfo(NULL, "[%s/%s] Syncing device emulator\n", get_guest_name(edev), edev->node->name);
    }
}

static inline void debug_remove(const vmm_emulate_device_t *edev)
{
    if (vmm_device_emulate_debug_remove(edev)) {
        vmm_linfo(NULL, "[%s/%s] Removing device emulator\n", get_guest_name(edev), edev->node->name);
    }
}

static inline void debug_read(const vmm_emulate_device_t *edev, physical_addr_t offset, int bytes, uint64_t val)
{
    if (vmm_device_emulate_debug_read(edev)) {
        vmm_linfo(
            NULL,
            "[%s/%s] Reading %i bytes at "
            "0x%" PRIPADDR ": 0x%" PRIx64 "\n",
            get_guest_name(edev), edev->node->name, bytes, offset + edev->reg->guest_physical_addr, val);
    }
}

static inline void debug_write(const vmm_emulate_device_t *edev, physical_addr_t offset, int bytes, uint64_t val)
{
    if (vmm_device_emulate_debug_write(edev)) {
        vmm_linfo(
            NULL,
            "[%s/%s] Wrote %i bytes at "
            "0x%" PRIPADDR ": 0x%" PRIx64 "\n",
            get_guest_name(edev), edev->node->name, bytes, offset + edev->reg->guest_physical_addr, val);
    }
}

static int devemu_doread(
    vmm_emulate_device_t *edev, physical_addr_t offset, void *dst, uint32_t dst_len, enum vmm_device_emulate_endianness dst_endian)
{
    int                                rc;
    uint16_t                           data16;
    uint32_t                           data32;
    uint64_t                           data64;
    enum vmm_device_emulate_endianness data_endian;

    if (!edev || (dst_endian <= VMM_DEVICE_EMULATE_UNKNOWN_ENDIAN) || (VMM_DEVICE_EMULATE_MAX_ENDIAN <= dst_endian)) {
        return VMM_EFAIL;
    }

    switch (dst_len) {
        case 1:
            if (edev->emu->read8) {
                rc = edev->emu->read8(edev, offset, dst);
                debug_read(edev, offset, sizeof(uint8_t), *((uint8_t *)dst));
            } else {
                vmm_printf("%s: edev=%s does not have read8()\n", __func__, edev->node->name);
                rc = VMM_ENOTAVAIL;
            }

            break;

        case 2:
            if (edev->emu->read16) {
                rc = edev->emu->read16(edev, offset, &data16);
                debug_read(edev, offset, sizeof(uint16_t), data16);
            } else {
                vmm_printf("%s: edev=%s does not have read16()\n", __func__, edev->node->name);
                rc = VMM_ENOTAVAIL;
            }

            if (!rc) {
                switch (edev->emu->endian) {
                    case VMM_DEVICE_EMULATE_LITTLE_ENDIAN:
                        data16      = vmm_cpu_to_le16(data16);
                        data_endian = VMM_DEVICE_EMULATE_LITTLE_ENDIAN;
                        break;

                    case VMM_DEVICE_EMULATE_BIG_ENDIAN:
                        data16      = vmm_cpu_to_be16(data16);
                        data_endian = VMM_DEVICE_EMULATE_BIG_ENDIAN;
                        break;

                    default:
                        data_endian = VMM_DEVICE_EMULATE_NATIVE_ENDIAN;
                        break;
                };

                if (data_endian != dst_endian) {
                    switch (dst_endian) {
                        case VMM_DEVICE_EMULATE_LITTLE_ENDIAN:
                            data16 = vmm_cpu_to_le16(data16);
                            break;

                        case VMM_DEVICE_EMULATE_BIG_ENDIAN:
                            data16 = vmm_cpu_to_be16(data16);
                            break;

                        default:
                            break;
                    };
                }

                *(uint16_t *)dst = data16;
            }

            break;

        case 4:
            if (edev->emu->read32) {
                rc = edev->emu->read32(edev, offset, &data32);
                debug_read(edev, offset, sizeof(uint32_t), data32);
            } else {
                vmm_printf("%s: edev=%s does not have read32()\n", __func__, edev->node->name);
                rc = VMM_ENOTAVAIL;
            }

            if (!rc) {
                switch (edev->emu->endian) {
                    case VMM_DEVICE_EMULATE_LITTLE_ENDIAN:
                        data32      = vmm_cpu_to_le32(data32);
                        data_endian = VMM_DEVICE_EMULATE_LITTLE_ENDIAN;
                        break;

                    case VMM_DEVICE_EMULATE_BIG_ENDIAN:
                        data32      = vmm_cpu_to_be32(data32);
                        data_endian = VMM_DEVICE_EMULATE_BIG_ENDIAN;
                        break;

                    default:
                        data_endian = VMM_DEVICE_EMULATE_NATIVE_ENDIAN;
                        break;
                };

                if (data_endian != dst_endian) {
                    switch (dst_endian) {
                        case VMM_DEVICE_EMULATE_LITTLE_ENDIAN:
                            data32 = vmm_cpu_to_le32(data32);
                            break;

                        case VMM_DEVICE_EMULATE_BIG_ENDIAN:
                            data32 = vmm_cpu_to_be32(data32);
                            break;

                        default:
                            break;
                    };
                }

                *(uint32_t *)dst = data32;
            }

            break;

        case 8:
            if (edev->emu->read64) {
                rc = edev->emu->read64(edev, offset, &data64);
                debug_read(edev, offset, sizeof(uint64_t), data64);
            } else {
                vmm_printf("%s: edev=%s does not have read64()\n", __func__, edev->node->name);
                rc = VMM_ENOTAVAIL;
            }

            if (!rc) {
                switch (edev->emu->endian) {
                    case VMM_DEVICE_EMULATE_LITTLE_ENDIAN:
                        data64      = vmm_cpu_to_le64(data64);
                        data_endian = VMM_DEVICE_EMULATE_LITTLE_ENDIAN;
                        break;

                    case VMM_DEVICE_EMULATE_BIG_ENDIAN:
                        data64      = vmm_cpu_to_be64(data64);
                        data_endian = VMM_DEVICE_EMULATE_BIG_ENDIAN;
                        break;

                    default:
                        data_endian = VMM_DEVICE_EMULATE_NATIVE_ENDIAN;
                        break;
                };

                if (data_endian != dst_endian) {
                    switch (dst_endian) {
                        case VMM_DEVICE_EMULATE_LITTLE_ENDIAN:
                            data64 = vmm_cpu_to_le64(data64);
                            break;

                        case VMM_DEVICE_EMULATE_BIG_ENDIAN:
                            data64 = vmm_cpu_to_be32(data64);
                            break;

                        default:
                            break;
                    };
                }

                *(uint64_t *)dst = data64;
            }

            break;

        default:
            vmm_printf("%s: edev=%s invalid len=%d\n", __func__, edev->node->name, dst_len);
            rc = VMM_EINVALID;
            break;
    };

    if (rc) {
        vmm_printf(
            "%s: edev=%s offset=0x%" PRIPADDR " dst_len=%d "
            "failed (error %d)\n",
            __func__, edev->node->name, offset, dst_len, rc);
    }

    return rc;
}

static int devemu_dowrite(
    vmm_emulate_device_t *edev, physical_addr_t offset, void *src, uint32_t src_len, enum vmm_device_emulate_endianness src_endian)
{
    int      rc;
    uint16_t data16;
    uint32_t data32;
    uint64_t data64;

    if (!edev || (src_endian <= VMM_DEVICE_EMULATE_UNKNOWN_ENDIAN) || (VMM_DEVICE_EMULATE_MAX_ENDIAN <= src_endian)) {
        return VMM_EFAIL;
    }

    switch (src_len) {
        case 1:
            if (edev->emu->write8) {
                rc = edev->emu->write8(edev, offset, *((uint8_t *)src));
                debug_write(edev, offset, sizeof(uint8_t), *((uint8_t *)src));
            } else {
                vmm_printf("%s: edev=%s does not have write8()\n", __func__, edev->node->name);
                rc = VMM_ENOTAVAIL;
            }

            break;

        case 2:
            data16 = *(uint16_t *)src;

            switch (src_endian) {
                case VMM_DEVICE_EMULATE_LITTLE_ENDIAN:
                    data16 = vmm_le16_to_cpu(data16);
                    break;

                case VMM_DEVICE_EMULATE_BIG_ENDIAN:
                    data16 = vmm_be16_to_cpu(data16);
                    break;

                default:
                    break;
            };

            switch (edev->emu->endian) {
                case VMM_DEVICE_EMULATE_LITTLE_ENDIAN:
                    data16 = vmm_cpu_to_le16(data16);
                    break;

                case VMM_DEVICE_EMULATE_BIG_ENDIAN:
                    data16 = vmm_cpu_to_be16(data16);
                    break;

                default:
                    break;
            };

            if (edev->emu->write16) {
                rc = edev->emu->write16(edev, offset, data16);
                debug_write(edev, offset, sizeof(uint16_t), data16);
            } else {
                vmm_printf("%s: edev=%s does not have write16()\n", __func__, edev->node->name);
                rc = VMM_ENOTAVAIL;
            }

            break;

        case 4:
            data32 = *(uint32_t *)src;

            switch (src_endian) {
                case VMM_DEVICE_EMULATE_LITTLE_ENDIAN:
                    data32 = vmm_le32_to_cpu(data32);
                    break;

                case VMM_DEVICE_EMULATE_BIG_ENDIAN:
                    data32 = vmm_be32_to_cpu(data32);
                    break;

                default:
                    break;
            };

            switch (edev->emu->endian) {
                case VMM_DEVICE_EMULATE_LITTLE_ENDIAN:
                    data32 = vmm_cpu_to_le32(data32);
                    break;

                case VMM_DEVICE_EMULATE_BIG_ENDIAN:
                    data32 = vmm_cpu_to_be32(data32);
                    break;

                default:
                    break;
            };

            if (edev->emu->write32) {
                rc = edev->emu->write32(edev, offset, data32);
                debug_write(edev, offset, sizeof(uint32_t), data32);
            } else {
                vmm_printf("%s: edev=%s does not have write32()\n", __func__, edev->node->name);
                rc = VMM_ENOTAVAIL;
            }

            break;

        case 8:
            data64 = *(uint64_t *)src;

            switch (src_endian) {
                case VMM_DEVICE_EMULATE_LITTLE_ENDIAN:
                    data64 = vmm_le64_to_cpu(data64);
                    break;

                case VMM_DEVICE_EMULATE_BIG_ENDIAN:
                    data64 = vmm_be64_to_cpu(data64);
                    break;

                default:
                    break;
            };

            switch (edev->emu->endian) {
                case VMM_DEVICE_EMULATE_LITTLE_ENDIAN:
                    data64 = vmm_cpu_to_le64(data64);
                    break;

                case VMM_DEVICE_EMULATE_BIG_ENDIAN:
                    data64 = vmm_cpu_to_be64(data64);
                    break;

                default:
                    break;
            };

            if (edev->emu->write64) {
                rc = edev->emu->write64(edev, offset, data64);
                debug_write(edev, offset, sizeof(uint64_t), data64);
            } else {
                vmm_printf("%s: edev=%s does not have write64()\n", __func__, edev->node->name);
                rc = VMM_ENOTAVAIL;
            }

            break;

        default:
            vmm_printf("%s: edev=%s invalid len=%d\n", __func__, edev->node->name, src_len);
            rc = VMM_EINVALID;
            break;
    };

    if (rc) {
        vmm_printf(
            "%s: edev=%s offset=0x%" PRIPADDR " src_len=%d "
            "failed (error %d)\n",
            __func__, edev->node->name, offset, src_len, rc);
    }

    return rc;
}

int vmm_device_emulate_emulate_read(
    vmm_vcpu_t *vcpu, physical_addr_t guest_physical_addr, void *dst, uint32_t dst_len, enum vmm_device_emulate_endianness dst_endian)
{
    int                rc;
    struct vmm_region *reg;

    if (!vcpu || !vcpu->guest) {
        return VMM_EFAIL;
    }

    reg = vmm_guest_find_region(vcpu->guest, guest_physical_addr, VMM_REGION_VIRTUAL | VMM_REGION_MEMORY, FALSE);

    if (!reg) {
        rc = VMM_ENOTAVAIL;
        goto skip;
    }

    rc = devemu_doread(reg->device_emulate_private, guest_physical_addr - reg->guest_physical_addr, dst, dst_len, dst_endian);
skip:

    if (rc) {
        vmm_printf(
            "%s: vcpu=%s gphys=0x%" PRIPADDR " dst_len=%d "
            "failed (error %d)\n",
            __func__, vcpu->name, guest_physical_addr, dst_len, rc);
        vmm_manager_vcpu_halt(vcpu);
    }

    return rc;
}

int vmm_device_emulate_emulate_write(
    vmm_vcpu_t *vcpu, physical_addr_t guest_physical_addr, void *src, uint32_t src_len, enum vmm_device_emulate_endianness src_endian)
{
    int                rc;
    struct vmm_region *reg;

    if (!vcpu || !vcpu->guest) {
        return VMM_EFAIL;
    }

    reg = vmm_guest_find_region(vcpu->guest, guest_physical_addr, VMM_REGION_VIRTUAL | VMM_REGION_MEMORY, FALSE);

    if (!reg) {
        rc = VMM_ENOTAVAIL;
        goto skip;
    }

    rc = devemu_dowrite(reg->device_emulate_private, guest_physical_addr - reg->guest_physical_addr, src, src_len, src_endian);
skip:

    if (rc) {
        vmm_printf(
            "%s: vcpu=%s gphys=0x%" PRIPADDR " src_len=%d "
            "failed (error %d)\n",
            __func__, vcpu->name, guest_physical_addr, src_len, rc);
        vmm_manager_vcpu_halt(vcpu);
    }

    return rc;
}

int vmm_device_emulate_emulate_ioread(
    vmm_vcpu_t *vcpu, physical_addr_t guest_physical_addr, void *dst, uint32_t dst_len, enum vmm_device_emulate_endianness dst_endian)
{
    int                rc;
    struct vmm_region *reg;

    if (!vcpu || !vcpu->guest) {
        return VMM_EFAIL;
    }

    reg = vmm_guest_find_region(vcpu->guest, guest_physical_addr, VMM_REGION_VIRTUAL | VMM_REGION_IO, FALSE);

    if (!reg) {
        rc = VMM_ENOTAVAIL;
        goto skip;
    }

    rc = devemu_doread(reg->device_emulate_private, guest_physical_addr - reg->guest_physical_addr, dst, dst_len, dst_endian);
skip:

    if (rc) {
        vmm_printf(
            "%s: vcpu=%s gphys=0x%" PRIPADDR " dst_len=%d "
            "failed (error %d)\n",
            __func__, vcpu->name, guest_physical_addr, dst_len, rc);
        vmm_manager_vcpu_halt(vcpu);
    }

    return rc;
}

int vmm_device_emulate_emulate_iowrite(
    vmm_vcpu_t *vcpu, physical_addr_t guest_physical_addr, void *src, uint32_t src_len, enum vmm_device_emulate_endianness src_endian)
{
    int                rc;
    struct vmm_region *reg;

    if (!vcpu || !vcpu->guest) {
        return VMM_EFAIL;
    }

    reg = vmm_guest_find_region(vcpu->guest, guest_physical_addr, VMM_REGION_VIRTUAL | VMM_REGION_IO, FALSE);

    if (!reg) {
        rc = VMM_ENOTAVAIL;
        goto skip;
    }

    rc = devemu_dowrite(reg->device_emulate_private, guest_physical_addr - reg->guest_physical_addr, src, src_len, src_endian);
skip:

    if (rc) {
        vmm_printf(
            "%s: vcpu=%s gphys=0x%" PRIPADDR " src_len=%d "
            "failed (error %d)\n",
            __func__, vcpu->name, guest_physical_addr, src_len, rc);
        vmm_manager_vcpu_halt(vcpu);
    }

    return rc;
}

int __vmm_device_emulate_emulate_irq(struct vmm_guest *guest, uint32_t irq, int cpu, int level)
{
    struct vmm_device_emulate_guest_irq     *gi;
    struct vmm_device_emulate_guest_context *eg;

    if (!guest) {
        return VMM_EFAIL;
    }

    eg = (struct vmm_device_emulate_guest_context *)guest->aspace.device_emulate_private;

    if (eg->g_irq_count <= irq) {
        return VMM_EINVALID;
    }

    list_for_each_entry(gi, &eg->g_irq[irq], head)
    {
        if (!gi->chip->handle) {
            continue;
        }

        gi->chip->handle(irq, cpu, level, gi->opaque);
    }

    return VMM_OK;
}

int __vmm_device_emulate_emulate_irq2(struct vmm_guest *guest, uint32_t irq, int cpu, int level0, int level1)
{
    struct vmm_device_emulate_guest_irq     *gi;
    struct vmm_device_emulate_guest_context *eg;

    if (!guest) {
        return VMM_EFAIL;
    }

    eg = (struct vmm_device_emulate_guest_context *)guest->aspace.device_emulate_private;

    if (eg->g_irq_count <= irq) {
        return VMM_EINVALID;
    }

    list_for_each_entry(gi, &eg->g_irq[irq], head)
    {
        if (gi->chip->handle2) {
            gi->chip->handle2(irq, cpu, level0, level1, gi->opaque);
        } else if (gi->chip->handle) {
            gi->chip->handle(irq, cpu, level0, gi->opaque);
            gi->chip->handle(irq, cpu, level1, gi->opaque);
        }
    }

    return VMM_OK;
}

int vmm_device_emulate_map_host2guest_irq(struct vmm_guest *guest, uint32_t irq, uint32_t host_irq)
{
    struct vmm_device_emulate_guest_irq     *gi;
    struct vmm_device_emulate_guest_context *eg;

    if (!guest) {
        return VMM_EFAIL;
    }

    eg = (struct vmm_device_emulate_guest_context *)guest->aspace.device_emulate_private;

    if (eg->g_irq_count <= irq) {
        return VMM_EINVALID;
    }

    list_for_each_entry(gi, &eg->g_irq[irq], head)
    {
        if (!gi->chip->map_host2guest) {
            continue;
        }

        gi->chip->map_host2guest(irq, host_irq, gi->opaque);
    }

    return VMM_OK;
}

int vmm_device_emulate_unmap_host2guest_irq(struct vmm_guest *guest, uint32_t irq)
{
    struct vmm_device_emulate_guest_irq     *gi;
    struct vmm_device_emulate_guest_context *eg;

    if (!guest) {
        return VMM_EFAIL;
    }

    eg = (struct vmm_device_emulate_guest_context *)guest->aspace.device_emulate_private;

    if (eg->g_irq_count <= irq) {
        return VMM_EINVALID;
    }

    list_for_each_entry(gi, &eg->g_irq[irq], head)
    {
        if (!gi->chip->unmap_host2guest) {
            continue;
        }

        gi->chip->unmap_host2guest(irq, gi->opaque);
    }

    return VMM_OK;
}

int vmm_device_emulate_notify_irq_enabled(struct vmm_guest *guest, uint32_t irq, int cpu)
{
    struct vmm_device_emulate_guest_irq     *gi;
    struct vmm_device_emulate_guest_context *eg;

    if (!guest) {
        return VMM_EFAIL;
    }

    eg = (struct vmm_device_emulate_guest_context *)guest->aspace.device_emulate_private;

    if (eg->g_irq_count <= irq) {
        return VMM_EINVALID;
    }

    list_for_each_entry(gi, &eg->g_irq[irq], head)
    {
        if (!gi->chip->notify_enabled) {
            continue;
        }

        gi->chip->notify_enabled(irq, cpu, gi->opaque);
    }

    return VMM_OK;
}

int vmm_device_emulate_notify_irq_disabled(struct vmm_guest *guest, uint32_t irq, int cpu)
{
    struct vmm_device_emulate_guest_irq     *gi;
    struct vmm_device_emulate_guest_context *eg;

    if (!guest) {
        return VMM_EFAIL;
    }

    eg = (struct vmm_device_emulate_guest_context *)guest->aspace.device_emulate_private;

    if (eg->g_irq_count <= irq) {
        return VMM_EINVALID;
    }

    list_for_each_entry(gi, &eg->g_irq[irq], head)
    {
        if (!gi->chip->notify_disabled) {
            continue;
        }

        gi->chip->notify_disabled(irq, cpu, gi->opaque);
    }

    return VMM_OK;
}

int vmm_device_emulate_register_irqchip(struct vmm_guest *guest, uint32_t irq, struct vmm_device_emulation_irqchip *chip, void *opaque)
{
    bool                                     found;
    struct vmm_device_emulate_guest_irq     *gi;
    struct vmm_device_emulate_guest_context *eg;

    /* Sanity checks */
    if (!guest || !chip) {
        return VMM_EFAIL;
    }

    eg = (struct vmm_device_emulate_guest_context *)guest->aspace.device_emulate_private;

    /* Sanity checks */
    if (eg->g_irq_count <= irq) {
        return VMM_EINVALID;
    }

    /* Check if irqchip is not already registered */
    gi    = NULL;
    found = FALSE;
    list_for_each_entry(gi, &eg->g_irq[irq], head)
    {
        if (gi->chip == chip && gi->opaque == opaque) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        return VMM_EEXIST;
    }

    /* Alloc guest irq */
    gi = vmm_zalloc(sizeof(struct vmm_device_emulate_guest_irq));

    if (!gi) {
        return VMM_ENOMEM;
    }

    /* Initialize guest irq */
    INIT_LIST_HEAD(&gi->head);
    gi->chip   = chip;
    gi->opaque = opaque;

    /* Add guest irq to list */
    list_add_tail(&gi->head, &eg->g_irq[irq]);

    return VMM_OK;
}

int vmm_device_emulate_unregister_irqchip(struct vmm_guest *guest, uint32_t irq, struct vmm_device_emulation_irqchip *chip, void *opaque)
{
    bool                                     found;
    struct vmm_device_emulate_guest_irq     *gi;
    struct vmm_device_emulate_guest_context *eg;

    /* Sanity checks */
    if (!guest || !chip) {
        return VMM_EFAIL;
    }

    eg = (struct vmm_device_emulate_guest_context *)guest->aspace.device_emulate_private;

    /* Sanity checks */
    if (eg->g_irq_count <= irq) {
        return VMM_EINVALID;
    }

    /* Check if irqchip is not already unregistered */
    gi    = NULL;
    found = FALSE;
    list_for_each_entry(gi, &eg->g_irq[irq], head)
    {
        if (gi->chip == chip && gi->opaque == opaque) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        return VMM_ENOTAVAIL;
    }

    /* Remove from list and free guest irq */
    list_del(&gi->head);
    vmm_free(gi);

    return VMM_OK;
}

uint32_t vmm_device_emulate_count_irqs(struct vmm_guest *guest)
{
    struct vmm_device_emulate_guest_context *eg;

    if (!guest) {
        return 0;
    }

    eg = (struct vmm_device_emulate_guest_context *)guest->aspace.device_emulate_private;

    return (eg) ? eg->g_irq_count : 0;
}

int vmm_device_emulate_register_emulator(vmm_emulator_t *emu)
{
    bool            found;
    vmm_emulator_t *e;

    if (!emu || !emu->probe || !emu->remove || !emu->reset || (emu->endian == VMM_DEVICE_EMULATE_UNKNOWN_ENDIAN) ||
        (VMM_DEVICE_EMULATE_MAX_ENDIAN <= emu->endian)) {
        return VMM_EFAIL;
    }

    vmm_mutex_lock(&dectrl.emu_lock);

    e     = NULL;
    found = FALSE;
    list_for_each_entry(e, &dectrl.emu_list, head)
    {
        if (strcmp(e->name, emu->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (found) {
        vmm_mutex_unlock(&dectrl.emu_lock);
        return VMM_EINVALID;
    }

    INIT_LIST_HEAD(&emu->head);

    list_add_tail(&emu->head, &dectrl.emu_list);

    vmm_mutex_unlock(&dectrl.emu_lock);

    return VMM_OK;
}

int vmm_device_emulate_simple_read8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = edev->emu->read_simple(edev, offset, &regval, sizeof(uint8_t));

    if (!rc) {
        *dst = regval & 0xFF;
    }

    return rc;
}

int vmm_device_emulate_simple_read16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t *dst)
{
    int      rc;
    uint32_t regval = 0x0;

    rc              = edev->emu->read_simple(edev, offset, &regval, sizeof(uint16_t));

    if (!rc) {
        *dst = regval & 0xFFFF;
    }

    return rc;
}

int vmm_device_emulate_simple_read32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t *dst)
{
    return edev->emu->read_simple(edev, offset, dst, sizeof(uint32_t));
}

int vmm_device_emulate_simple_write8(vmm_emulate_device_t *edev, physical_addr_t offset, uint8_t src)
{
    return edev->emu->write_simple(edev, offset, 0xFFFFFF00, src, sizeof(uint8_t));
}

int vmm_device_emulate_simple_write16(vmm_emulate_device_t *edev, physical_addr_t offset, uint16_t src)
{
    return edev->emu->write_simple(edev, offset, 0xFFFF0000, src, sizeof(uint16_t));
}

int vmm_device_emulate_simple_write32(vmm_emulate_device_t *edev, physical_addr_t offset, uint32_t src)
{
    return edev->emu->write_simple(edev, offset, 0x00000000, src, sizeof(uint32_t));
}

int vmm_device_emulate_unregister_emulator(vmm_emulator_t *emu)
{
    bool            found;
    vmm_emulator_t *e;

    vmm_mutex_lock(&dectrl.emu_lock);

    if (emu == NULL || list_empty(&dectrl.emu_list)) {
        vmm_mutex_unlock(&dectrl.emu_lock);
        return VMM_EFAIL;
    }

    e     = NULL;
    found = FALSE;
    list_for_each_entry(e, &dectrl.emu_list, head)
    {
        if (strcmp(e->name, emu->name) == 0) {
            found = TRUE;
            break;
        }
    }

    if (!found) {
        vmm_mutex_unlock(&dectrl.emu_lock);
        return VMM_ENOTAVAIL;
    }

    list_del(&e->head);

    vmm_mutex_unlock(&dectrl.emu_lock);

    return VMM_OK;
}

vmm_emulator_t *vmm_device_emulate_find_emulator(const char *name)
{
    bool            found;
    vmm_emulator_t *emu;

    if (!name) {
        return NULL;
    }

    found = FALSE;
    emu   = NULL;

    vmm_mutex_lock(&dectrl.emu_lock);

    list_for_each_entry(emu, &dectrl.emu_list, head)
    {
        if (strcmp(emu->name, name) == 0) {
            found = TRUE;
            break;
        }
    }

    vmm_mutex_unlock(&dectrl.emu_lock);

    if (!found) {
        return NULL;
    }

    return emu;
}

vmm_emulator_t *vmm_device_emulate_emulator(int index)
{
    bool            found;
    vmm_emulator_t *emu;

    if (index < 0) {
        return NULL;
    }

    emu   = NULL;
    found = FALSE;

    vmm_mutex_lock(&dectrl.emu_lock);

    list_for_each_entry(emu, &dectrl.emu_list, head)
    {
        if (!index) {
            found = TRUE;
            break;
        }

        index--;
    }

    vmm_mutex_unlock(&dectrl.emu_lock);

    if (!found) {
        return NULL;
    }

    return emu;
}

uint32_t vmm_device_emulate_emulator_count(void)
{
    uint32_t        retval;
    vmm_emulator_t *emu;

    retval = 0;

    vmm_mutex_lock(&dectrl.emu_lock);

    list_for_each_entry(emu, &dectrl.emu_list, head)
    {
        retval++;
    }

    vmm_mutex_unlock(&dectrl.emu_lock);

    return retval;
}

static int devemu_sync(struct vmm_guest *guest, vmm_emulate_device_t *edev, uint64_t val, void *v)
{
    debug_sync(edev);

    if (edev->emu->sync) {
        return edev->emu->sync(edev, val, v);
    }

    return VMM_OK;
}

int vmm_device_emulate_sync_children(struct vmm_guest *guest, vmm_emulate_device_t *edev, uint64_t val, void *v)
{
    int                   rc;
    irq_flags_t           f;
    vmm_emulate_device_t *e, *en;

    if (!guest || !edev) {
        return VMM_EFAIL;
    }

    vmm_read_lock_irq_save_lite(&edev->child_list_lock, f);

    list_for_each_entry_safe(e, en, &edev->child_list, head)
    {
        vmm_read_unlock_irq_restore_lite(&edev->child_list_lock, f);
        rc = devemu_sync(guest, e, val, v);

        if (rc) {
            return rc;
        }

        vmm_read_lock_irq_save_lite(&edev->child_list_lock, f);
    }

    vmm_read_unlock_irq_restore_lite(&edev->child_list_lock, f);

    return VMM_OK;
}

int vmm_device_emulate_sync_parent(struct vmm_guest *guest, vmm_emulate_device_t *edev, uint64_t val, void *v)
{
    if (!guest || !edev) {
        return VMM_EFAIL;
    }

    if (!edev->parent) {
        return VMM_EINVALID;
    }

    return devemu_sync(guest, edev->parent, val, v);
}

int vmm_device_emulate_reset_context(struct vmm_guest *guest)
{
    if (!guest) {
        return VMM_EFAIL;
    }

    /* For now nothing to do here. */

    return VMM_OK;
}

static int devemu_reset_edev(struct vmm_guest *guest, vmm_emulate_device_t *edev)
{
    irq_flags_t           f;
    int                   rc = VMM_OK;
    vmm_emulate_device_t *e, *en;

    debug_reset(edev);

    if ((rc = edev->emu->reset(edev))) {
        if (edev->parent) {
            vmm_printf("%s: %s/%s/%s reset error %d\n", __func__, guest->name, edev->parent->node->name, edev->node->name, rc);
        } else {
            vmm_printf("%s: %s/%s reset error %d\n", __func__, guest->name, edev->node->name, rc);
        }

        return rc;
    }

    vmm_read_lock_irq_save_lite(&edev->child_list_lock, f);

    list_for_each_entry_safe(e, en, &edev->child_list, head)
    {
        vmm_read_unlock_irq_restore_lite(&edev->child_list_lock, f);
        rc = devemu_reset_edev(guest, e);

        if (rc) {
            return rc;
        }

        vmm_read_lock_irq_save_lite(&edev->child_list_lock, f);
    }

    vmm_read_unlock_irq_restore_lite(&edev->child_list_lock, f);

    return VMM_OK;
}

int vmm_device_emulate_reset_region(struct vmm_guest *guest, struct vmm_region *reg)
{
    vmm_emulate_device_t *edev;

    if (!reg || !reg->device_emulate_private) {
        return VMM_EFAIL;
    }

    if (!(reg->flags & VMM_REGION_IS_DEVICE) || (reg->flags & VMM_REGION_ALIAS)) {
        return VMM_EINVALID;
    }

    edev = (vmm_emulate_device_t *)reg->device_emulate_private;

    return devemu_reset_edev(guest, edev);
}

static int devemu_remove_edev(struct vmm_guest *guest, vmm_emulate_device_t *edev)
{
    int                   rc;
    irq_flags_t           f;
    vmm_emulate_device_t *e;

    vmm_write_lock_irq_save_lite(&edev->child_list_lock, f);

    while (!list_empty(&edev->child_list)) {
        e = list_first_entry(&edev->child_list, vmm_emulate_device_t, head);

        list_del(&e->head);

        vmm_write_unlock_irq_restore_lite(&edev->child_list_lock, f);

        rc = devemu_remove_edev(guest, e);

        if (rc) {
            vmm_write_lock_irq_save_lite(&edev->child_list_lock, f);
            list_add(&e->head, &edev->child_list);
            vmm_write_unlock_irq_restore_lite(&edev->child_list_lock, f);
            return rc;
        }

        vmm_write_lock_irq_save_lite(&edev->child_list_lock, f);
    }

    vmm_write_unlock_irq_restore_lite(&edev->child_list_lock, f);

    debug_remove(edev);

    if ((rc = edev->emu->remove(edev))) {
        if (edev->parent) {
            vmm_printf("%s: %s/%s/%s remove error %d\n", __func__, guest->name, edev->parent->node->name, edev->node->name, rc);
        } else {
            vmm_printf("%s: %s/%s remove error %d\n", __func__, guest->name, edev->node->name, rc);
        }

        return rc;
    }

    vmm_device_tree_dref_node(edev->node);
    edev->node = NULL;

    if (edev->reg) {
        edev->reg->device_emulate_private = NULL;
        edev->reg                         = NULL;
    }

    vmm_free(edev);

    return VMM_OK;
}

int vmm_device_emulate_remove_region(struct vmm_guest *guest, struct vmm_region *reg)
{
    int                   rc;
    vmm_emulate_device_t *edev;

    if (!reg) {
        return VMM_EFAIL;
    }

    if (!(reg->flags & VMM_REGION_IS_DEVICE) || (reg->flags & VMM_REGION_ALIAS)) {
        return VMM_EINVALID;
    }

    if (reg->device_emulate_private) {
        edev = reg->device_emulate_private;

        rc   = devemu_remove_edev(guest, edev);

        if (rc) {
            return rc;
        }
    }

    return VMM_OK;
}

static int set_debug_info(vmm_emulate_device_t *edev)
{
    int rc = VMM_OK;
#ifdef CONFIG_DEVICE_EMULATE_DEBUG
    char const *const attr = VMM_DEVICE_TREE_DEBUG_ATTR_NAME;
    uint32_t          i;

    i = vmm_device_tree_attrlen(edev->node, attr) / sizeof(uint32_t);

    if (i > 0) {
        rc = vmm_device_tree_read_u32_atindex(edev->node, attr, &edev->debug_info, 0);

        if (VMM_OK != rc) {
            edev->debug_info = VMM_DEVICE_EMULATE_DEBUG_NONE;
        }
    } else {
        edev->debug_info = VMM_DEVICE_EMULATE_DEBUG_NONE;
    }

#endif
    return rc;
}

static vmm_emulate_device_t *devemu_probe_edev(
    struct vmm_guest *guest, vmm_device_tree_node_t *node, struct vmm_region *reg, vmm_emulate_device_t *parent)
{
    int                                  rc;
    bool                                 found;
    irq_flags_t                          f;
    vmm_emulator_t                      *emu;
    vmm_device_tree_node_t              *child;
    vmm_emulate_device_t                *edev = NULL, *edevc;
    const struct vmm_device_tree_nodeid *match;

    vmm_mutex_lock(&dectrl.emu_lock);

    found = FALSE;
    list_for_each_entry(emu, &dectrl.emu_list, head)
    {
        match = vmm_device_tree_match_node(emu->match_table, node);

        if (!match) {
            continue;
        }

        found = TRUE;

        edev  = vmm_zalloc(sizeof(vmm_emulate_device_t));

        if (!edev) {
            vmm_mutex_unlock(&dectrl.emu_lock);
            return VMM_ERR_PTR(VMM_ENOMEM);
        }

        INIT_SPIN_LOCK(&edev->lock);
        vmm_device_tree_ref_node(node);
        edev->node   = node;
        edev->reg    = reg;
        edev->emu    = emu;
        edev->parent = parent;
        INIT_LIST_HEAD(&edev->head);
        INIT_RW_LOCK(&edev->child_list_lock);
        INIT_LIST_HEAD(&edev->child_list);
        edev->private = NULL;
        set_debug_info(edev);

        debug_probe(edev);

        if ((rc = emu->probe(guest, edev, match))) {
            if (parent) {
                vmm_printf("%s: %s/%s/%s probe error %d\n", __func__, guest->name, parent->node->name, edev->node->name, rc);
            } else {
                vmm_printf("%s: %s/%s probe error %d\n", __func__, guest->name, edev->node->name, rc);
            }

            vmm_mutex_unlock(&dectrl.emu_lock);
            vmm_device_tree_dref_node(edev->node);
            edev->node = NULL;
            vmm_free(edev);
            return VMM_ERR_PTR(rc);
        }

        debug_reset(edev);

        if ((rc = emu->reset(edev))) {
            if (parent) {
                vmm_printf("%s: %s/%s/%s reset error %d\n", __func__, guest->name, parent->node->name, edev->node->name, rc);
            } else {
                vmm_printf("%s: %s/%s reset error %d\n", __func__, guest->name, edev->node->name, rc);
            }

            vmm_mutex_unlock(&dectrl.emu_lock);
            vmm_device_tree_dref_node(edev->node);
            edev->node = NULL;
            vmm_free(edev);
            return VMM_ERR_PTR(rc);
        }

        if (reg) {
            reg->device_emulate_private = edev;
        }
    }

    vmm_mutex_unlock(&dectrl.emu_lock);

    if (!found) {
        if (parent) {
            vmm_printf("%s: No emulator found for %s/%s/%s\n", __func__, guest->name, parent->node->name, node->name);
        } else {
            vmm_printf("%s: No emulator found for %s/%s\n", __func__, guest->name, node->name);
        }

        return VMM_ERR_PTR(VMM_ENOTAVAIL);
    }

    if (vmm_device_tree_getattr(edev->node, VMM_DEVICE_TREE_NO_CHILD_PROBE_ATTR_NAME)) {
        goto skip_child_probe;
    }

    vmm_device_tree_for_each_child(child, edev->node)
    {
        edevc = devemu_probe_edev(guest, child, NULL, edev);

        if (VMM_IS_ERR(edevc)) {
            vmm_device_tree_dref_node(child);
            devemu_remove_edev(guest, edev);
            return edevc;
        }

        vmm_write_lock_irq_save_lite(&edev->child_list_lock, f);
        list_add_tail(&edevc->head, &edev->child_list);
        vmm_write_unlock_irq_restore_lite(&edev->child_list_lock, f);
    }

skip_child_probe:
    return edev;
}

int vmm_device_emulate_probe_region(struct vmm_guest *guest, struct vmm_region *reg)
{
    vmm_emulate_device_t *edev;

    if (!guest || !reg || reg->device_emulate_private) {
        return VMM_EFAIL;
    }

    if (!(reg->flags & VMM_REGION_IS_DEVICE) || (reg->flags & VMM_REGION_ALIAS)) {
        return VMM_EINVALID;
    }

    edev = devemu_probe_edev(guest, reg->node, reg, NULL);

    if (VMM_IS_ERR(edev)) {
        return VMM_PTR_ERR(edev);
    }

    return VMM_OK;
}

int vmm_device_emulate_init_context(struct vmm_guest *guest)
{
    uint32_t                                 ite;
    int                                      rc = VMM_OK;
    struct vmm_device_emulate_guest_context *eg;

    if (!guest) {
        rc = VMM_EFAIL;
        goto devemu_init_context_done;
    }

    if (guest->aspace.device_emulate_private) {
        rc = VMM_EFAIL;
        goto devemu_init_context_done;
    }

    eg = vmm_zalloc(sizeof(struct vmm_device_emulate_guest_context));

    if (!eg) {
        rc = VMM_EFAIL;
        goto devemu_init_context_done;
    }

    eg->g_irq       = NULL;
    eg->g_irq_count = 0;
    rc              = vmm_device_tree_read_u32(guest->aspace.node, VMM_DEVICE_TREE_GUESTIRQCNT_ATTR_NAME, &eg->g_irq_count);

    if (rc) {
        goto devemu_init_context_free;
    }

    eg->g_irq = vmm_zalloc(sizeof(struct double_list) * eg->g_irq_count);

    if (!eg->g_irq) {
        rc = VMM_ENOMEM;
        goto devemu_init_context_free;
    }

    for (ite = 0; ite < eg->g_irq_count; ite++) {
        INIT_LIST_HEAD(&eg->g_irq[ite]);
    }

    guest->aspace.device_emulate_private = eg;

    goto devemu_init_context_done;

devemu_init_context_free:
    vmm_free(eg);
devemu_init_context_done:
    return rc;
}

int vmm_device_emulate_deinit_context(struct vmm_guest *guest)
{
    int                                      rc = VMM_OK;
    struct vmm_device_emulate_guest_context *eg;

    if (!guest) {
        return VMM_EFAIL;
    }

    eg                                   = guest->aspace.device_emulate_private;
    guest->aspace.device_emulate_private = NULL;

    if (eg) {
        if (eg->g_irq) {
            vmm_free(eg->g_irq);
            eg->g_irq       = NULL;
            eg->g_irq_count = 0;
        }

        vmm_free(eg);
    }

    return rc;
}

int __init vmm_device_emulate_init(void)
{
    memset(&dectrl, 0, sizeof(dectrl));

#ifdef CONFIG_CPU_BE
    dectrl.host_endian = VMM_DEVICE_EMULATE_BIG_ENDIAN;
#else
    dectrl.host_endian = VMM_DEVICE_EMULATE_LITTLE_ENDIAN;
#endif

    INIT_MUTEX(&dectrl.emu_lock);
    INIT_LIST_HEAD(&dectrl.emu_list);

    return VMM_OK;
}
