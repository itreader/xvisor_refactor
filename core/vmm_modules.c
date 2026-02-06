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
 * @file vmm_modules.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file of module management code
 */

#include <arch_sections.h>
#include <libs/bitmap.h>
#include <libs/elf.h>
#include <libs/kallsyms.h>
#include <libs/libsort.h>
#include <libs/list.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_modules.h>
#include <vmm_page_pool.h>
#include <vmm_spinlocks.h>
#include <vmm_stdio.h>

#ifndef ARCH_SHF_SMALL
#define ARCH_SHF_SMALL 0
#endif

/*
 * Modules' sections will be aligned on page boundaries
 * to ensure complete separation of code and data, but
 * only when CONFIG_DEBUG_SET_MODULE_RONX=y
 */
#ifdef CONFIG_DEBUG_SET_MODULE_RONX
#define debug_align(X) ALIGN(X, PAGE_SIZE)
#else
#define debug_align(X) (X)
#endif

struct load_info {
    Elf_Ehdr *hdr;
    uint64_t  len;
    Elf_Shdr *sechdrs;
    char     *secstrings, *strtab;
    uint64_t *strmap;
    uint64_t  symoffs, stroffs;

    struct {
        uint32_t sym, str;
    } index;
};

/* FIXME: Implement reference counting for loadable modules */

struct module_wrap {
    double_list_t head;

    /* vmm_module_t and additional info */
    vmm_module_t mod;
    int          mod_ret;
    bool         built_in;

    /* Pages allocated for module */
    virtual_addr_t pg_start;
    uint32_t       pg_count;
    uint32_t       core_size;
    uint32_t       core_text_size;
    uint32_t       core_ro_size;

    /* Exported symbols */
    struct vmm_symbol *syms;
    uint32_t           num_syms;
};

struct vmm_modules_ctrl {
    vmm_spinlock_t lock;
    double_list_t  mod_list;
    uint32_t       mod_count;
};

static struct vmm_modules_ctrl modctrl;

int vmm_modules_find_symbol(const char *symname, struct vmm_symbol *sym)
{
    uint32_t            s;
    bool                found;
    irq_flags_t         flags;
    struct module_wrap *mwrap = NULL;

    if (!symname || !sym) {
        return VMM_EFAIL;
    }

    sym->addr = kallsyms_lookup_name(symname);

    if (sym->addr) {
        if (strlcpy(sym->name, symname, sizeof(sym->name)) >= sizeof(sym->name)) {
            return VMM_EOVERFLOW;
        }

        sym->type = VMM_SYMBOL_GPL;
        return VMM_OK;
    }

    vmm_spin_lock_irq_save(&modctrl.lock, flags);

    found = FALSE;
    list_for_each_entry(mwrap, &modctrl.mod_list, head)
    {
        for (s = 0; s < mwrap->num_syms; s++) {
            if (strcmp(mwrap->syms[s].name, symname) == 0) {
                memcpy(sym, &mwrap->syms[s], sizeof(*sym));
                found = TRUE;
                break;
            }
        }

        if (found) {
            break;
        }
    }

    vmm_spin_unlock_irq_restore(&modctrl.lock, flags);

    if (!found) {
        return VMM_EFAIL;
    }

    return VMM_OK;
}

bool vmm_modules_isbuiltin(vmm_module_t *mod)
{
    struct module_wrap *mwrap;

    if (!mod) {
        return FALSE;
    }

    mwrap = container_of(mod, struct module_wrap, mod);

    if (mwrap->mod.signature != VMM_MODULE_SIGNATURE) {
        return FALSE;
    }

    return (mwrap->built_in) ? TRUE : FALSE;
}

#ifdef CONFIG_MODULES

/* Find a module section: 0 means not found. */
static uint32_t find_sec(const struct load_info *info, const char *name)
{
    uint32_t i;

    for (i = 1; i < info->hdr->e_shnum; i++) {
        Elf_Shdr *shdr = &info->sechdrs[i];

        /* Alloc bit cleared means "ignore it." */
        if ((shdr->sh_flags & SHF_ALLOC) && strcmp(info->secstrings + shdr->sh_name, name) == 0) {
            return i;
        }
    }

    return 0;
}

/* Sets info->hdr and info->len. */
static int sethdr_and_check(struct load_info *info, void *mod, uint64_t len)
{
    Elf_Ehdr *hdr = mod;

    if (len < sizeof(*hdr)) {
        return VMM_ENOEXEC;
    }

    /* Suck in entire file: we'll want most of it. */
    /* vmalloc barfs on "unusual" numbers.  Check here */
    if (len > 1 * 1024 * 1024) {
        return VMM_EINVALID;
    }

    /* Sanity checks against loading binaries or wrong arch,
       weird elf version */
    if (memcmp(hdr->e_ident, ELFMAG, SELFMAG) != 0 || hdr->e_type != ET_REL || !arch_elf_check_hdr(hdr) || hdr->e_shentsize != sizeof(Elf_Shdr)) {
        return VMM_ENOEXEC;
    }

    if (len < hdr->e_shoff + hdr->e_shnum * sizeof(Elf_Shdr)) {
        return VMM_ENOEXEC;
    }

    info->hdr = hdr;
    info->len = len;

    return 0;
}

static int rewrite_section_headers(struct load_info *info)
{
    uint32_t i;

    /* This should always be true, but let's be sure. */
    info->sechdrs[0].sh_addr = 0;

    for (i = 1; i < info->hdr->e_shnum; i++) {
        Elf_Shdr *shdr = &info->sechdrs[i];

        if (shdr->sh_type != SHT_NOBITS && info->len < shdr->sh_offset + shdr->sh_size) {
            vmm_printf("Module len %lu truncated\n", info->len);
            return VMM_ENOEXEC;
        }

        /* Mark all sections sh_addr with their address in the
           temporary image. */
        shdr->sh_addr = (uint64_t)info->hdr + shdr->sh_offset;
    }

    return VMM_OK;
}

/*
 * Set up our basic convenience variables (pointers to section headers,
 * search for module section index etc), and do some basic section
 * verification.
 */
static int setup_load_info(struct load_info *info)
{
    uint32_t i;
    int      err;

    /* Set up the convenience variables */
    info->sechdrs    = (void *)info->hdr + info->hdr->e_shoff;
    info->secstrings = (void *)info->hdr + info->sechdrs[info->hdr->e_shstrndx].sh_offset;

    if ((err = rewrite_section_headers(info))) {
        return err;
    }

    /* Find internal symbols and strings. */
    for (i = 1; i < info->hdr->e_shnum; i++) {
        if (info->sechdrs[i].sh_type == SHT_SYMTAB) {
            info->index.sym = i;
            info->index.str = info->sechdrs[i].sh_link;
            info->strtab    = (char *)info->hdr + info->sechdrs[info->index.str].sh_offset;
            break;
        }
    }

    /* If symbol table not found then return failure */
    if (!info->strtab) {
        return VMM_ENOTAVAIL;
    }

    return VMM_OK;
}

static int alloc_and_load_modtbl(struct module_wrap *mwrap, struct load_info *info)
{
    uint32_t i;

    i = find_sec(info, ".modtbl");

    if (!i) {
        return VMM_ENOEXEC;
    }

    memcpy(&mwrap->mod, (void *)info->sechdrs[i].sh_addr, sizeof(mwrap->mod));

    if (mwrap->mod.signature != VMM_MODULE_SIGNATURE) {
        return VMM_ENOEXEC;
    }

    mwrap->mod_ret = 0;

    info->sechdrs[i].sh_flags &= ~SHF_ALLOC;
    info->sechdrs[i].sh_addr = (uint64_t)&mwrap->mod;

    return VMM_OK;
}

static int alloc_and_load_symtbl(struct module_wrap *mwrap, struct load_info *info)
{
    uint32_t i;

    i = find_sec(info, ".symtbl");

    if (!i) {
        mwrap->syms     = NULL;
        mwrap->num_syms = 0;
        return VMM_OK;
    }

    mwrap->syms = vmm_malloc(info->sechdrs[i].sh_size);

    if (!mwrap->syms) {
        return VMM_ENOMEM;
    }

    memcpy(&mwrap->syms, (void *)info->sechdrs[i].sh_addr, sizeof(mwrap->mod));
    mwrap->num_syms = info->sechdrs[i].sh_size / sizeof(struct vmm_symbol);

    info->sechdrs[i].sh_flags &= ~SHF_ALLOC;
    info->sechdrs[i].sh_addr = (uint64_t)mwrap->syms;

    return VMM_OK;
}

/* Update size with this section: return offset. */
static long get_offset(struct module_wrap *mwrap, uint32_t *size, Elf_Shdr *sechdr, uint32_t section)
{
    long ret;

    /*  *size += arch_mod_section_prepend(mod, section); */
    ret   = align(*size, sechdr->sh_addralign ?: 1);
    *size = ret + sechdr->sh_size;

    return ret;
}

/* Lay out the SHF_ALLOC sections in a way not dissimilar to how ld
   might -- code, read-only data, read-write data, small data.  Tally
   sizes, and place the offsets into sh_entsize fields: high bit means it
   belongs in init. */
static void layout_sections(struct module_wrap *mwrap, struct load_info *info)
{
    static uint64_t const masks[][2] = {
        /* NOTE: all executable code must be the first section
         * in this array; otherwise modify the text_size
         * finder in the two loops below */
        {SHF_EXECINSTR | SHF_ALLOC,  ARCH_SHF_SMALL            },
        {SHF_ALLOC,                  SHF_WRITE | ARCH_SHF_SMALL},
        {SHF_WRITE | SHF_ALLOC,      ARCH_SHF_SMALL            },
        {ARCH_SHF_SMALL | SHF_ALLOC, 0                         }
    };
    uint32_t m, i;

    for (i = 0; i < info->hdr->e_shnum; i++) {
        info->sechdrs[i].sh_entsize = ~0UL;
    }

    for (m = 0; m < array_size(masks); ++m) {
        for (i = 0; i < info->hdr->e_shnum; ++i) {
            Elf_Shdr *s = &info->sechdrs[i];

            if ((s->sh_flags & masks[m][0]) != masks[m][0] || (s->sh_flags & masks[m][1]) || s->sh_entsize != ~0UL) {
                continue;
            }

            s->sh_entsize = get_offset(mwrap, &mwrap->core_size, s, i);
        }

        switch (m) {
            case 0: /* executable */
                mwrap->core_size      = debug_align(mwrap->core_size);
                mwrap->core_text_size = mwrap->core_size;
                break;

            case 1: /* RO: text and ro-data */
                mwrap->core_size    = debug_align(mwrap->core_size);
                mwrap->core_ro_size = mwrap->core_size;
                break;

            case 3: /* whole core */
                mwrap->core_size = debug_align(mwrap->core_size);
                break;
        }
    }
}

static bool is_core_symbol(const Elf_Sym *src, const Elf_Shdr *sechdrs, uint32_t shnum)
{
    const Elf_Shdr *sec;

    if (src->st_shndx == SHN_UNDEF || src->st_shndx >= shnum || !src->st_name) {
        return false;
    }

    sec = sechdrs + src->st_shndx;

    if (!(sec->sh_flags & SHF_ALLOC)
#if 1 /* FIXME: ??? */
#ifndef CONFIG_KALLSYMS_ALL
        || !(sec->sh_flags & SHF_EXECINSTR)
#endif
#endif
    )
        return false;

    return true;
}

static void layout_symtab(struct module_wrap *mwrap, struct load_info *info)
{
    Elf_Shdr      *symsect = info->sechdrs + info->index.sym;
    Elf_Shdr      *strsect = info->sechdrs + info->index.str;
    const Elf_Sym *src;
    uint32_t       i, nsrc, ndst;

    /* Put symbol section at end of module. */
    symsect->sh_flags |= SHF_ALLOC;
    symsect->sh_entsize = get_offset(mwrap, &mwrap->core_size, symsect, info->index.sym);

    src                 = (void *)info->hdr + symsect->sh_offset;
    nsrc                = symsect->sh_size / sizeof(*src);

    for (ndst = i = 1; i < nsrc; ++i, ++src) {
        if (is_core_symbol(src, info->sechdrs, info->hdr->e_shnum)) {
            uint32_t j = src->st_name;

            while (!__test_and_set_bit(j, info->strmap) && info->strtab[j]) {
                ++j;
            }

            ++ndst;
        }
    }

    /* Append room for core symbols at end of core part. */
    info->symoffs    = align(mwrap->core_size, symsect->sh_addralign ?: 1);
    mwrap->core_size = info->symoffs + ndst * sizeof(Elf_Sym);

    /* Put string table section at end of init part of module. */
    strsect->sh_flags |= SHF_ALLOC;
    strsect->sh_entsize = get_offset(mwrap, &mwrap->core_size, strsect, info->index.str);

    /* Append room for core symbols' strings at end of core part. */
    info->stroffs       = mwrap->core_size;
    __set_bit(0, info->strmap);
    mwrap->core_size += bitmap_weight(info->strmap, strsect->sh_size);
}

static int move_module(struct module_wrap *mwrap, struct load_info *info)
{
    uint32_t       i;
    virtual_addr_t addr = vmm_page_pool_alloc(VMM_PAGE_POOL_NORMAL, VMM_SIZE_TO_PAGE(mwrap->core_size));

    if (!addr) {
        return VMM_ENOMEM;
    }

    mwrap->pg_count = VMM_SIZE_TO_PAGE(mwrap->core_size);
    mwrap->pg_start = addr;

    memset((void *)mwrap->pg_start, 0, mwrap->core_size);

    /* Transfer each section which specifies SHF_ALLOC */
    for (i = 0; i < info->hdr->e_shnum; i++) {
        void     *dest;
        Elf_Shdr *shdr = &info->sechdrs[i];

        if (!(shdr->sh_flags & SHF_ALLOC)) {
            continue;
        }

        dest = (void *)mwrap->pg_start + shdr->sh_entsize;

        if (shdr->sh_type != SHT_NOBITS) {
            memcpy(dest, (void *)shdr->sh_addr, shdr->sh_size);
        }

        /* Update sh_addr to point to copy in image. */
        shdr->sh_addr = (uint64_t)dest;
    }

    return VMM_OK;
}

static int alloc_and_load_sections(struct module_wrap *mwrap, struct load_info *info)
{
    int err;

    layout_sections(mwrap, info);

    info->strmap = vmm_malloc(BITS_TO_LONGS(info->sechdrs[info->index.str].sh_size) * sizeof(long));

    if (!info->strmap) {
        return VMM_ENOMEM;
    }

    layout_symtab(mwrap, info);

    /* Allocate and move to the final place */
    err = move_module(mwrap, info);

    if (err) {
        goto free_strmap;
    }

    return VMM_OK;

free_strmap:
    vmm_free(info->strmap);
    return err;
}

/* Change all symbols so that st_value encodes the pointer directly. */
static int simplify_symbols(struct module_wrap *mwrap, struct load_info *info)
{
    Elf_Shdr         *symsec = &info->sechdrs[info->index.sym];
    Elf_Sym          *sym    = (void *)symsec->sh_addr;
    uint64_t          secbase;
    uint32_t          i;
    int               ret = VMM_OK;
    struct vmm_symbol vsym;

    for (i = 1; i < symsec->sh_size / sizeof(Elf_Sym); i++) {
        const char *name = info->strtab + sym[i].st_name;

        if (strcmp(name, "test_func") == 0) {
            vmm_printf("%s: sym %s\n", __func__, name);
        }

        ret = VMM_OK;

        switch (sym[i].st_shndx) {
            case SHN_COMMON:
                /* We compiled with -fno-common.  These are not
                   supposed to happen.  */
                vmm_printf("%s: please compile with -fno-common\n", mwrap->mod.name);
                ret = VMM_ENOEXEC;
                break;

            case SHN_ABS:
                break;

            case SHN_UNDEF:
                ret = vmm_modules_find_symbol(name, &vsym);

                if (ret) {
                    break;
                }

                /* Ok if resolved.  */
                sym[i].st_value = vsym.addr;
                break;

            default:
                secbase = info->sechdrs[sym[i].st_shndx].sh_addr;
                sym[i].st_value += secbase;
                break;
        }

        if (ret) {
            break;
        }
    }

    return ret;
}

static int apply_relocations(struct module_wrap *mwrap, struct load_info *info)
{
    uint32_t i;
    int      err = 0;

    /* Now do relocations. */
    for (i = 1; i < info->hdr->e_shnum; i++) {
        uint32_t infosec = info->sechdrs[i].sh_info;

        /* Not a valid relocation section? */
        if (infosec >= info->hdr->e_shnum) {
            continue;
        }

        /* Don't bother with non-allocated sections */
        if (!(info->sechdrs[infosec].sh_flags & SHF_ALLOC)) {
            continue;
        }

        if (info->sechdrs[i].sh_type == SHT_REL) {
            err = arch_elf_apply_relocate(info->sechdrs, info->strtab, info->index.sym, i, &mwrap->mod);
        } else if (info->sechdrs[i].sh_type == SHT_RELA) {
            err = arch_elf_apply_relocate_add(info->sechdrs, info->strtab, info->index.sym, i, &mwrap->mod);
        }

        if (err < 0) {
            break;
        }
    }

    return err;
}

int vmm_modules_load(virtual_addr_t load_addr, virtual_size_t load_size)
{
    int              i, rc;
    irq_flags_t      flags;
    struct load_info info = {
        NULL,
    };
    struct module_wrap *mwrap;

    if ((rc = sethdr_and_check(&info, (void *)load_addr, load_size))) {
        return rc;
    }

    if ((rc = setup_load_info(&info))) {
        return rc;
    }

    mwrap = vmm_zalloc(sizeof(*mwrap));

    if (!mwrap) {
        return VMM_ENOMEM;
    }

    INIT_LIST_HEAD(&mwrap->head);

    /* Allocate and load .modtbl section
     * Note: This will clear SHF_ALLOC flag
     */
    if ((rc = alloc_and_load_modtbl(mwrap, &info))) {
        goto free_mwrap;
    }

    /* Allocate and load .symtbl section
     * Note: This will clear SHF_ALLOC flag
     */
    if ((rc = alloc_and_load_symtbl(mwrap, &info))) {
        goto free_mwrap;
    }

    /* Allocate and load all sections with SHF_ALLOC flag */
    if ((rc = alloc_and_load_sections(mwrap, &info))) {
        goto free_syms;
    }

    /* Enable SHF_ALLOC flag for .modtbl & .symtbl
     * so that relocation apply to these sections
     */
    for (i = 1; i < info.hdr->e_shnum; i++) {
        const char *name = info.secstrings + info.sechdrs[i].sh_name;

        if (strcmp(name, ".modtbl") == 0) {
            info.sechdrs[i].sh_flags |= SHF_ALLOC;
        } else if (strcmp(name, ".symtbl") == 0) {
            info.sechdrs[i].sh_flags |= SHF_ALLOC;
        }
    }

    /* Resolve symbols */
    if ((rc = simplify_symbols(mwrap, &info))) {
        goto free_pages;
    }

    /* Apply relocations to loaded sections */
    if ((rc = apply_relocations(mwrap, &info))) {
        goto free_pages;
    }

    /* Get rid of temporary strmap. */
    vmm_free(info.strmap);

    if (mwrap->mod.init) {
        if ((rc = mwrap->mod.init())) {
            goto free_pages;
        }

        mwrap->mod_ret = rc;
    }

    vmm_spin_lock_irq_save(&modctrl.lock, flags);
    list_add_tail(&mwrap->head, &modctrl.mod_list);
    modctrl.mod_count++;
    vmm_spin_unlock_irq_restore(&modctrl.lock, flags);

    return VMM_OK;

free_pages:
    vmm_page_pool_free(VMM_PAGE_POOL_NORMAL, mwrap->pg_start, mwrap->pg_count);
free_syms:

    if (mwrap->syms) {
        vmm_free(mwrap->syms);
    }

free_mwrap:
    vmm_free(mwrap);
    return rc;
}

int vmm_modules_unload(vmm_module_t *mod)
{
    irq_flags_t         flags;
    struct module_wrap *mwrap;

    if (!mod) {
        return VMM_EFAIL;
    }

    mwrap = container_of(mod, struct module_wrap, mod);

    if (mwrap->mod.signature != VMM_MODULE_SIGNATURE) {
        return VMM_EFAIL;
    }

    if (mwrap->built_in) {
        return VMM_EFAIL;
    }

    vmm_spin_lock_irq_save(&modctrl.lock, flags);

    if (mwrap->mod.exit) {
        mwrap->mod.exit();
    }

    list_del(&mwrap->head);
    vmm_page_pool_free(VMM_PAGE_POOL_NORMAL, mwrap->pg_start, mwrap->pg_count);
    vmm_free(mwrap);
    modctrl.mod_count--;

    vmm_spin_unlock_irq_restore(&modctrl.lock, flags);

    return VMM_OK;
}

#else

int vmm_modules_load(virtual_addr_t load_addr, virtual_size_t load_size)
{
    return VMM_ENOTAVAIL;
}

int vmm_modules_unload(vmm_module_t *mod)
{
    return VMM_ENOTAVAIL;
}

#endif

vmm_module_t *vmm_modules_getmodule(uint32_t index)
{
    bool                found = FALSE;
    irq_flags_t         flags;
    struct module_wrap *mwrap;

    vmm_spin_lock_irq_save(&modctrl.lock, flags);

    if (modctrl.mod_count <= index) {
        vmm_spin_unlock_irq_restore(&modctrl.lock, flags);
        return NULL;
    }

    list_for_each_entry(mwrap, &modctrl.mod_list, head)
    {
        if (!index) {
            found = TRUE;
            break;
        }

        index--;
    }

    vmm_spin_unlock_irq_restore(&modctrl.lock, flags);

    return (found) ? &mwrap->mod : NULL;
}

uint32_t vmm_modules_count(void)
{
    uint32_t    ret;
    irq_flags_t flags;

    vmm_spin_lock_irq_save(&modctrl.lock, flags);
    ret = modctrl.mod_count;
    vmm_spin_unlock_irq_restore(&modctrl.lock, flags);

    return ret;
}

struct modules_list {
    int           nr_modules;
    double_list_t mod_list;
};

static struct modules_list *__init aggregate_modules(uint32_t mod_start, uint32_t size)
{
    virtual_addr_t       ca;
    uint32_t            *cp, mod_end = mod_start + size;
    vmm_module_t        *modinfo       = NULL;
    struct modules_list *cong_mod_list = (struct modules_list *)vmm_malloc(sizeof(struct modules_list));

    INIT_LIST_HEAD(&cong_mod_list->mod_list);
    cong_mod_list->nr_modules = 0;

    /*
     * Search for modules signatures from mod start to
     * end of modules area picking up modules information
     * base, creating a linked list.
     */
    for (ca = mod_start; ca < mod_end;) {
        cp = (uint32_t *)ca;

        if (*cp == VMM_MODULE_SIGNATURE) {
            modinfo = (vmm_module_t *)ca;
            list_add(&modinfo->head, &cong_mod_list->mod_list);
            cong_mod_list->nr_modules++;
            ca += sizeof(vmm_module_t);
        } else {
            /* next word to check for signature. */
            ca += sizeof(uint32_t);
        }
    }

    return cong_mod_list;
}

static int __init cmp_list_element(void *p, double_list_t *a, double_list_t *b)
{
    vmm_module_t *moda, *modb;

    moda = list_entry(a, vmm_module_t, head);
    modb = list_entry(b, vmm_module_t, head);

    return moda->ipriority - modb->ipriority;
}

int __init vmm_modules_init(void)
{
    int                  ret;
    struct module_wrap  *mwrap;
    vmm_module_t        *mod_entry;
    struct modules_list *ag_mod_list;

    /* Reset the control structure */
    memset(&modctrl, 0, sizeof(modctrl));
    INIT_SPIN_LOCK(&modctrl.lock);
    INIT_LIST_HEAD(&modctrl.mod_list);
    modctrl.mod_count = 0;

    ag_mod_list       = aggregate_modules(arch_modtable_vaddr(), arch_modtable_size());

    /* If no built-in modules found then return */
    if (!ag_mod_list->nr_modules) {
        return VMM_OK;
    }

    list_mergesort(NULL, &ag_mod_list->mod_list, cmp_list_element);

    /* Initialize built-in modules in sorted order */
    list_for_each_entry(mod_entry, &ag_mod_list->mod_list, head)
    {
        mwrap = vmm_zalloc(sizeof(struct module_wrap));

        if (unlikely(!mwrap)) {
            break;
        }

        INIT_LIST_HEAD(&mwrap->head);
        memcpy(&mwrap->mod, mod_entry, sizeof(vmm_module_t));
        mwrap->built_in = TRUE;

        /* Initialize module if required */
        if (mwrap->mod.init) {
#if defined(CONFIG_VERBOSE_MODE)
            vmm_printf("Module Init %s\n", mwrap->mod.name);
#endif

            if ((ret = mwrap->mod.init())) {
                vmm_printf("%s: %s init error %d\n", __func__, mwrap->mod.name, ret);
            }

            mwrap->mod_ret = ret;
        }

        list_add_tail(&mwrap->head, &modctrl.mod_list);
        modctrl.mod_count++;
    }

    return VMM_OK;
}
