#/**
# Copyright (c) 2010 Anup Patel.
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

core-objs-y+= vmm_main.o
core-objs-y+= vmm_initfn.o
core-objs-y+= vmm_heap.o
core-objs-y+= vmm_page_pool.o
core-objs-y+= vmm_stdio.o
core-objs-y+= vmm_cpumask.o
core-objs-y+= vmm_device_tree.o
core-objs-y+= vmm_device_tree_irq.o
core-objs-y+= vmm_device_tree_reg.o
core-objs-y+= vmm_device_resource.o
core-objs-y+= vmm_device_driver.o
core-objs-y+= vmm_platform.o
core-objs-y+= vmm_platform_msi.o
core-objs-y+= vmm_device_emulate.o
core-objs-y+= vmm_resource.o
core-objs-y+= vmm_host_irq.o
core-objs-y+= vmm_host_extend_irq.o
core-objs-y+= vmm_host_irq_domain.o
core-objs-y+= vmm_host_ram.o
core-objs-y+= vmm_host_virtual_address_pool.o
core-objs-y+= vmm_host_address_space.o
core-objs-y+= vmm_msi.o
core-objs-y+= vmm_cpu_hotplug.o
core-objs-y+= vmm_per_cpu.o
core-objs-$(CONFIG_SMP)+= vmm_smp.o
core-objs-y+= vmm_clocksource.o
core-objs-y+= vmm_clock_chip.o
core-objs-y+= vmm_timer.o
core-objs-y+= vmm_delay.o
core-objs-y+= vmm_share_memory.o
core-objs-y+= vmm_vcpu_irq.o
core-objs-y+= vmm_guest_address_space.o
core-objs-y+= vmm_manager.o
core-objs-y+= vmm_scheduler.o
core-objs-y+= vmm_threads.o
core-objs-y+= vmm_waitqueue.o
core-objs-y+= vmm_completion.o
core-objs-y+= vmm_semaphore.o
core-objs-y+= vmm_mutex.o
core-objs-y+= vmm_notifier.o
core-objs-y+= vmm_workqueue.o
core-objs-y+= vmm_command_manager.o
core-objs-y+= vmm_wall_clock.o
core-objs-y+= vmm_iommu.o
core-objs-y+= vmm_char_device.o
core-objs-y+= vmm_modules.o
core-objs-y+= vmm_params.o
core-objs-$(CONFIG_PROFILE)+= vmm_profiler.o
core-objs-$(CONFIG_LOAD_BALANCER)+= vmm_load_balancer.o
core-objs-y+= vmm_exception_table.o
