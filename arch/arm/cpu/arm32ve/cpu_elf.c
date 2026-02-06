/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file cpu_elf.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief implementation of CPU specific elf functions
 */

#include <libs/elf.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>

int arch_elf_check_hdr(const struct elf32_hdr *x)
{
    /* Make sure it's an ARM executable */
    if (x->e_machine != EM_ARM) {
        return 0;
    }

    /* Make sure the entry address is reasonable */
    if (x->e_entry & 3) {
        return 0;
    }

    /* Don't allow unknown ABI */
    if ((x->e_flags & EF_ARM_EABI_MASK) == EF_ARM_EABI_UNKNOWN) {
        return 0;
    }

    return 1;
}

int arch_elf_apply_relocate(struct elf32_shdr *sechdrs, const char *strtab, uint32_t symindex, uint32_t relindex, vmm_module_t *mod)
{
    struct elf32_shdr *symsec = sechdrs + symindex;
    struct elf32_shdr *relsec = sechdrs + relindex;
    struct elf32_shdr *dstsec = sechdrs + relsec->sh_info;
    struct elf32_rel  *rel    = (void *)relsec->sh_addr;
    uint32_t           i;

    for (i = 0; i < relsec->sh_size / sizeof(Elf32_Rel); i++, rel++) {
        virtual_addr_t    loc;
        struct elf32_sym *sym;
        const char       *symname;
        int32_t           offset;

        offset = ELF32_R_SYM(rel->r_info);

        if ((offset < 0) || (offset > (symsec->sh_size / sizeof(Elf32_Sym)))) {
            vmm_printf(
                "%s: section %u reloc %u: "
                "bad relocation sym offset\n",
                mod->name, relindex, i);
            return VMM_ENOEXEC;
        }

        sym     = ((Elf32_Sym *)symsec->sh_addr) + offset;
        symname = strtab + sym->st_name;

        if (((int32_t)rel->r_offset < 0) || (rel->r_offset > dstsec->sh_size - sizeof(uint32_t))) {
            vmm_printf(
                "%s: section %u reloc %u sym '%s': out of "
                "bounds relocation, offset %d size %u\n",
                mod->name, relindex, i, symname, rel->r_offset, dstsec->sh_size);
            return VMM_ENOEXEC;
        }

        loc = dstsec->sh_addr + rel->r_offset;

        switch (ELF32_R_TYPE(rel->r_info)) {
            case R_ARM_NONE:
                /* ignore */
                break;

            case R_ARM_ABS32:
                *(uint32_t *)loc += sym->st_value;
                break;

            case R_ARM_PC24:
            case R_ARM_CALL:
            case R_ARM_JUMP24:
                offset = (*(uint32_t *)loc & 0x00ffffff) << 2;

                if (offset & 0x02000000) {
                    offset -= 0x04000000;
                }

                offset += sym->st_value - loc;

                if (offset & 3 || offset <= (int32_t)0xfe000000 || offset >= (int32_t)0x02000000) {
                    vmm_printf(
                        "%s: section %u reloc %u sym '%s': "
                        "relocation %u out of range (%" PRIADDR " -> %#x)\n",
                        mod->name, relindex, i, symname, ELF32_R_TYPE(rel->r_info), loc, sym->st_value);
                    return VMM_ENOEXEC;
                }

                offset >>= 2;

                *(uint32_t *)loc &= 0xff000000;
                *(uint32_t *)loc |= offset & 0x00ffffff;
                break;

            case R_ARM_V4BX:
                /* Preserve Rm and the condition code. Alter
                 * other bits to re-code instruction as
                 * MOV PC,Rm.
                 */
                *(uint32_t *)loc &= 0xf000000f;
                *(uint32_t *)loc |= 0x01a0f000;
                break;

            case R_ARM_PREL31:
                offset           = *(uint32_t *)loc + sym->st_value - loc;
                *(uint32_t *)loc = offset & 0x7fffffff;
                break;

            case R_ARM_MOVW_ABS_NC:
            case R_ARM_MOVT_ABS:
                offset = *(uint32_t *)loc;
                offset = ((offset & 0xf0000) >> 4) | (offset & 0xfff);
                offset = (offset ^ 0x8000) - 0x8000;

                offset += sym->st_value;

                if (ELF32_R_TYPE(rel->r_info) == R_ARM_MOVT_ABS) {
                    offset >>= 16;
                }

                *(uint32_t *)loc &= 0xfff0f000;
                *(uint32_t *)loc |= ((offset & 0xf000) << 4) | (offset & 0x0fff);
                break;

            default:
                vmm_printf("%s: unknown relocation: %u\n", mod->name, ELF32_R_TYPE(rel->r_info));
                return VMM_ENOEXEC;
        }
    }

    return 0;
}

int arch_elf_apply_relocate_add(Elf32_Shdr *sechdrs, const char *strtab, uint32_t symindex, uint32_t relsec, vmm_module_t *mod)
{
    vmm_printf("module %s: ADD RELOCATION unsupported\n", mod->name);
    return VMM_ENOEXEC;
}
