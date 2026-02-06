/**
 * Copyright (c) 2010 Anup Patel.
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file vmm_device_tree.h
 * @author Anup Patel (anup@brainfault.org)
 * @author Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * @brief Device Tree Header File.
 */
#ifndef __VMM_DEVICE_TREE_H_
#define __VMM_DEVICE_TREE_H_

#include <libs/list.h>
#include <libs/xref.h>
#include <vmm_compiler.h>
#include <vmm_limits.h>
#include <vmm_spinlocks.h>
#include <vmm_types.h>

#define VMM_DEVICE_TREE_PATH_SEPARATOR              '/'
#define VMM_DEVICE_TREE_PATH_SEPARATOR_STRING       "/"

#define VMM_DEVICE_TREE_MODEL_ATTR_NAME             "model"
#define VMM_DEVICE_TREE_DEVICE_TYPE_ATTR_NAME       "device_type"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_CPU         "cpu"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_GUEST       "guest"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_VCPU        "vcpu"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_RAM         "ram"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_ALLOCED_RAM "alloced_ram"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_COLORED_RAM "colored_ram"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_SHARED_RAM  "shared_ram"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_ROM         "rom"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_ALLOCED_ROM "alloced_rom"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_COLORED_ROM "colored_rom"
#define VMM_DEVICE_TREE_DEVICE_TYPE_VAL_SHARED_ROM  "shared_rom"
#define VMM_DEVICE_TREE_COMPATIBLE_ATTR_NAME        "compatible"
#define VMM_DEVICE_TREE_DMA_COHERENT_ATTR_NAME      "dma-coherent"
#define VMM_DEVICE_TREE_CLOCK_FREQ_ATTR_NAME        "clock-frequency"
#define VMM_DEVICE_TREE_CLOCKS_ATTR_NAME            "clocks"
#define VMM_DEVICE_TREE_CLOCK_NAMES_ATTR_NAME       "clock-names"
#define VMM_DEVICE_TREE_CLOCK_OUT_NAMES_ATTR_NAME   "clock-output-names"
#define VMM_DEVICE_TREE_REG_ATTR_NAME               "reg"
#define VMM_DEVICE_TREE_REG_NAMES_ATTR_NAME         "reg-names"
#define VMM_DEVICE_TREE_VIRTUAL_REG_ATTR_NAME       "virtual-reg"
#define VMM_DEVICE_TREE_RANGES_ATTR_NAME            "ranges"
#define VMM_DEVICE_TREE_BIG_ENDIAN_ATTR_NAME        "big-endian"
#define VMM_DEVICE_TREE_NATIVE_ENDIAN_ATTR_NAME     "native-endian"
#define VMM_DEVICE_TREE_ADDR_CELLS_ATTR_NAME        "#address-cells"
#define VMM_DEVICE_TREE_SIZE_CELLS_ATTR_NAME        "#size-cells"
#define VMM_DEVICE_TREE_PHANDLE_ATTR_NAME           "phandle"

#define VMM_DEVICE_TREE_DEBUG_ATTR_NAME             "debug"

#define VMM_DEVICE_TREE_CHOSEN_NODE_NAME            "chosen"
#define VMM_DEVICE_TREE_CONSOLE_ATTR_NAME           "console"
#define VMM_DEVICE_TREE_STDOUT_ATTR_NAME            "stdout-path"
#define VMM_DEVICE_TREE_RTCDEV_ATTR_NAME            "rtcdev"
#define VMM_DEVICE_TREE_BOOTARGS_ATTR_NAME          "bootargs"
#define VMM_DEVICE_TREE_BOOTCMD_ATTR_NAME           "bootcmd"

#define VMM_DEVICE_TREE_ALIASES_NODE_NAME           "aliases"

#define VMM_DEVICE_TREE_VMMINFO_NODE_NAME           "vmm"
#define VMM_DEVICE_TREE_VMMNET_NODE_NAME            "net"
#define VMM_DEVICE_TREE_NETSTACK_NODE_NAME          "hoststack"

#define VMM_DEVICE_TREE_MEMORY_NODE_NAME            "memory"
#define VMM_DEVICE_TREE_MEMORY_PHYS_ADDR_ATTR_NAME  "physical_addr"
#define VMM_DEVICE_TREE_MEMORY_PHYS_SIZE_ATTR_NAME  "physical_size"

#define VMM_DEVICE_TREE_RESERVED_MEMORY_NODE_NAME   "reserved-memory"

#define VMM_DEVICE_TREE_CPUS_NODE_NAME              "cpus"
#define VMM_DEVICE_TREE_INTERRUPTS_ATTR_NAME        "interrupts"
#define VMM_DEVICE_TREE_INTERRUPT_CNTRL_ATTR_NAME   "interrupt-controller"
#define VMM_DEVICE_TREE_ENABLE_METHOD_ATTR_NAME     "enable-method"
#define VMM_DEVICE_TREE_CPU_CLEAR_ADDR_ATTR_NAME    "cpu-clear-addr"
#define VMM_DEVICE_TREE_CPU_RELEASE_ADDR_ATTR_NAME  "cpu-release-addr"

#define VMM_DEVICE_TREE_GUESTINFO_NODE_NAME         "guests"
#define VMM_DEVICE_TREE_VCPUS_NODE_NAME             "vcpus"
#define VMM_DEVICE_TREE_VCPU_TEMPLATE_NODE_NAME     "vcpu_template"
#define VMM_DEVICE_TREE_ENDIANNESS_ATTR_NAME        "endianness"
#define VMM_DEVICE_TREE_ENDIANNESS_VAL_BIG          "big"
#define VMM_DEVICE_TREE_ENDIANNESS_VAL_LITTLE       "little"
#define VMM_DEVICE_TREE_START_PC_ATTR_NAME          "start_pc"
#define VMM_DEVICE_TREE_PRIORITY_ATTR_NAME          "priority"
#define VMM_DEVICE_TREE_TIME_SLICE_ATTR_NAME        "time_slice"
#define VMM_DEVICE_TREE_DEADLINE_ATTR_NAME          "deadline"
#define VMM_DEVICE_TREE_PERIODICITY_ATTR_NAME       "periodicity"
#define VMM_DEVICE_TREE_ADDRSPACE_NODE_NAME         "aspace"
#define VMM_DEVICE_TREE_GUESTIRQCNT_ATTR_NAME       "guest_irq_count"
#define VMM_DEVICE_TREE_MANIFEST_TYPE_ATTR_NAME     "manifest_type"
#define VMM_DEVICE_TREE_MANIFEST_TYPE_VAL_REAL      "real"
#define VMM_DEVICE_TREE_MANIFEST_TYPE_VAL_VIRTUAL   "virtual"
#define VMM_DEVICE_TREE_MANIFEST_TYPE_VAL_ALIAS     "alias"
#define VMM_DEVICE_TREE_ADDRESS_TYPE_ATTR_NAME      "address_type"
#define VMM_DEVICE_TREE_ADDRESS_TYPE_VAL_MEMORY     "memory"
#define VMM_DEVICE_TREE_ADDRESS_TYPE_VAL_IO         "io"
#define VMM_DEVICE_TREE_GUEST_PHYS_ATTR_NAME        "guest_physical_addr"
#define VMM_DEVICE_TREE_HOST_PHYS_ATTR_NAME         "host_physical_addr"
#define VMM_DEVICE_TREE_ALIAS_PHYS_ATTR_NAME        "alias_physical_addr"
#define VMM_DEVICE_TREE_PHYS_SIZE_ATTR_NAME         "physical_size"
#define VMM_DEVICE_TREE_ALIGN_ORDER_ATTR_NAME       "align_order"
#define VMM_DEVICE_TREE_FIRST_COLOR_ATTR_NAME       "first_color"
#define VMM_DEVICE_TREE_NUM_COLORS_ATTR_NAME        "num_colors"
#define VMM_DEVICE_TREE_SHARED_MEM_ATTR_NAME        "shared_mem"
#define VMM_DEVICE_TREE_MAP_ORDER_ATTR_NAME         "map_order"
#define VMM_DEVICE_TREE_SWITCH_ATTR_NAME            "switch"
#define VMM_DEVICE_TREE_DOMAIN_ATTR_NAME            "domain"
#define VMM_DEVICE_TREE_NODE_ADDR_ATTR_NAME         "node_addr"
#define VMM_DEVICE_TREE_NODE_NS_NAME_ATTR_NAME      "node_ns_name"
#define VMM_DEVICE_TREE_BLKDEV_ATTR_NAME            "blkdev"
#define VMM_DEVICE_TREE_VCPU_AFFINITY_ATTR_NAME     "affinity"
#define VMM_DEVICE_TREE_VCPU_POWEROFF_ATTR_NAME     "poweroff"
#define VMM_DEVICE_TREE_NO_CHILD_PROBE_ATTR_NAME    "no-child-probe"
#define VMM_DEVICE_TREE_THREADS_AFFINITY_ATTR_NAME  "threads_affinity"

enum vmm_device_tree_attrypes {
    VMM_DEVICE_TREE_ATTRTYPE_UINT32    = 0,
    VMM_DEVICE_TREE_ATTRTYPE_UINT64    = 1,
    VMM_DEVICE_TREE_ATTRTYPE_VIRTADDR  = 2,
    VMM_DEVICE_TREE_ATTRTYPE_VIRTSIZE  = 3,
    VMM_DEVICE_TREE_ATTRTYPE_PHYSADDR  = 4,
    VMM_DEVICE_TREE_ATTRTYPE_PHYSSIZE  = 5,
    VMM_DEVICE_TREE_ATTRTYPE_STRING    = 6,
    VMM_DEVICE_TREE_ATTRTYPE_BYTEARRAY = 7,
    VMM_DEVICE_TREE_MAX_ATTRTYPE       = 8
};

struct vmm_device_tree_attr {
    double_list_t head;
    char          name[VMM_FIELD_SHORT_NAME_SIZE];
    uint32_t      type;
    void         *value;
    uint32_t      len;
};

struct vmm_device_tree_nodeid {
    char        name[VMM_FIELD_SHORT_NAME_SIZE];
    char        type[VMM_FIELD_TYPE_SIZE];
    char        compatible[VMM_FIELD_COMPAT_SIZE];
    const void *data;
};

#define VMM_DEVICE_TREE_NIDTBL_SIGNATURE 0xDEADF001

struct vmm_device_tree_nidtable_entry {
    uint32_t                      signature;
    char                          subsys[VMM_FIELD_SHORT_NAME_SIZE];
    struct vmm_device_tree_nodeid nodeid;
};

#ifndef __VMM_MODULES__

#define VMM_DEVICE_TREE_NIDTBL_ENTRY(nid, _subsys, _name, _type, _compat, _data) \
    __nidtbl struct vmm_device_tree_nidtable_entry __##nid = {                   \
        .signature         = VMM_DEVICE_TREE_NIDTBL_SIGNATURE,                   \
        .subsys            = (_subsys),                                          \
        .nodeid.name       = (_name),                                            \
        .nodeid.type       = (_type),                                            \
        .nodeid.compatible = (_compat),                                          \
        .nodeid.data       = (_data),                                            \
    }

#else

/**
 * TODO: NodeID table enteries cannot be created from runtime pluggable
 * modules. This will be added in future because vmm_modules needs to be
 * updated to support it.
 */
#define VMM_DEVICE_TREE_NIDTBL_ENTRY(nid, _subsys, _name, _type, _compat, _data)

#endif

struct vmm_device_tree_node {
    /* Private fields */
    double_list_t                head;
    vmm_rwlock_t                 attr_lock;
    double_list_t                attr_list;
    vmm_rwlock_t                 child_lock;
    double_list_t                child_list;
    struct xref                  ref_count;
    /* Public fields */
    char                         name[VMM_FIELD_SHORT_NAME_SIZE];
    struct vmm_device_tree_node *parent;
    void                        *system_data; /* System data pointer
                                         (Arch. specific code can use this to
                                          pass inforation to device driver) */
    void *private;                            /* Generic Private pointer */
};

typedef struct vmm_device_tree_node vmm_device_tree_node_t;

#define VMM_MAX_PHANDLE_ARGS 8

struct vmm_device_tree_phandle_args {
    vmm_device_tree_node_t *np;
    int                     args_count;
    uint32_t                args[VMM_MAX_PHANDLE_ARGS];
};

/** Check whether given attribute type is literal or literal list
 *  NOTE: literal means 32-bit or 64-bit number
 */
bool vmm_device_tree_isliteral(uint32_t attrtype);

/** Get size of literal corresponding to attribute type */
uint32_t vmm_device_tree_literal_size(uint32_t attrtype);

/** Estimate type of attribute from its name */
uint32_t vmm_device_tree_estimate_attrtype(const char *name);

/** Get attribute value */
const void *vmm_device_tree_attrval(const vmm_device_tree_node_t *node, const char *attrib);

/** Get length of attribute value */
uint32_t vmm_device_tree_attrlen(const vmm_device_tree_node_t *node, const char *attrib);

/** Check if a device tree node have any attribute */
bool vmm_device_tree_have_attr(const vmm_device_tree_node_t *node);

/** Get next attribute of a device tree node */
struct vmm_device_tree_attr *vmm_device_tree_next_attr(const vmm_device_tree_node_t *node, struct vmm_device_tree_attr *current);

/** Itreate over each attribute of a device tree node */
#define vmm_device_tree_for_each_attr(attr, node) \
    for (attr = vmm_device_tree_next_attr(node, NULL); attr; attr = vmm_device_tree_next_attr(node, attr))

/** Set an attribute for a device tree node */
int vmm_device_tree_setattr(vmm_device_tree_node_t *node, const char *name, void *value, uint32_t type, uint32_t len, bool value_is_be);

/** Get an attribute from a device tree node */
struct vmm_device_tree_attr *vmm_device_tree_getattr(const vmm_device_tree_node_t *node, const char *name);

/** Delete an attribute from a device tree node */
int vmm_device_tree_delattr(vmm_device_tree_node_t *node, const char *name);

/** Read uint8_t from attribute at particular index */
int vmm_device_tree_read_u8_atindex(const vmm_device_tree_node_t *node, const char *attrib, uint8_t *out, int index);

/** Read an array of uint8_t from attribute */
int vmm_device_tree_read_u8_array(const vmm_device_tree_node_t *node, const char *attrib, uint8_t *out, size_t size);

/** Read uint8_t from attribute */
static inline int vmm_device_tree_read_u8(const vmm_device_tree_node_t *node, const char *attrib, uint8_t *out)
{
    return vmm_device_tree_read_u8_array(node, attrib, out, 1);
}

/** Read uint16_t from attribute at particular index */
int vmm_device_tree_read_u16_atindex(const vmm_device_tree_node_t *node, const char *attrib, uint16_t *out, int index);

/** Read an array of uint16_t from attribute */
int vmm_device_tree_read_u16_array(const vmm_device_tree_node_t *node, const char *attrib, uint16_t *out, size_t size);

/** Read uint16_t from attribute */
static inline int vmm_device_tree_read_u16(const vmm_device_tree_node_t *node, const char *attrib, uint16_t *out)
{
    return vmm_device_tree_read_u16_array(node, attrib, out, 1);
}

/** Read uint32_t from attribute at particular index */
int vmm_device_tree_read_u32_atindex(const vmm_device_tree_node_t *node, const char *attrib, uint32_t *out, int index);

/** Read an array of uint32_t from attribute */
int vmm_device_tree_read_u32_array(const vmm_device_tree_node_t *node, const char *attrib, uint32_t *out, size_t size);

/** Read uint32_t from attribute */
static inline int vmm_device_tree_read_u32(const vmm_device_tree_node_t *node, const char *attrib, uint32_t *out)
{
    return vmm_device_tree_read_u32_array(node, attrib, out, 1);
}

/** Read uint64_t from attribute at particular index */
int vmm_device_tree_read_u64_atindex(const vmm_device_tree_node_t *node, const char *attrib, uint64_t *out, int index);

/** Read an array of uint64_t from attribute */
int vmm_device_tree_read_u64_array(const vmm_device_tree_node_t *node, const char *attrib, uint64_t *out, size_t size);

/** Read uint64_t from attribute */
static inline int vmm_device_tree_read_u64(const vmm_device_tree_node_t *node, const char *attrib, uint64_t *out)
{
    return vmm_device_tree_read_u64_array(node, attrib, out, 1);
}

/** Read physical address from attribute at particular index */
int vmm_device_tree_read_physaddr_atindex(const vmm_device_tree_node_t *node, const char *attrib, physical_addr_t *out, int index);

/** Read an array of physical address from attribute */
int vmm_device_tree_read_physaddr_array(const vmm_device_tree_node_t *node, const char *attrib, physical_addr_t *out, size_t size);

/** Read physical address from attribute */
static inline int vmm_device_tree_read_physaddr(const vmm_device_tree_node_t *node, const char *attrib, physical_addr_t *out)
{
    return vmm_device_tree_read_physaddr_array(node, attrib, out, 1);
}

/** Read physical size from attribute at particular index */
int vmm_device_tree_read_physsize_atindex(const vmm_device_tree_node_t *node, const char *attrib, physical_size_t *out, int index);

/** Read an array of physical size from attribute */
int vmm_device_tree_read_physsize_array(const vmm_device_tree_node_t *node, const char *attrib, physical_size_t *out, size_t size);

/** Read physical size from attribute */
static inline int vmm_device_tree_read_physsize(const vmm_device_tree_node_t *node, const char *attrib, physical_size_t *out)
{
    return vmm_device_tree_read_physsize_array(node, attrib, out, 1);
}

/** Read virtual address from attribute at particular index */
int vmm_device_tree_read_virtaddr_atindex(const vmm_device_tree_node_t *node, const char *attrib, virtual_addr_t *out, int index);

/** Read an array of virtual address from attribute */
int vmm_device_tree_read_virtaddr_array(const vmm_device_tree_node_t *node, const char *attrib, virtual_addr_t *out, size_t size);

/** Read virtual address from attribute */
static inline int vmm_device_tree_read_virtaddr(const vmm_device_tree_node_t *node, const char *attrib, virtual_addr_t *out)
{
    return vmm_device_tree_read_virtaddr_array(node, attrib, out, 1);
}

/** Read virtual size from attribute at particular index */
int vmm_device_tree_read_virtsize_atindex(const vmm_device_tree_node_t *node, const char *attrib, virtual_size_t *out, int index);

/** Read an array of virtual size from attribute */
int vmm_device_tree_read_virtsize_array(const vmm_device_tree_node_t *node, const char *attrib, virtual_size_t *out, size_t size);

/** Read virtual size from attribute */
static inline int vmm_device_tree_read_virtsize(const vmm_device_tree_node_t *node, const char *attrib, virtual_size_t *out)
{
    return vmm_device_tree_read_virtsize_array(node, attrib, out, 1);
}

/** Read string from attribute */
int vmm_device_tree_read_string(const vmm_device_tree_node_t *node, const char *attrib, const char **out);

/** Find string in a list and return index
 *
 *  This function searches a string list property and returns the index
 *  of a specific string value.
 */
int vmm_device_tree_match_string(vmm_device_tree_node_t *node, const char *attrib, const char *string);

/** Find and return the number of strings from a multiple strings property.
 *
 *  Search for a attribute in a device tree node and retrieve the number
 *  of null terminated string contain in it. Returns the number of strings
 *  on success, VMM_EINVALID if the property does not exist, VMM_ENODATA
 *  if property does not have a value, and VMM_EILSEQ if the string is not
 *  null-terminated within the length of the property data.
 */
int vmm_device_tree_count_strings(vmm_device_tree_node_t *node, const char *attrib);

/** Retrive string in a list based on index
 *
 *  Returns size of string (0 <=) upon success and VMM_Exxxx (< 0)
 *  upon failure
 */
int vmm_device_tree_string_index(vmm_device_tree_node_t *node, const char *attrib, int index, const char **out);

/** Retrive the next uint32_t value.
 *
 *  Returns NULL when uint32_t is not available.
 */
const uint32_t *vmm_device_tree_next_u32(struct vmm_device_tree_attr *attr, const uint32_t *cur, uint32_t *val);

/** Retrive the next string.
 *
 *  Returns NULL when string is not available.
 */
const char *vmm_device_tree_next_string(struct vmm_device_tree_attr *attr, const char *cur);

#define vmm_device_tree_for_each_u32(np, attrname, attr, p, u) \
    for (attr = vmm_device_tree_getattr(np, attrname), p = vmm_device_tree_next_u32(attr, NULL, &u); p; p = vmm_device_tree_next_u32(attr, p, &u))

#define vmm_device_tree_for_each_string(np, attrname, attr, s) \
    for (attr = vmm_device_tree_getattr(np, attrname), s = vmm_device_tree_next_string(attr, NULL); s; s = vmm_device_tree_next_string(attr, s))

/** Create a path string for a given node */
int vmm_device_tree_getpath(char *out, size_t out_len, const vmm_device_tree_node_t *node);

/** Get child node below a given node
 *  NOTE: The returned node will have increased refrence count
 */
vmm_device_tree_node_t *vmm_device_tree_getchild(vmm_device_tree_node_t *node, const char *path);

/** Get node corresponding to a path string
 *  NOTE: If path == NULL then root node will be returned
 *  NOTE: The returned node will have increased refrence count
 */
vmm_device_tree_node_t *vmm_device_tree_getnode(const char *path);

/** Match a node with nodeid table
 *  Returns NULL if node does not match otherwise nodeid table entry
 */
const struct vmm_device_tree_nodeid *vmm_device_tree_match_node(const struct vmm_device_tree_nodeid *matches, const vmm_device_tree_node_t *node);

/** Find node matching nodeid table starting from given node
 *  NOTE: If node == NULL then node == root
 *  NOTE: The returned node will have increased refrence count
 */
vmm_device_tree_node_t *vmm_device_tree_find_matching(vmm_device_tree_node_t *node, const struct vmm_device_tree_nodeid *matches);

/** Iterate over all matching nodes
 *  NOTE: If node == NULL then node == root
 */
void vmm_device_tree_iterate_matching(
    vmm_device_tree_node_t *node, const struct vmm_device_tree_nodeid                                         *matches,
    void (*found)(vmm_device_tree_node_t *node, const struct vmm_device_tree_nodeid *match, void *data), void *found_data);

/** Find compatible node starting from given node
 *  NOTE: If node == NULL then node == root
 *  NOTE: The returned node will have increased refrence count
 */
vmm_device_tree_node_t *vmm_device_tree_find_compatible(vmm_device_tree_node_t *node, const char *device_type, const char *compatible);

/** Check if node is compatible to given compatibility string */
bool vmm_device_tree_is_compatible(const vmm_device_tree_node_t *node, const char *compatible);

/** Find a node with given phandle value
 *  NOTE: This is based on 'phandle' attributes of device tree node
 *  NOTE: The returned node will have increased refrence count
 */
vmm_device_tree_node_t *vmm_device_tree_find_node_by_phandle(uint32_t phandle);

/** Resolve a phandle property to a vmm_device_tree_node pointer
 *  NOTE: The returned node will have increased refrence count
 */
vmm_device_tree_node_t *vmm_device_tree_parse_phandle(const vmm_device_tree_node_t *node, const char *phandle_name, int index);

/** Find a node pointed by phandle in a list
 *
 *  This function is useful to parse lists of phandles and their arguments.
 *  Returns VMM_OK on success and fills out (i.e. args), on error returns
 *  appropriate errno value.
 *
 *  Example:
 *
 *  phandle1: node1 {
 *      #list-cells = <2>;
 *  }
 *
 *  phandle2: node2 {
 *  #list-cells = <1>;
 *  }
 *
 *  node3 {
 *  list = <&phandle1 1 2 &phandle2 3>;
 *  }
 *
 *  To get a device_node of the `node2' node you may call this:
 *  vmm_device_tree_parse_phandle_with_args(node3, "list", "#list-cells", 1, &out);
 *
 *  NOTE: The returned nodes will have increased refrence count
 */
int vmm_device_tree_parse_phandle_with_args(
    const vmm_device_tree_node_t *node, const char *list_name, const char *cells_name, int index, struct vmm_device_tree_phandle_args *out);

/**
 * Find a node pointed by phandle in a list
 *
 * This function is useful to parse lists of phandles and their arguments.
 * Returns 0 on success and fills out_args, on error returns appropriate
 * errno value.
 *
 * Example:
 *
 * phandle1: node1 {
 * }
 *
 * phandle2: node2 {
 * }
 *
 * node3 {
 *  list = <&phandle1 0 2 &phandle2 2 3>;
 * }
 *
 * To get a device_node of the `node2' node you may call this:
 * vmm_device_tree_parse_phandle_with_fixed_args(node3, "list", 2, 1, &args);
 *
 *  NOTE: The returned nodes will have increased refrence count
 */
int vmm_device_tree_parse_phandle_with_fixed_args(
    const vmm_device_tree_node_t *node, const char *list_name, int cells_count, int index, struct vmm_device_tree_phandle_args *out);

/** Find the number of phandles references in a property
 *
 *  Returns the number of phandle + argument tuples within a property. It
 *  is a typical pattern to encode a list of phandle and variable
 *  arguments into a single property. The number of arguments is encoded
 *  by a property in the phandle-target node. For example, a gpios
 *  property would contain a list of GPIO specifies consisting of a
 *  phandle and 1 or more arguments. The number of arguments are
 *  determined by the #gpio-cells property in the node pointed to by the
 *  phandle.
 */
int vmm_device_tree_count_phandle_with_args(const vmm_device_tree_node_t *node, const char *list_name, const char *cells_name);

/** Increase reference count of give node */
vmm_device_tree_node_t *vmm_device_tree_ref_node(vmm_device_tree_node_t *node);

/** De-refernce a device tree node */
void vmm_device_tree_dref_node(vmm_device_tree_node_t *node);

/** Check if a device tree node have any child node */
bool vmm_device_tree_have_child(const vmm_device_tree_node_t *node);

/** Get next child node of a device tree node */
vmm_device_tree_node_t *vmm_device_tree_next_child(const vmm_device_tree_node_t *node, vmm_device_tree_node_t *current);

/** Itreate over each child node of a device tree node
 *  NOTE: If we need to break-out of the loop in-between then
 *  we will need use vmm_device_tree_dref_node() on child node of
 *  current iteration just before breaking-out loop.
 */
#define vmm_device_tree_for_each_child(child, node) \
    for (child = vmm_device_tree_next_child(node, NULL); child; child = vmm_device_tree_next_child(node, child))

/** Find the child node by name for a given parent
 *  @node:  parent node
 *  @name:  child name to look for.
 *
 *  This function looks for child node for given matching name
 *
 *  Returns a node pointer if found, with refcount incremented, use
 *  vmm_device_tree_dref_node() on it when done.
 *  Returns NULL if node is not found.
 */
vmm_device_tree_node_t *vmm_device_tree_get_child_by_name(vmm_device_tree_node_t *node, const char *name);

/** Add new node to device tree with given name
 *  NOTE: This function allows parent == NULL to enable creation of
 *  root node but only once.
 *  NOTE: Once root node is created, subsequent calls to this function
 *  with parent == NULL will add nodes under root node.
 */
vmm_device_tree_node_t *vmm_device_tree_addnode(vmm_device_tree_node_t *parent, const char *name);

/** Copy a node to another location in device tree */
int vmm_device_tree_copynode(vmm_device_tree_node_t *parent, const char *name, vmm_device_tree_node_t *src);

/** Delete a node from device tree */
int vmm_device_tree_delnode(vmm_device_tree_node_t *node);

/** Get device clock-frequency
 *  NOTE: This is based on 'clock-frequency' attribute of device tree node
 *  NOTE: This API if for hard-coding clock frequency in device tree node
 *  and it does not use clock_xxxx() APIs
 */
int vmm_device_tree_clock_frequency(vmm_device_tree_node_t *node, uint32_t *clock_freq);

/** Get count of device irqs
 *  NOTE: This is based on 'irq' attribute of device tree node
 */
uint32_t vmm_device_tree_irq_count(vmm_device_tree_node_t *node);

/**
 * Given a device tree node, find its interrupt parent node
 * @child: pointer to device node
 *
 * Returns a pointer to the interrupt parent node, or NULL if
 * the interrupt parent could not be determined.
 */
vmm_device_tree_node_t *vmm_device_tree_irq_find_parent(vmm_device_tree_node_t *child);

/**
 * Resolve an interrupt for a device
 * @device: the device whose interrupt is to be resolved
 * @index: index of the interrupt to resolve
 * @out_irq: structure filled by this function
 *
 * This function resolves an interrupt for a node by walking the interrupt tree,
 * finding which interrupt controller node it is attached to, and returning the
 * interrupt specifier that can be used to retrieve an Xvisor IRQ number.
 */
int vmm_device_tree_irq_parse_one(vmm_device_tree_node_t *device, int index, struct vmm_device_tree_phandle_args *out_irq);

/**
 * Find host irq_domain for a device node
 * @dev: Device node of the device whose interrupt is to be mapped
 *
 * Returns a pointer to the host irq_domain , or NULL if not found.
 */
struct vmm_host_irq_domain *vmm_device_tree_irq_domain_find(vmm_device_tree_node_t *dev);

/**
 * Parse and map an interrupt into Xvisor space
 * @dev: Device node of the device whose interrupt is to be mapped
 * @index: Index of the interrupt to map
 */
uint32_t vmm_device_tree_irq_parse_map(vmm_device_tree_node_t *dev, int index);

/** vmm_device_tree_is_available - check if a device is available for use
 *  @node: Node to check for availability
 *
 *  Returns TRUE if the status property is absent or set to "okay" or "ok",
 *  FALSE otherwise
 */
bool vmm_device_tree_is_available(const vmm_device_tree_node_t *node);

/** vmm_device_tree_alias_get_id - Get alias id for the given device_node
 * @np:         Pointer to the given device_node
 * @stem:       Alias stem of the given device_node
 *
 * The function scans all the properties of 'aliases' node to get the alias id
 * for the given device_node and alias stem.  It returns the alias id if found.
 */
int vmm_device_tree_alias_get_id(vmm_device_tree_node_t *node, const char *stem);

/** Get physical size of device registers
 *  NOTE: This is based on 'reg' and 'virtual-reg' attributes
 *  of device tree node
 */
int vmm_device_tree_regsize(vmm_device_tree_node_t *node, physical_size_t *size, int regset);

/** Get physical address of device registers
 *  NOTE: This is based on 'reg' and 'virtual-reg' attributes
 *  of device tree node
 */
int vmm_device_tree_regaddr(vmm_device_tree_node_t *node, physical_addr_t *addr, int regset);

/** Map device registers to virtual address
 *  NOTE: This is based on 'reg' and 'virtual-reg' attributes
 *  of device tree node
 */
int vmm_device_tree_regmap(vmm_device_tree_node_t *node, virtual_addr_t *addr, int regset);

/** Unmap device registers from virtual address
 *  NOTE: This is based on 'reg' and 'virtual-reg' attributes
 *  of device tree node
 */
int vmm_device_tree_regunmap(vmm_device_tree_node_t *node, virtual_addr_t addr, int regset);

/** Convert regname to regset index
 *  NOTE: This is based on 'reg-names' attribute of device tree node
 */
int vmm_device_tree_regname_to_regset(vmm_device_tree_node_t *node, const char *regname);

/** Map device registers to virtual address
 *  NOTE: This is based on 'reg' and 'reg-names' attributes
 *  of device tree node
 */
int vmm_device_tree_regmap_byname(vmm_device_tree_node_t *node, virtual_addr_t *addr, const char *regname);

/** Unmap device registers from virtual address
 *  NOTE: This is based on 'reg' and 'reg-names' attributes
 *  of device tree node
 */
int vmm_device_tree_regunmap_byname(vmm_device_tree_node_t *node, virtual_addr_t addr, const char *regname);

/** Request hostmem resource region for device registers physical
 *  address and Map device registers to a virtual address
 *  NOTE: This is based on 'reg' attribute of device tree node
 */
int vmm_device_tree_request_regmap(vmm_device_tree_node_t *node, virtual_addr_t *addr, int regset, const char *resname);

/** Unmap device registers virtual address and release hostmem
 *  resource region for device registers
 *  NOTE: This is based on 'reg' attribute of device tree node
 */
int vmm_device_tree_regunmap_release(vmm_device_tree_node_t *node, virtual_addr_t addr, int regset);

/** Check whether device registers are big endian */
bool vmm_device_tree_is_reg_big_endian(vmm_device_tree_node_t *node);

/** Check whether device is DMA cache-coherent */
bool vmm_device_tree_is_dma_coherent(vmm_device_tree_node_t *node);

/** Initialize device tree based reserved-memory */
int vmm_device_tree_reserved_memory_init(void);

/** Count number of enteries in nodeid table */
uint32_t vmm_device_tree_nidtable_count(void);

/** Get nodeid table entry at given index */
struct vmm_device_tree_nidtable_entry *vmm_device_tree_nidtable_get(int index);

/** Create matches table from nodeid table with given subsys
 *  NOTE: If subsys==NULL then matches table is created from all enteries
 */
const struct vmm_device_tree_nodeid *vmm_device_tree_nidtable_create_matches(const char *subsys);

/** Destroy matches table created from nodeid table */
void vmm_device_tree_nidtable_destroy_matches(const struct vmm_device_tree_nodeid *matches);

/** Initialize device tree */
int vmm_device_tree_init(void);

#endif /* __VMM_DEVICE_TREE_H_ */
