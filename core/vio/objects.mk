#/**
# Copyright (c) 2013 Anup Patel.
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# @file objects.mk
# @author Anup Patel (anup@brainfault.org)
# @brief list of core objects to be build
# */

core-objs-$(CONFIG_VIRTIO)+= vio/vmm_virtio.o

core-objs-$(CONFIG_VMSG)+= vio/vmm_vmsg.o

core-objs-$(CONFIG_VSERIAL)+= vio/vmm_vserial.o

core-objs-$(CONFIG_VSPI)+= vio/vmm_vspi.o

core-objs-$(CONFIG_VDISK)+= vio/vmm_virtual_disk.o

core-objs-$(CONFIG_VDISPLAY)+= vio/vmm_virtual_display.o

core-objs-$(CONFIG_VINPUT)+= vio/vmm_virtual_input_core.o

vmm_virtual_input_core-y+= vmm_virtual_input.o
vmm_virtual_input_core-y+= vmm_keymaps.o
vmm_virtual_input_core-y+= keymaps/modifiers.o
vmm_virtual_input_core-y+= keymaps/common.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_AR)+= keymaps/ar.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_BEPO)+= keymaps/bepo.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_CZ)+= keymaps/cz.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_DA)+= keymaps/da.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_DE_CH)+= keymaps/de-ch.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_DE)+= keymaps/de.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_EN_GB)+= keymaps/en-gb.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_EN_US)+= keymaps/en-us.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_ES)+= keymaps/es.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_ET)+= keymaps/et.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_FI)+= keymaps/fi.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_FO)+= keymaps/fo.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_FR_BE)+= keymaps/fr-be.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_FR_CA)+= keymaps/fr-ca.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_FR_CH)+= keymaps/fr-ch.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_FR)+= keymaps/fr.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_HR)+= keymaps/hr.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_HU)+= keymaps/hu.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_IS)+= keymaps/is.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_IT)+= keymaps/it.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_JA)+= keymaps/ja.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_LT)+= keymaps/lt.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_LV)+= keymaps/lv.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_MK)+= keymaps/mk.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_NL_BE)+= keymaps/nl-be.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_NL)+= keymaps/nl.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_NO)+= keymaps/no.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_PL)+= keymaps/pl.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_PT_BR)+= keymaps/pt-br.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_PT)+= keymaps/pt.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_RU)+= keymaps/ru.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_SL)+= keymaps/sl.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_SV)+= keymaps/sv.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_TH)+= keymaps/th.o
vmm_virtual_input_core-$(CONFIG_VINPUT_KEYMAP_TR)+= keymaps/tr.o

%/vmm_virtual_input_core.o: $(foreach obj,$(vmm_virtual_input_core-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/vmm_virtual_input_core.dep: $(foreach dep,$(vmm_virtual_input_core-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)
