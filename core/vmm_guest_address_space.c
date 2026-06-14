/**
 * @brief
 *     客户机地址空间管理、虚拟地址池维护、客户机物理地址与主机物理地址的映射等功能
 * @file vmm_guest_address_space.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief 客户机地址空间实现
 */

#include <arch_guest.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_device_emulate.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_guest_address_space.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_host_ram.h>
#include <vmm_notifier.h>
#include <vmm_stdio.h>

static BLOCKING_NOTIFIER_CHAIN(guest_address_space_notifier_chain);

/**
 * @brief 注册客户机地址空间通知器客户端
 * @param nb 要注册的通知器块
 * @return 返回操作结果，成功为VMM_OK
 */
int vmm_guest_address_space_register_client(vmm_notifier_block_t *nb)
{
    int rc = vmm_blocking_notifier_register(&guest_address_space_notifier_chain, nb);

    return rc;
}

/**
 * @brief 注销客户机地址空间通知器客户端
 * @param nb 要注销的通知器块
 * @return 返回操作结果，成功为VMM_OK
 */
int vmm_guest_address_space_unregister_client(vmm_notifier_block_t *nb)
{
    int rc = vmm_blocking_notifier_unregister(&guest_address_space_notifier_chain, nb);

    return rc;
}

/**
 * @brief 遍历客户机的区域
 * @param guest 客户机指针
 * @param reg_flags 区域标志，用于过滤区域
 * @param func 回调函数，用于处理每个区域
 * @param private 传递给回调函数的私有数据
 */
void vmm_guest_iterate_region(
    struct vmm_guest *guest, uint32_t reg_flags, void (*func)(struct vmm_guest *, struct vmm_region *, void *), void *private)
{
    irq_flags_t                     flags; /**< 标志位 */
    vmm_rwlock_t                   *root_lock = NULL; /**< NULL成员 */
    red_black_root_t          *root      = NULL; /**< NULL成员 */
    struct vmm_region              *reg = NULL, *n = NULL; /**< NULL成员 */
    struct vmm_guest_address_space *addr_space; /**< 地址空间 */

    if (!guest || !func) {
        return;
    }

    addr_space = &guest->addr_space; /**< &guest->addr_space成员 */

    /* Find out region tree root */
    if (reg_flags & VMM_REGION_IO) {
        root      = &addr_space->reg_iotree; /**< &addr_space->reg_iotree成员 */
        root_lock = &addr_space->reg_iotree_lock; /**< &addr_space->reg_iotree_lock成员 */
    } else {
        root      = &addr_space->reg_memtree; /**< &addr_space->reg_memtree成员 */
        root_lock = &addr_space->reg_memory_tree_lock; /**< &addr_space->reg_memory_tree_lock成员 */
    }

    /* Post-order traversal for red_black_tree nodes */
    vmm_read_lock_irq_save_lite(root_lock, flags); /**< flags)成员 */
    red_black_tree_postorder_for_each_entry_safe(reg, n, root, head)
    {
        if ((reg->flags & reg_flags) == reg_flags) {
            vmm_read_unlock_irq_restore_lite(root_lock, flags); /**< flags)成员 */
            func(guest, reg, private); /**< private)成员 */
            vmm_read_lock_irq_save_lite(root_lock, flags); /**< flags)成员 */
        }
    }
    vmm_read_unlock_irq_restore_lite(root_lock, flags); /**< flags)成员 */
}

/**
 * @brief 查找客户机中的区域
 * @param guest 客户机指针
 * @param guest_physical_addr 客户机物理地址
 * @param reg_flags 区域标志
 * @param resolve_alias 是否解析别名区域
 * @return 返回找到的区域指针，如果未找到则返回NULL
 */
struct vmm_region *vmm_guest_find_region(struct vmm_guest *guest, physical_addr_t guest_physical_addr, uint32_t reg_flags, bool resolve_alias)
{
    bool                            found = FALSE; /**< FALSE成员 */
    uint32_t                        cmp_flags; /**< cmp_flags成员 */
    irq_flags_t                     flags; /**< 标志位 */
    vmm_rwlock_t                   *root_lock = NULL; /**< NULL成员 */
    red_black_root_t          *root      = NULL; /**< NULL成员 */
    red_black_node_t          *pos       = NULL; /**< NULL成员 */
    struct vmm_region              *reg       = NULL; /**< NULL成员 */
    struct vmm_guest_address_space *addr_space; /**< 地址空间 */

    if (!guest) {
        return NULL; /**< NULL成员 */
    }

    addr_space    = &guest->addr_space; /**< &guest->addr_space成员 */

    /* Determine flags we need to compare */
    cmp_flags = reg_flags & ~VMM_REGION_MANIFEST_MASK; /**< ~VMM_REGION_MANIFEST_MASK成员 */

    /* Find out region tree root */
    if (reg_flags & VMM_REGION_IO) {
        root      = &addr_space->reg_iotree; /**< &addr_space->reg_iotree成员 */
        root_lock = &addr_space->reg_iotree_lock; /**< &addr_space->reg_iotree_lock成员 */
    } else {
        root      = &addr_space->reg_memtree; /**< &addr_space->reg_memtree成员 */
        root_lock = &addr_space->reg_memory_tree_lock; /**< &addr_space->reg_memory_tree_lock成员 */
    }

    /* Try to find region ignoring required manifest flags */
    reg   = NULL; /**< NULL成员 */
    found = FALSE; /**< FALSE成员 */
    vmm_read_lock_irq_save_lite(root_lock, flags); /**< flags)成员 */
    pos = root->red_black_node; /**< root->red_black_node成员 */

    while (pos) {
        reg = rb_entry(pos, struct vmm_region, head); /**< head)成员 */

        if (guest_physical_addr < VMM_REGION_GPHYS_START(reg)) {
            pos = pos->rb_left; /**< pos->rb_left成员 */
        } else if (VMM_REGION_GPHYS_END(reg) <= guest_physical_addr) {
            pos = pos->rb_right; /**< pos->rb_right成员 */
        } else {
            if ((reg->flags & cmp_flags) == cmp_flags) {
                found = TRUE; /**< TRUE成员 */
            }

            break;
        }
    }

    vmm_read_unlock_irq_restore_lite(root_lock, flags); /**< flags)成员 */

    if (!found) {
        return NULL; /**< NULL成员 */
    }

    /* Check if we can skip resolve alias */
    if (!resolve_alias) {
        goto done; /**< done成员 */
    }

    /* Resolve aliased regions */
    while (reg->flags & VMM_REGION_ALIAS) {
        guest_physical_addr = VMM_REGION_GPHYS_TO_APHYS(reg, guest_physical_addr); /**< guest_physical_addr)成员 */
        reg                 = NULL; /**< NULL成员 */
        found               = FALSE; /**< FALSE成员 */
        vmm_read_lock_irq_save_lite(root_lock, flags); /**< flags)成员 */
        pos = root->red_black_node; /**< root->red_black_node成员 */

        while (pos) {
            reg = rb_entry(pos, struct vmm_region, head); /**< head)成员 */

            if (guest_physical_addr < VMM_REGION_GPHYS_START(reg)) {
                pos = pos->rb_left; /**< pos->rb_left成员 */
            } else if (VMM_REGION_GPHYS_END(reg) <= guest_physical_addr) {
                pos = pos->rb_right; /**< pos->rb_right成员 */
            } else {
                if ((reg->flags & cmp_flags) == cmp_flags) {
                    found = TRUE; /**< TRUE成员 */
                }

                break;
            }
        }

        vmm_read_unlock_irq_restore_lite(root_lock, flags); /**< flags)成员 */

        if (!found) {
            return NULL; /**< NULL成员 */
        }
    }

done:
    cmp_flags = reg_flags & VMM_REGION_MANIFEST_MASK; /**< VMM_REGION_MANIFEST_MASK成员 */

    if ((reg->flags & cmp_flags) != cmp_flags) {
        return NULL; /**< NULL成员 */
    }

    return reg; /**< 寄存器 */
}

/**
 * @brief 获取客户机物理地址的映射偏移
 * @param reg 寄存器值或索引
 * @param map_index 索引
 * @return 偏移量
 */
static physical_addr_t mapping_gphys_offset(struct vmm_region *reg, uint32_t map_index)
{
    if (reg->maps_count <= map_index) {
        return reg->phys_size;
    }

    return ((physical_addr_t)map_index) << reg->map_order;
}

/**
 * @brief 获取客户机物理地址映射的大小
 * @param reg 寄存器值或索引
 * @param map_index 索引
 * @return 大小值（字节）
 */
static physical_size_t mapping_phys_size(struct vmm_region *reg, uint32_t map_index)
{
    physical_size_t map_size;
    physical_size_t size;

    if (reg->maps_count <= map_index) {
        return 0;
    }

    map_size = ((physical_size_t)1) << reg->map_order;

    size     = reg->phys_size - mapping_gphys_offset(reg, map_index);

    return (size < map_size) ? size : map_size;
}

/**
 * @brief 查找地址映射
 * @return 成功返回目标指针，失败返回NULL
 */
static struct vmm_region_mapping *mapping_find(
    struct vmm_guest *guest, struct vmm_region *reg, uint32_t *map_index, physical_addr_t guest_physical_addr)
{
    uint32_t i; /**< i */

    if ((guest_physical_addr < VMM_REGION_GPHYS_START(reg)) || (VMM_REGION_GPHYS_END(reg) <= guest_physical_addr)) {
        return NULL; /**< NULL成员 */
    }

    i = (guest_physical_addr - VMM_REGION_GPHYS_START(reg)) >> reg->map_order; /**< reg->map_order成员 */

    if (map_index) {
        *map_index = i;
    }

    return &reg->maps[i]; /**< 映射数组 */
}

/**
 * @brief 查找客户机物理地址对应的映射条目
 */
void vmm_guest_find_mapping(
    struct vmm_guest *guest, struct vmm_region *reg, physical_addr_t guest_physical_addr, physical_addr_t *hphys_addr, physical_size_t *avail_size)
{
    uint32_t                   i; /**< i */
    physical_addr_t            map_gphys_addr; /**< map_gphys_addr成员 */
    physical_addr_t            hphys = 0; /**< 0 */
    physical_size_t            size  = 0; /**< 0 */
    struct vmm_region_mapping *map; /**< 映射 */

    if (!guest || !reg) {
        goto done; /**< done成员 */
    }

    map = mapping_find(guest, reg, &i, guest_physical_addr); /**< guest_physical_addr)成员 */

    if (!map) {
        goto done; /**< done成员 */
    }

    map_gphys_addr = reg->guest_physical_addr + mapping_gphys_offset(reg, i); /**< i) */

    hphys          = map->hphys_addr + (guest_physical_addr - map_gphys_addr); /**< map_gphys_addr)成员 */
    size           = map->hphys_addr + mapping_phys_size(reg, i) - hphys; /**< hphys成员 */

done:

    if (hphys_addr) {
        *hphys_addr = hphys;
    }

    if (avail_size) {
        *avail_size = size;
    }
}

/**
 * @brief 客户机 遍历 映射
 */
void vmm_guest_iterate_mapping(
    struct vmm_guest *guest, struct vmm_region *reg,
    void (*func)(
        struct vmm_guest *guest, struct vmm_region *reg, physical_addr_t guest_physical_addr, physical_addr_t hphys_addr, physical_size_t phys_size,
        void *private),
    void *private)
{
    uint32_t i; /**< i */

    if (!guest || !reg || !func) {
        return;
    }

    for (i = 0; i < reg->maps_count; i++) {
        func(guest, reg, reg->guest_physical_addr + mapping_gphys_offset(reg, i), reg->maps[i].hphys_addr, mapping_phys_size(reg, i), private); /**< 映射数组 */
    }
}

/**
 * @brief 用真实设备地址覆盖客户机的设备映射
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_overwrite_real_device_mapping(
    struct vmm_guest *guest, struct vmm_region *reg, physical_addr_t guest_physical_addr, physical_addr_t hphys_addr)
{
    struct vmm_region_mapping *map; /**< 映射 */

    if (!guest || !reg) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    if (!(reg->flags & VMM_REGION_REAL) || !(reg->flags & VMM_REGION_IS_DEVICE)) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    map = mapping_find(guest, reg, NULL, guest_physical_addr); /**< guest_physical_addr)成员 */

    if (!map) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    map->hphys_addr = hphys_addr; /**< 主机物理地址 */

    return VMM_OK; /**< VMM_OK成员 */
}

/**
 * @brief 客户机 内存 读
 * @param guest 指向客户机结构体的指针
 * @param guest_physical_addr 客户机物理地址
 * @param dst 目标缓冲区指针
 * @param len 大小
 * @param cacheable 是否可缓存标志
 * @return 成功返回实际读取的字节数，失败返回0
 */
uint32_t vmm_guest_memory_read(struct vmm_guest *guest, physical_addr_t guest_physical_addr, void *dst, uint32_t len, bool cacheable)
{
    uint32_t bytes_read = 0;
    uint32_t to_read;
    physical_size_t    avail_size;
    physical_addr_t    hphys_addr;
    struct vmm_region *reg = NULL;

    if (!guest || !dst || !len) {
        return 0; /**< 0 */
    }

    while (bytes_read < len) {
        reg = vmm_guest_find_region(guest, guest_physical_addr, VMM_REGION_REAL | VMM_REGION_MEMORY, TRUE);

        if (!reg) {
            break;
        }

        vmm_guest_find_mapping(guest, reg, guest_physical_addr, &hphys_addr, &avail_size);
        to_read = (avail_size < U32_MAX) ? avail_size : U32_MAX;
        to_read = ((len - bytes_read) < to_read) ? (len - bytes_read) : to_read;

        to_read = vmm_host_memory_read(hphys_addr, dst, to_read, cacheable);

        if (!to_read) {
            break;
        }

        guest_physical_addr += to_read;
        bytes_read += to_read;
        dst += to_read;
    }

    return bytes_read;
}

/**
 * @brief 客户机 内存 写
 * @param guest 指向客户机结构体的指针
 * @param guest_physical_addr 客户机物理地址
 * @param src 源设备树节点
 * @param len 大小
 * @param cacheable 是否可缓存标志
 * @return 成功返回实际写入的字节数，失败返回0
 */
uint32_t vmm_guest_memory_write(struct vmm_guest *guest, physical_addr_t guest_physical_addr, void *src, uint32_t len, bool cacheable)
{
    uint32_t bytes_written = 0;
    uint32_t to_write;
    physical_size_t    avail_size;
    physical_addr_t    hphys_addr;
    struct vmm_region *reg = NULL;

    if (!guest || !src || !len) {
        return 0; /**< 0 */
    }

    while (bytes_written < len) {
        reg = vmm_guest_find_region(guest, guest_physical_addr, VMM_REGION_REAL | VMM_REGION_MEMORY, TRUE);

        if (!reg) {
            break;
        }

        vmm_guest_find_mapping(guest, reg, guest_physical_addr, &hphys_addr, &avail_size);
        to_write = (avail_size < U32_MAX) ? avail_size : U32_MAX;
        to_write = ((len - bytes_written) < to_write) ? (len - bytes_written) : to_write;

        to_write = vmm_host_memory_write(hphys_addr, src, to_write, cacheable);

        if (!to_write) {
            break;
        }

        guest_physical_addr += to_write;
        bytes_written += to_write;
        src += to_write;
    }

    return bytes_written;
}

/**
 * @brief 客户机 物理 映射
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_physical_map(
    struct vmm_guest *guest, physical_addr_t guest_physical_addr, physical_size_t gphys_size, physical_addr_t *hphys_addr, physical_size_t *phys_size,
    uint32_t *reg_flags)
{
    physical_addr_t    hphys; /**< hphys成员 */
    physical_size_t    size; /**< 大小 */
    struct vmm_region *reg = NULL; /**< NULL成员 */

    if (!guest || !hphys_addr) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    reg = vmm_guest_find_region(guest, guest_physical_addr, VMM_REGION_MEMORY, FALSE); /**< FALSE)成员 */

    if (!reg) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    while (reg->flags & VMM_REGION_ALIAS) {
        guest_physical_addr = VMM_REGION_GPHYS_TO_APHYS(reg, guest_physical_addr); /**< guest_physical_addr)成员 */
        reg                 = vmm_guest_find_region(guest, guest_physical_addr, VMM_REGION_MEMORY, FALSE); /**< FALSE)成员 */

        if (!reg) {
            return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
        }
    }

    vmm_guest_find_mapping(guest, reg, guest_physical_addr, &hphys, &size); /**< &size)成员 */

    if (gphys_size < size) {
        size = gphys_size; /**< gphys_size成员 */
    }

    if (hphys_addr) {
        *hphys_addr = hphys;
    }

    if (phys_size) {
        *phys_size = size;
    }

    if (reg_flags) {
        *reg_flags = reg->flags;
    }

    return VMM_OK; /**< VMM_OK成员 */
}

/**
 * @brief 取消客户机物理地址到主机地址的映射
 * @param guest 指向客户机结构体的指针
 * @param guest_physical_addr 客户机物理地址
 * @param phys_size 物理内存大小
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_physical_unmap(struct vmm_guest *guest, physical_addr_t guest_physical_addr, physical_size_t phys_size)
{
    /* We don't have dynamic mappings for guest regions
     * so nothing to do here.
     */
    return VMM_OK;
}

/**
 * @brief 检查地址空间区域节点是否有效
 * @param rnode 资源节点指针
 * @return 有效返回TRUE，否则返回FALSE
 */
bool is_region_node_valid(vmm_device_tree_node_t *rnode)
{
    const char         *aval;
    vmm_share_memory_t *share_memory;
    bool                is_real         = FALSE;
    bool                is_alias        = FALSE;
    bool                is_alloced      = FALSE;
    bool                is_colored      = FALSE;
    bool                is_shared       = FALSE;
    physical_size_t     size            = 0;
    bool                shm_available   = FALSE;
    physical_size_t     shm_size        = 0;
    uint32_t            shm_align_order = 0;
    uint32_t first_color = 0;
    uint32_t num_colors = 0;
    uint32_t align_order = 0;
    physical_addr_t guest_physical_addr = 0;
    physical_addr_t alias_physical_addr = 0;
    physical_addr_t hphys_addr = 0;

    if (vmm_device_tree_read_string(rnode, VMM_DEVICE_TREE_MANIFEST_TYPE_ATTR_NAME, &aval)) {
        return FALSE;
    }

    if (strcmp(aval, VMM_DEVICE_TREE_MANIFEST_TYPE_VAL_REAL) != 0 && strcmp(aval, VMM_DEVICE_TREE_MANIFEST_TYPE_VAL_VIRTUAL) != 0 &&
        strcmp(aval, VMM_DEVICE_TREE_MANIFEST_TYPE_VAL_ALIAS) != 0) {
        return FALSE;
    }

    if (strcmp(aval, VMM_DEVICE_TREE_MANIFEST_TYPE_VAL_REAL) == 0) {
        is_real = TRUE;
    }

    if (strcmp(aval, VMM_DEVICE_TREE_MANIFEST_TYPE_VAL_ALIAS) == 0) {
        is_alias = TRUE;
    }

    if (vmm_device_tree_read_string(rnode, VMM_DEVICE_TREE_ADDRESS_TYPE_ATTR_NAME, &aval)) {
        return FALSE;
    }

    if (strcmp(aval, VMM_DEVICE_TREE_ADDRESS_TYPE_VAL_IO) != 0 && strcmp(aval, VMM_DEVICE_TREE_ADDRESS_TYPE_VAL_MEMORY) != 0) {
        return FALSE;
    }

    if (vmm_device_tree_read_string(rnode, VMM_DEVICE_TREE_DEVICE_TYPE_ATTR_NAME, &aval)) {
        return FALSE;
    }

    if (!strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_ALLOCED_RAM) || !strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_ALLOCED_ROM)) {
        is_alloced = TRUE;
    }

    if (!strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_COLORED_RAM) || !strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_COLORED_ROM)) {
        is_colored = TRUE;
    }

    if (!strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_SHARED_RAM) || !strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_SHARED_ROM)) {
        is_shared = TRUE;
    }

    if (vmm_device_tree_read_physaddr(rnode, VMM_DEVICE_TREE_GUEST_PHYS_ATTR_NAME, &guest_physical_addr)) {
        return FALSE;
    }

    if (is_real && !is_alloced && !is_colored && !is_shared) {
        if (vmm_device_tree_read_physaddr(rnode, VMM_DEVICE_TREE_HOST_PHYS_ATTR_NAME, &hphys_addr)) {
            return FALSE;
        }
    }

    if (is_alias) {
        if (vmm_device_tree_read_physaddr(rnode, VMM_DEVICE_TREE_ALIAS_PHYS_ATTR_NAME, &alias_physical_addr)) {
            return FALSE;
        }
    }

    if (vmm_device_tree_read_physsize(rnode, VMM_DEVICE_TREE_PHYS_SIZE_ATTR_NAME, &size)) {
        return FALSE;
    }

    if (vmm_device_tree_read_u32(rnode, VMM_DEVICE_TREE_FIRST_COLOR_ATTR_NAME, &first_color)) {
        first_color = 0;
    }

    if (vmm_device_tree_read_u32(rnode, VMM_DEVICE_TREE_NUM_COLORS_ATTR_NAME, &num_colors)) {
        num_colors = 0;
    }

    if (!vmm_device_tree_read_string(rnode, VMM_DEVICE_TREE_SHARED_MEM_ATTR_NAME, &aval)) {
        share_memory = vmm_share_memory_find_byname(aval);

        if (share_memory) {
            shm_available   = TRUE;
            shm_size        = vmm_share_memory_get_size(share_memory);
            shm_align_order = vmm_share_memory_get_align_order(share_memory);
            vmm_share_memory_dref(share_memory);
        }
    }

    if (is_colored) {
        if (!num_colors) {
            return FALSE;
        }

        if (vmm_host_ram_color_count() <= first_color) {
            return FALSE;
        }

        if (vmm_host_ram_color_count() < (first_color + num_colors)) {
            return FALSE;
        }
    }

    if (is_shared) {
        if (!shm_available) {
            return FALSE;
        }

        if (shm_size < size) {
            return FALSE;
        }
    }

    if (is_colored || is_shared) {
        if (is_colored) {
            align_order = vmm_host_ram_color_order();
        } else if (is_shared && shm_available) {
            align_order = shm_align_order;
        } else {
            return FALSE;
        }
    } else {
        if (vmm_device_tree_read_u32(rnode, VMM_DEVICE_TREE_ALIGN_ORDER_ATTR_NAME, &align_order)) {
            align_order = 0;
        }
    }

    if (BITS_PER_LONG <= align_order) {
        return FALSE;
    }

    if (size & order_mask(align_order)) {
        return FALSE;
    }

    if (guest_physical_addr & order_mask(align_order)) {
        return FALSE;
    }

    if (hphys_addr & order_mask(align_order)) {
        return FALSE;
    }

    if (alias_physical_addr & order_mask(align_order)) {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief 检查地址空间区域是否与现有区域重叠
 * @param guest 指向客户机结构体的指针
 * @param reg 寄存器值或索引
 * @param overlapping 重叠区域指针
 * @return 编号值
 */
static bool is_region_overlapping(struct vmm_guest *guest, struct vmm_region *reg, struct vmm_region **overlapping)
{
    bool                            ret = FALSE;
    irq_flags_t                     flags;
    vmm_rwlock_t                   *root_lock = NULL;
    red_black_root_t          *root      = NULL;
    red_black_node_t          *pos       = NULL;
    struct vmm_region              *treg      = NULL;
    struct vmm_guest_address_space *addr_space    = &guest->addr_space;

    if (reg->flags & VMM_REGION_IO) {
        root      = &addr_space->reg_iotree; /**< &addr_space->reg_iotree成员 */
        root_lock = &addr_space->reg_iotree_lock; /**< &addr_space->reg_iotree_lock成员 */
    } else {
        root      = &addr_space->reg_memtree;
        root_lock = &addr_space->reg_memory_tree_lock;
    }

    vmm_read_lock_irq_save_lite(root_lock, flags);

    pos = root->red_black_node;

    while (pos) {
        treg = rb_entry(pos, struct vmm_region, head);

        if (VMM_REGION_GPHYS_END(reg) <= VMM_REGION_GPHYS_START(treg)) {
            pos = pos->rb_left;
        } else if (VMM_REGION_GPHYS_END(treg) <= VMM_REGION_GPHYS_START(reg)) {
            pos = pos->rb_right;
        } else {
            if (overlapping) {
                *overlapping = treg;
            }

            ret = TRUE;
            break;
        }
    }

    vmm_read_unlock_irq_restore_lite(root_lock, flags);

    return ret;
}

/**
 * @brief 输出地址空间区域重叠的警告信息
 * @param func 函数名称字符串
 * @param guest 指向客户机结构体的指针
 * @param reg 寄存器值或索引
 * @param reg_overlap 重叠寄存器区域指针
 */
static void region_overlap_message(const char *func, struct vmm_guest *guest, struct vmm_region *reg, struct vmm_region *reg_overlap)
{
    const physical_size_t reg_size         = reg->guest_physical_addr + reg->phys_size;
    const physical_size_t overlap_reg_size = reg_overlap->guest_physical_addr + reg_overlap->phys_size;

    vmm_printf(
        "%s: Region for %s/%s (0x%" PRIPADDR " - 0x%" PRIPADDR ") "
        "overlaps with region %s/%s "
        "(0x%" PRIPADDR " - 0x%" PRIPADDR ")\n",
        func, guest->name, reg->node->name, reg->guest_physical_addr, reg_size, guest->name, reg_overlap->node->name,
        reg_overlap->guest_physical_addr, overlap_reg_size);
}

/**
 * @brief 向地址空间添加区域
 * @param guest 指向客户机结构体的指针
 * @param rnode 资源节点指针
 * @param new_reg 新寄存器区域指针
 * @param rprivate 私有资源数据指针
 * @param add_probe_list 是否添加到探测列表
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int region_add(struct vmm_guest *guest, vmm_device_tree_node_t *rnode, struct vmm_region **new_reg, void *rprivate, bool add_probe_list)
{
    uint32_t               i;
    int                    rc;
    const char            *aval;
    irq_flags_t            flags;
    vmm_rwlock_t          *root_lock  = NULL;
    double_list_t         *root_plist = NULL;
    red_black_root_t *root       = NULL;
    red_black_node_t **new = NULL;
    red_black_node_t *pnode = NULL;
    struct vmm_region *reg = NULL;
    struct vmm_region *pnode_reg = NULL;
    struct vmm_guest_address_space *addr_space      = &guest->addr_space;
    struct vmm_region              *reg_overlap = NULL;

    /* Increment ref count of region node */
    vmm_device_tree_ref_node(rnode);

    /* Sanity check on region node */
    if (!is_region_node_valid(rnode)) {
        rc = VMM_ERR_INVALID;
        goto region_fail;
    }

    /* Allocate region instance */
    reg = vmm_zalloc(sizeof(struct vmm_region));
    RB_CLEAR_NODE(&reg->head);
    INIT_LIST_HEAD(&reg->phead);

    /* Fillup region details */
    reg->node   = rnode;
    reg->addr_space = addr_space;
    reg->flags  = 0x0;

    /* Determine manifest_type */
    rc          = vmm_device_tree_read_string(reg->node, VMM_DEVICE_TREE_MANIFEST_TYPE_ATTR_NAME, &aval);

    if (rc) {
        goto region_free_fail;
    }

    /* Update region flags based on manifest_type */
    if (!strcmp(aval, VMM_DEVICE_TREE_MANIFEST_TYPE_VAL_REAL)) {
        reg->flags |= VMM_REGION_REAL;
    } else if (!strcmp(aval, VMM_DEVICE_TREE_MANIFEST_TYPE_VAL_ALIAS)) {
        reg->flags |= VMM_REGION_ALIAS;
    } else {
        reg->flags |= VMM_REGION_VIRTUAL;
    }

    /* Determine address_type */
    rc = vmm_device_tree_read_string(reg->node, VMM_DEVICE_TREE_ADDRESS_TYPE_ATTR_NAME, &aval);

    if (rc) {
        goto region_free_fail;
    }

    /* Update region flags based on address_type */
    if (!strcmp(aval, VMM_DEVICE_TREE_ADDRESS_TYPE_VAL_IO)) {
        reg->flags |= VMM_REGION_IO;
    } else {
        reg->flags |= VMM_REGION_MEMORY;
    }

    /* Determine device_type */
    rc = vmm_device_tree_read_string(reg->node, VMM_DEVICE_TREE_DEVICE_TYPE_ATTR_NAME, &aval);

    if (rc) {
        goto region_free_fail;
    }

    /* Update region flags based on device_type */
    if (!strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_RAM) || !strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_ALLOCED_RAM) ||
        !strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_COLORED_RAM) || !strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_SHARED_RAM)) {
        reg->flags |= VMM_REGION_IS_RAM;
    } else if (
        !strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_ROM) || !strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_ALLOCED_ROM) ||
        !strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_COLORED_ROM) || !strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_SHARED_ROM)) {
        reg->flags |= VMM_REGION_READONLY;
        reg->flags |= VMM_REGION_IS_ROM;
    } else {
        reg->flags |= VMM_REGION_IS_DEVICE;
    }

    if (!strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_RAM) || !strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_ROM)) {
        reg->flags |= VMM_REGION_IS_RESERVED;
    }

    if (!strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_ALLOCED_RAM) || !strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_ALLOCED_ROM)) {
        reg->flags |= VMM_REGION_IS_ALLOCED;
    }

    if (!strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_COLORED_RAM) || !strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_COLORED_ROM)) {
        reg->flags |= VMM_REGION_IS_COLORED;
    }

    if (!strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_SHARED_RAM) || !strcmp(aval, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_SHARED_ROM)) {
        reg->flags |= VMM_REGION_IS_SHARED;
    }

    if ((reg->flags & VMM_REGION_REAL) && (reg->flags & VMM_REGION_MEMORY) && (reg->flags & VMM_REGION_IS_RAM)) {
        reg->flags |= VMM_REGION_CACHEABLE;
        reg->flags |= VMM_REGION_BUFFERABLE;
    }

    /* Determine region guest physical address */
    rc = vmm_device_tree_read_physaddr(reg->node, VMM_DEVICE_TREE_GUEST_PHYS_ATTR_NAME, &reg->guest_physical_addr);

    if (rc) {
        goto region_free_fail;
    }

    /* Determine region alias physical address */
    if (reg->flags & VMM_REGION_ALIAS) {
        rc = vmm_device_tree_read_physaddr(reg->node, VMM_DEVICE_TREE_ALIAS_PHYS_ATTR_NAME, &reg->alias_physical_addr);

        if (rc) {
            goto region_free_fail;
        }
    } else {
        reg->alias_physical_addr = reg->guest_physical_addr;
    }

    /* Determine region size */
    rc = vmm_device_tree_read_physsize(reg->node, VMM_DEVICE_TREE_PHYS_SIZE_ATTR_NAME, &reg->phys_size);

    if (rc) {
        goto region_free_fail;
    }

    /* Determine region first_color */
    if (reg->flags & VMM_REGION_IS_COLORED) {
        rc = vmm_device_tree_read_u32(rnode, VMM_DEVICE_TREE_FIRST_COLOR_ATTR_NAME, &reg->first_color);

        if (rc) {
            goto region_free_fail;
        }
    } else {
        reg->first_color = 0;
    }

    /* Determine region num_colors */
    if (reg->flags & VMM_REGION_IS_COLORED) {
        rc = vmm_device_tree_read_u32(rnode, VMM_DEVICE_TREE_NUM_COLORS_ATTR_NAME, &reg->num_colors);

        if (rc) {
            goto region_free_fail;
        }
    } else {
        reg->num_colors = 0;
    }

    /* Determine region shared memory */
    if (reg->flags & VMM_REGION_IS_SHARED) {
        rc = vmm_device_tree_read_string(reg->node, VMM_DEVICE_TREE_SHARED_MEM_ATTR_NAME, &aval);

        if (rc) {
            goto region_free_fail;
        }

        reg->share_memory = vmm_share_memory_find_byname(aval);

        if (!reg->share_memory) {
            rc = VMM_ERR_INVALID;
            goto region_free_fail;
        }

        if (vmm_share_memory_get_size(reg->share_memory) < reg->phys_size) {
            rc = VMM_ERR_INVALID;
            goto region_dref_shm_fail;
        }
    } else {
        reg->share_memory = NULL;
    }

    /* Determine region align_order */
    if (reg->flags & (VMM_REGION_IS_COLORED | VMM_REGION_IS_SHARED)) {
        if (reg->flags & VMM_REGION_IS_COLORED) {
            reg->align_order = vmm_host_ram_color_order();
        } else if (reg->flags & VMM_REGION_IS_SHARED) {
            reg->align_order = vmm_share_memory_get_align_order(reg->share_memory);
        } else {
            rc = VMM_ERR_INVALID;
            goto region_dref_shm_fail;
        }
    } else {
        rc = vmm_device_tree_read_u32(reg->node, VMM_DEVICE_TREE_ALIGN_ORDER_ATTR_NAME, &reg->align_order);

        if (rc) {
            reg->align_order = 0;
        }
    }

    /* Compute default mapping order for guest region */
    reg->map_order = VMM_PAGE_SHIFT;

    for (i = VMM_PAGE_SHIFT; i < 64; i++) {
        if (reg->phys_size <= ((uint64_t)1 << i)) {
            reg->map_order = i;
            break;
        }
    }

    if (i == 64) {
        rc = VMM_ERR_INVALID;
        goto region_dref_shm_fail;
    }

    /*
     * Overwrite mapping order for alloced RAM/ROM regions
     * based on align_order or map_order DT attribute
     */
    if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) && (reg->flags & (VMM_REGION_IS_RAM | VMM_REGION_IS_ROM)) &&
        (reg->flags & VMM_REGION_IS_ALLOCED)) {
        if ((VMM_PAGE_SHIFT <= reg->align_order) && (reg->align_order < reg->map_order)) {
            reg->map_order = reg->align_order;
        }

        i  = 0;
        rc = vmm_device_tree_read_u32(reg->node, VMM_DEVICE_TREE_MAP_ORDER_ATTR_NAME, &i);

        if (!rc && (VMM_PAGE_SHIFT <= i)) {
            reg->map_order = i;
        }
    }

    /* Overwrite mapping order for colored RAM/ROM regions */
    if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) && (reg->flags & (VMM_REGION_IS_RAM | VMM_REGION_IS_ROM)) &&
        (reg->flags & VMM_REGION_IS_COLORED)) {
        reg->map_order = reg->align_order;
    }

    /* Compute number of mappings for guest region */
    reg->maps_count = reg->phys_size >> reg->map_order;

    if ((((physical_size_t)reg->maps_count) << reg->map_order) < reg->phys_size) {
        reg->maps_count++;
    }

    /* Allocate mappings for guest region */
    reg->maps = vmm_zalloc(sizeof(*reg->maps) * reg->maps_count);

    if (!reg->maps) {
        rc = VMM_ERR_NOMEM;
        goto region_dref_shm_fail;
    }

    reg->maps[0].hphys_addr = reg->guest_physical_addr + mapping_gphys_offset(reg, 0);
    reg->maps[0].flags      = 0;

    for (i = 1; i < reg->maps_count; i++) {
        reg->maps[i].hphys_addr = reg->guest_physical_addr + mapping_gphys_offset(reg, i);
        reg->maps[i].flags      = 0;
    }

    reg->device_emulate_private = NULL;
    reg->private                = rprivate;

    /* Ensure region does not overlap other regions */
    if (is_region_overlapping(guest, reg, &reg_overlap)) {
        region_overlap_message(__func__, guest, reg, reg_overlap);
        rc = VMM_ERR_INVALID;
        goto region_free_maps_fail;
    }

    /*
     * Mapping0 from device tree for
     * non-alloced non-colored non-shared real guest region
     */
    if ((reg->flags & VMM_REGION_REAL) && !(reg->flags & VMM_REGION_IS_ALLOCED) && !(reg->flags & VMM_REGION_IS_COLORED) &&
        !(reg->flags & VMM_REGION_IS_SHARED)) {
        rc = vmm_device_tree_read_physaddr(reg->node, VMM_DEVICE_TREE_HOST_PHYS_ATTR_NAME, &reg->maps[0].hphys_addr);

        if (rc) {
            goto region_free_maps_fail;
        }
    }

    /*
     * Mapping0 from shared memory instance for
     * shared RAM/ROM regions
     */
    if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) && (reg->flags & (VMM_REGION_IS_RAM | VMM_REGION_IS_ROM)) &&
        (reg->flags & VMM_REGION_IS_SHARED)) {
        reg->maps[0].hphys_addr = vmm_share_memory_get_addr(reg->share_memory);
    }

    /* Reserve host RAM for reserved RAM/ROM regions */
    if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) && (reg->flags & (VMM_REGION_IS_RAM | VMM_REGION_IS_ROM)) &&
        (reg->flags & VMM_REGION_IS_RESERVED)) {
        for (i = 0; i < reg->maps_count; i++) {
            rc = vmm_host_ram_reserve(reg->maps[i].hphys_addr, mapping_phys_size(reg, i));

            if (rc) {
                vmm_printf(
                    "%s: Failed to reserve "
                    "host RAM for %s/%s\n",
                    __func__, guest->name, reg->node->name);
                goto region_ram_free_fail;
            } else {
                reg->maps[i].flags |= VMM_REGION_MAPPING_ISHOSTRAM;
            }
        }
    }

    /* Allocate host RAM for alloced RAM/ROM regions */
    if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) && (reg->flags & (VMM_REGION_IS_RAM | VMM_REGION_IS_ROM)) &&
        (reg->flags & VMM_REGION_IS_ALLOCED)) {
        for (i = 0; i < reg->maps_count; i++) {
            if (!vmm_host_ram_alloc(&reg->maps[i].hphys_addr, mapping_phys_size(reg, i), reg->align_order)) {
                vmm_printf(
                    "%s: Failed to alloc "
                    "host RAM for %s/%s\n",
                    __func__, guest->name, reg->node->name);
                rc = VMM_ERR_NOMEM;
                goto region_ram_free_fail;
            } else {
                reg->maps[i].flags |= VMM_REGION_MAPPING_ISHOSTRAM;

                if (reg->flags & VMM_REGION_IS_ROM) {
                    vmm_host_memory_set(reg->maps[i].hphys_addr, 0, mapping_phys_size(reg, i), FALSE);
                }
            }
        }
    }

    /* Allocate host RAM for colored RAM/ROM regions */
    if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) && (reg->flags & (VMM_REGION_IS_RAM | VMM_REGION_IS_ROM)) &&
        (reg->flags & VMM_REGION_IS_COLORED)) {
        for (i = 0; i < reg->maps_count; i++) {
            if (!vmm_host_ram_color_alloc(&reg->maps[i].hphys_addr, reg->first_color + umod32(i, reg->num_colors))) {
                vmm_printf(
                    "%s: Failed to alloc "
                    "host RAM for %s/%s\n",
                    __func__, guest->name, reg->node->name);
                rc = VMM_ERR_NOMEM;
                goto region_ram_free_fail;
            } else {
                reg->maps[i].flags |= VMM_REGION_MAPPING_ISHOSTRAM;

                if (reg->flags & VMM_REGION_IS_ROM) {
                    vmm_host_memory_set(reg->maps[i].hphys_addr, 0, mapping_phys_size(reg, i), FALSE);
                }
            }
        }
    }

    /* Probe device emulation for real & virtual device regions */
    if ((reg->flags & VMM_REGION_IS_DEVICE) && !(reg->flags & VMM_REGION_ALIAS)) {
        if ((rc = vmm_device_emulate_probe_region(guest, reg))) {
            goto region_ram_free_fail;
        }
    }

    /* Call arch specific add region callback */
    rc = arch_guest_add_region(guest, reg);

    if (rc) {
        goto region_unprobe_fail;
    }

    /* Add region to tree and probe list */
    if (reg->flags & VMM_REGION_IO) {
        root       = &addr_space->reg_iotree;
        root_plist = &addr_space->reg_ioprobe_list;
        root_lock  = &addr_space->reg_iotree_lock;
    } else {
        root       = &addr_space->reg_memtree;
        root_plist = &addr_space->reg_memprobe_list;
        root_lock  = &addr_space->reg_memory_tree_lock;
    }

    vmm_write_lock_irq_save_lite(root_lock, flags);
    new = &root->red_black_node;

    while (*new) {
        pnode     = *new;
        pnode_reg = rb_entry(pnode, struct vmm_region, head);

        if (VMM_REGION_GPHYS_END(reg) <= VMM_REGION_GPHYS_START(pnode_reg)) {
            new = &pnode->rb_left;
        } else if (VMM_REGION_GPHYS_END(pnode_reg) <= VMM_REGION_GPHYS_START(reg)) {
            new = &pnode->rb_right;
        } else {
            rc = VMM_ERR_INVALID;
            vmm_write_unlock_irq_restore_lite(root_lock, flags);
            goto region_arch_del_fail;
        }
    }

    rb_link_node(&reg->head, pnode, new);
    rb_insert_color(&reg->head, root);

    if (add_probe_list) {
        list_add_tail(&reg->phead, root_plist);
    }

    vmm_write_unlock_irq_restore_lite(root_lock, flags);

    if (new_reg) {
        *new_reg = reg;
    }

    return VMM_OK;

region_arch_del_fail:
    arch_guest_del_region(guest, reg);
region_unprobe_fail:

    if ((reg->flags & VMM_REGION_IS_DEVICE) && !(reg->flags & VMM_REGION_ALIAS)) {
        vmm_device_emulate_remove_region(guest, reg);
    }

region_ram_free_fail:

    if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) && (reg->flags & (VMM_REGION_IS_RAM | VMM_REGION_IS_ROM))) {
        for (i = 0; i < reg->maps_count; i++) {
            if (!(reg->maps[i].flags & VMM_REGION_MAPPING_ISHOSTRAM)) {
                continue;
            }

            vmm_host_ram_free(reg->maps[i].hphys_addr, mapping_phys_size(reg, i));
            reg->maps[i].flags &= ~VMM_REGION_MAPPING_ISHOSTRAM;
        }
    }

region_free_maps_fail:
    vmm_free(reg->maps);
region_dref_shm_fail:

    if (reg->share_memory) {
        vmm_share_memory_dref(reg->share_memory);
        reg->share_memory = NULL;
    }

region_free_fail:
    vmm_free(reg);
region_fail:
    vmm_device_tree_dref_node(rnode);
    return rc;
}

/**
 * @brief 从地址空间删除区域
 * @param guest 指向客户机结构体的指针
 * @param reg 寄存器值或索引
 * @param del_reg_tree 是否删除寄存器树
 * @param del_probe_list 是否删除探测列表
 * @return 成功返回VMM_OK，失败返回错误码
 */
static int region_del(struct vmm_guest *guest, struct vmm_region *reg, bool del_reg_tree, bool del_probe_list)
{
    uint32_t                        i;
    int                             rc = VMM_OK;
    irq_flags_t                     flags;
    vmm_rwlock_t                   *root_lock;
    red_black_root_t          *root   = NULL;
    vmm_device_tree_node_t         *rnode  = reg->node;
    struct vmm_guest_address_space *addr_space = &guest->addr_space;

    /* Remove it from region tree if not removed already */
    if (del_reg_tree) {
        if (reg->flags & VMM_REGION_IO) {
            root      = &addr_space->reg_iotree; /**< &addr_space->reg_iotree成员 */
            root_lock = &addr_space->reg_iotree_lock; /**< &addr_space->reg_iotree_lock成员 */
        } else {
            root      = &addr_space->reg_memtree; /**< &addr_space->reg_memtree成员 */
            root_lock = &addr_space->reg_memory_tree_lock; /**< &addr_space->reg_memory_tree_lock成员 */
        }

        vmm_write_lock_irq_save_lite(root_lock, flags); /**< flags)成员 */
        rb_erase(&reg->head, root); /**< root)成员 */
        vmm_write_unlock_irq_restore_lite(root_lock, flags); /**< flags)成员 */
    }

    /* Remove it from probe list if not removed already */
    if (del_probe_list) {
        if (reg->flags & VMM_REGION_IO) {
            root_lock = &addr_space->reg_iotree_lock;
        } else {
            root_lock = &addr_space->reg_memory_tree_lock;
        }

        vmm_write_lock_irq_save_lite(root_lock, flags);
        list_del(&reg->phead);
        vmm_write_unlock_irq_restore_lite(root_lock, flags);
    }

    /* Call arch specific del region callback */
    rc = arch_guest_del_region(guest, reg);

    if (rc) {
        vmm_printf(
            "%s: arch_guest_del_region() failed for %s/%s "
            "(error %d)\n",
            __func__, guest->name, reg->node->name, rc);
    }

    /* Remove emulator for if virtual region */
    if ((reg->flags & VMM_REGION_IS_DEVICE) && !(reg->flags & VMM_REGION_ALIAS)) {
        vmm_device_emulate_remove_region(guest, reg);
    }

    /* Free host RAM if region has alloced/reserved host RAM */
    if (!(reg->flags & (VMM_REGION_ALIAS | VMM_REGION_VIRTUAL)) && (reg->flags & (VMM_REGION_IS_RAM | VMM_REGION_IS_ROM))) {
        for (i = 0; i < reg->maps_count; i++) {
            if (!(reg->maps[i].flags & VMM_REGION_MAPPING_ISHOSTRAM)) {
                continue;
            }

            rc = vmm_host_ram_free(reg->maps[i].hphys_addr, mapping_phys_size(reg, i));

            if (rc) {
                vmm_printf(
                    "%s: Failed to free host RAM "
                    "for %s/%s (error %d)\n",
                    __func__, guest->name, reg->node->name, rc);
            }

            reg->maps[i].flags &= ~VMM_REGION_MAPPING_ISHOSTRAM;
        }
    }

    /* Free region mappings */
    vmm_free(reg->maps);

    /* De-reference shared memory */
    if (reg->share_memory) {
        vmm_share_memory_dref(reg->share_memory);
        reg->share_memory = NULL;
    }

    /* Free the region */
    vmm_free(reg);

    /* De-reference the region node */
    vmm_device_tree_dref_node(rnode);

    return rc;
}

/**
 * @brief 复位客户地址空间
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_address_space_reset(struct vmm_guest *guest)
{
    irq_flags_t                          flags;
    vmm_rwlock_t                        *root_lock = NULL;
    red_black_root_t               *root      = NULL;
    struct vmm_guest_address_space      *addr_space;
    struct vmm_region *reg = NULL;
    struct vmm_region *next_reg = NULL;
    struct vmm_guest_address_space_event evt;

    /* Sanity Check */
    if (!guest) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    addr_space    = &guest->addr_space;

    /* Reset device emulation for io regions */
    root      = &addr_space->reg_iotree;
    root_lock = &addr_space->reg_iotree_lock;
    vmm_read_lock_irq_save_lite(root_lock, flags);
    red_black_tree_postorder_for_each_entry_safe(reg, next_reg, root, head)
    {
        vmm_read_unlock_irq_restore_lite(root_lock, flags);

        if ((reg->flags & VMM_REGION_IS_DEVICE) && !(reg->flags & VMM_REGION_ALIAS)) {
            vmm_device_emulate_reset_region(guest, reg);
        }

        vmm_read_lock_irq_save_lite(root_lock, flags);
    }
    vmm_read_unlock_irq_restore_lite(root_lock, flags);

    /* Reset device emulation for mem regions */
    root      = &addr_space->reg_memtree;
    root_lock = &addr_space->reg_memory_tree_lock;
    vmm_read_lock_irq_save_lite(root_lock, flags);
    red_black_tree_postorder_for_each_entry_safe(reg, next_reg, root, head)
    {
        vmm_read_unlock_irq_restore_lite(root_lock, flags);

        if ((reg->flags & VMM_REGION_IS_DEVICE) && !(reg->flags & VMM_REGION_ALIAS)) {
            vmm_device_emulate_reset_region(guest, reg);
        }

        vmm_read_lock_irq_save_lite(root_lock, flags);
    }
    vmm_read_unlock_irq_restore_lite(root_lock, flags);

    /*
     * Notify the listeners about reset event.
     * No locks taken at this point.
     */
    evt.guest = guest;
    evt.data  = NULL;
    vmm_blocking_notifier_call(&guest_address_space_notifier_chain, VMM_GUEST_ADDRESS_SPACE_EVENT_RESET, &evt);

    /* Reset device emulation context */
    return vmm_device_emulate_reset_context(guest);
}

/**
 * @brief 从设备树节点添加客户机地址空间区域
 * @param guest 指向客户机结构体的指针
 * @param node 设备树节点指针
 * @param rprivate 私有资源数据指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_add_region_from_node(struct vmm_guest *guest, vmm_device_tree_node_t *node, void *rprivate)
{
    int                rc;
    struct vmm_region *reg = NULL;

    /* Sanity checks */
    if (!guest || !guest->addr_space.node || !node) {
        return VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
    }

    /* TODO: Make sure addr_space node is not parent of given node */
    /* TODO: Make sure addr_space node is ancestor of given node */

    /* Add region */
    rc = region_add(guest, node, &reg, rprivate, FALSE);

    if (rc) {
        return rc;
    }

    /* Mark this region as dynamically added */
    reg->flags |= VMM_REGION_IS_DYNAMIC;

    return VMM_OK;
}

/**
 * @brief 客户机 添加 区域
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_add_region(
    struct vmm_guest *guest, vmm_device_tree_node_t *parent, const char *name, const char *device_type, const char *mainfest_type,
    const char *address_type, const char *compatible, uint32_t compatible_len, physical_addr_t guest_physical_addr, physical_addr_t alias_physical_addr,
    physical_size_t phys_size, uint32_t align_order, physical_addr_t hphys_addr, void *rprivate)
{
    int                     rc; /**< rc */
    struct vmm_region      *reg = NULL; /**< NULL成员 */
    vmm_device_tree_node_t *rnode; /**< rnode成员 */

    /* Sanity checks */
    if (!guest || !guest->addr_space.node || !parent || !name || !device_type || !mainfest_type || !address_type) {
        rc = VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
        goto failed; /**< failed成员 */
    }

    /* TODO: Make sure addr_space node is parent/ancestor of given
     * parent node
     */

    /* Create region node */
    rnode = vmm_device_tree_addnode(parent, name); /**< name)成员 */

    if (!rnode) {
        rc = VMM_ERR_INVALID; /**< VMM_ERR_INVALID成员 */
        goto failed; /**< failed成员 */
    }

    /* Set device type */
    rc = vmm_device_tree_setattr(
        rnode, VMM_DEVICE_TREE_DEVICE_TYPE_ATTR_NAME, (void *)device_type, VMM_DEVICE_TREE_ATTRTYPE_STRING, strlen(device_type) + 1, FALSE); /**< FALSE)成员 */

    if (rc) {
        goto failed_delnode; /**< failed_delnode成员 */
    }

    /* Set manifest type */
    rc = vmm_device_tree_setattr(
        rnode, VMM_DEVICE_TREE_MANIFEST_TYPE_ATTR_NAME, (void *)mainfest_type, VMM_DEVICE_TREE_ATTRTYPE_STRING, strlen(mainfest_type) + 1, FALSE); /**< FALSE)成员 */

    if (rc) {
        goto failed_delnode; /**< failed_delnode成员 */
    }

    /* Set address type */
    rc = vmm_device_tree_setattr(
        rnode, VMM_DEVICE_TREE_ADDRESS_TYPE_ATTR_NAME, (void *)address_type, VMM_DEVICE_TREE_ATTRTYPE_STRING, strlen(address_type) + 1, FALSE); /**< FALSE)成员 */

    if (rc) {
        goto failed_delnode; /**< failed_delnode成员 */
    }

    /* Set compatible */
    if (compatible) {
        rc = vmm_device_tree_setattr(
            rnode, VMM_DEVICE_TREE_COMPATIBLE_ATTR_NAME, (void *)compatible, VMM_DEVICE_TREE_ATTRTYPE_STRING, compatible_len, FALSE); /**< FALSE)成员 */

        if (rc) {
            goto failed_delnode; /**< failed_delnode成员 */
        }
    }

    /* Set guest physical address */
    rc = vmm_device_tree_setattr(
        rnode, VMM_DEVICE_TREE_GUEST_PHYS_ATTR_NAME, &guest_physical_addr, VMM_DEVICE_TREE_ATTRTYPE_PHYSADDR, sizeof(guest_physical_addr), FALSE); /**< FALSE)成员 */

    if (rc) {
        goto failed_delnode; /**< failed_delnode成员 */
    }

    if (!strcmp(mainfest_type, VMM_DEVICE_TREE_MANIFEST_TYPE_VAL_REAL)) {
        /* Set host physical address */
        rc = vmm_device_tree_setattr(
            rnode, VMM_DEVICE_TREE_HOST_PHYS_ATTR_NAME, &hphys_addr, VMM_DEVICE_TREE_ATTRTYPE_PHYSADDR, sizeof(hphys_addr), FALSE); /**< FALSE)成员 */

        if (rc) {
            goto failed_delnode; /**< failed_delnode成员 */
        }
    } else if (!strcmp(mainfest_type, VMM_DEVICE_TREE_MANIFEST_TYPE_VAL_ALIAS)) {
        /* Set alias physical address */
        rc = vmm_device_tree_setattr(
            rnode, VMM_DEVICE_TREE_ALIAS_PHYS_ATTR_NAME, &alias_physical_addr, VMM_DEVICE_TREE_ATTRTYPE_PHYSADDR, sizeof(alias_physical_addr), FALSE); /**< FALSE)成员 */

        if (rc) {
            goto failed_delnode; /**< failed_delnode成员 */
        }
    }

    /* Set physical size */
    rc = vmm_device_tree_setattr(rnode, VMM_DEVICE_TREE_PHYS_SIZE_ATTR_NAME, &phys_size, VMM_DEVICE_TREE_ATTRTYPE_PHYSSIZE, sizeof(phys_size), FALSE); /**< FALSE)成员 */

    if (rc) {
        goto failed_delnode; /**< failed_delnode成员 */
    }

    /* Set alignment order */
    rc = vmm_device_tree_setattr(
        rnode, VMM_DEVICE_TREE_ALIGN_ORDER_ATTR_NAME, &align_order, VMM_DEVICE_TREE_ATTRTYPE_UINT32, sizeof(align_order), FALSE); /**< FALSE)成员 */

    if (rc) {
        goto failed_delnode; /**< failed_delnode成员 */
    }

    /* Add region */
    rc = region_add(guest, rnode, &reg, rprivate, FALSE); /**< FALSE)成员 */

    if (rc) {
        goto failed_delnode; /**< failed_delnode成员 */
    }

    /* Mark this region as dynamically added */
    reg->flags |= VMM_REGION_IS_DYNAMIC; /**< VMM_REGION_IS_DYNAMIC成员 */

    return VMM_OK; /**< VMM_OK成员 */

failed_delnode:
    vmm_device_tree_delnode(rnode);
failed:
    return rc; /**< rc */
}

/**
 * @brief 删除客户机地址空间中的指定区域
 * @param guest 指向客户机结构体的指针
 * @param reg 寄存器值或索引
 * @param del_node 待删除的节点指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_del_region(struct vmm_guest *guest, struct vmm_region *reg, bool del_node)
{
    int                     rc;
    vmm_device_tree_node_t *rnode;

    /* Sanity checks */
    if (!guest || !reg || !reg->node) {
        return VMM_ERR_INVALID;
    }

    if (reg->addr_space->guest != guest) {
        return VMM_ERR_INVALID;
    }

    if (!(reg->flags & VMM_REGION_IS_DYNAMIC)) {
        return VMM_ERR_INVALID;
    }

    rnode = reg->node;

    /* Delete region */
    rc    = region_del(guest, reg, TRUE, FALSE);

    if (rc) {
        return rc;
    }

    /* Delete region node if required */
    if (del_node) {
        vmm_device_tree_delnode(rnode);
    }

    return VMM_OK;
}

/**
 * @brief 初始化客户地址空间
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_address_space_init(struct vmm_guest *guest)
{
    int                                  rc;
    struct vmm_guest_address_space      *addr_space;
    vmm_device_tree_node_t              *rnode = NULL;
    struct vmm_guest_address_space_event evt;

    /* Sanity Check */
    if (!guest) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    if (guest->addr_space.initialized) {
        return VMM_ERR_INVALID;
    }

    addr_space = &guest->addr_space;

    /* Reset the address space for guest */
    memset(addr_space, 0, sizeof(struct vmm_guest_address_space));

    /* Initialize address space of guest */
    addr_space->node = vmm_device_tree_getchild(guest->node, VMM_DEVICE_TREE_ADDRSPACE_NODE_NAME);

    if (!addr_space->node) {
        vmm_printf("%s: %s/addr_space node not found\n", __func__, guest->name);
        return VMM_ERR_FAIL;
    }

    addr_space->guest = guest;
    INIT_RW_LOCK(&addr_space->reg_iotree_lock);
    addr_space->reg_iotree = RB_ROOT;
    INIT_LIST_HEAD(&addr_space->reg_ioprobe_list);
    INIT_RW_LOCK(&addr_space->reg_memory_tree_lock);
    addr_space->reg_memtree = RB_ROOT;
    INIT_LIST_HEAD(&addr_space->reg_memprobe_list);
    guest->addr_space.device_emulate_private = NULL;

    /* Initialize device emulation context */
    if ((rc = vmm_device_emulate_init_context(guest))) {
        return rc;
    }

    /* Create regions */
    vmm_device_tree_for_each_child(rnode, addr_space->node)
    {
        rc = region_add(guest, rnode, NULL, NULL, TRUE);

        if (rc) {
            vmm_device_tree_dref_node(rnode);
            return rc;
        }
    }

    /* Mark address space as initialized */
    addr_space->initialized = TRUE;

    /*
     * Notify the listeners that init is complete.
     * No locks taken at this point.
     */
    evt.guest           = guest;
    evt.data            = NULL;
    vmm_blocking_notifier_call(&guest_address_space_notifier_chain, VMM_GUEST_ADDRESS_SPACE_EVENT_INIT, &evt);

    return VMM_OK;
}

/**
 * @brief 反初始化客户机地址空间
 * @param guest 指向客户机结构体的指针
 * @return 成功返回VMM_OK，失败返回错误码
 */
int vmm_guest_address_space_deinit(struct vmm_guest *guest)
{
    int                                  rc;
    irq_flags_t                          flags;
    vmm_rwlock_t                        *root_lock;
    red_black_root_t               *root;
    double_list_t                       *root_plist;
    struct vmm_guest_address_space      *addr_space;
    struct vmm_region                   *reg = NULL;
    struct vmm_guest_address_space_event evt;

    /* Sanity Check */
    if (!guest) {
        return VMM_ERR_FAIL; /**< VMM_ERR_FAIL成员 */
    }

    addr_space    = &guest->addr_space;

    /*
     * About to deinit the guest address space. Regions
     * are still valid. Handler should take care of
     * internal locking, none taken at this point.
     */
    evt.guest = guest;
    evt.data  = NULL;
    vmm_blocking_notifier_call(&guest_address_space_notifier_chain, VMM_GUEST_ADDRESS_SPACE_EVENT_DEINIT, &evt);

    /* Mark address space as uninitialized */
    addr_space->initialized = FALSE;

    /* One-by-one remove all io regions in reverse probing order */
    root                = &addr_space->reg_iotree;
    root_plist          = &addr_space->reg_ioprobe_list;
    root_lock           = &addr_space->reg_iotree_lock;
    vmm_write_lock_irq_save_lite(root_lock, flags);

    while (!list_empty(root_plist)) {
        /* Get last region from probe list */
        reg = list_entry(list_pop_tail(root_plist), struct vmm_region, phead);

        /* Remove region from tree */
        rb_erase(&reg->head, root);

        /* Delete the region */
        vmm_write_unlock_irq_restore_lite(root_lock, flags);
        region_del(guest, reg, FALSE, FALSE);
        vmm_write_lock_irq_save_lite(root_lock, flags);
    }

    *root = RB_ROOT;
    vmm_write_unlock_irq_restore_lite(root_lock, flags);

    /* One-by-one remove all mem regions in reverse probing order */
    root       = &addr_space->reg_memtree;
    root_plist = &addr_space->reg_memprobe_list;
    root_lock  = &addr_space->reg_memory_tree_lock;
    vmm_write_lock_irq_save_lite(root_lock, flags);

    while (!list_empty(root_plist)) {
        /* Get last region from probe list */
        reg = list_entry(list_pop_tail(root_plist), struct vmm_region, phead);

        /* Remove region from tree */
        rb_erase(&reg->head, root);

        /* Delete the region */
        vmm_write_unlock_irq_restore_lite(root_lock, flags);
        region_del(guest, reg, FALSE, FALSE);
        vmm_write_lock_irq_save_lite(root_lock, flags);
    }

    *root = RB_ROOT;
    vmm_write_unlock_irq_restore_lite(root_lock, flags);

    /* DeInitialize device emulation context */
    if ((rc = vmm_device_emulate_deinit_context(guest))) {
        return rc;
    }

    guest->addr_space.device_emulate_private = NULL;

    /* De-reference address space node */
    if (guest->addr_space.node) {
        vmm_device_tree_dref_node(guest->addr_space.node);
        guest->addr_space.node = NULL;
    }

    return VMM_OK;
}
