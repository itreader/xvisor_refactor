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
 * @file vmm_main.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Hypervisor启动、停止和复位主文件
 */

#include <arch_board.h>
#include <arch_cpu.h>
#include <vmm_char_device.h>
#include <vmm_clock_chip.h>
#include <vmm_clocksource.h>
#include <vmm_command_manager.h>
#include <vmm_cpu_hotplug.h>
#include <vmm_delay.h>
#include <vmm_device_driver.h>
#include <vmm_device_emulate.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_exception_table.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_host_irq.h>
#include <vmm_initfn.h>
#include <vmm_iommu.h>
#include <vmm_load_balancer.h>
#include <vmm_manager.h>
#include <vmm_modules.h>
#include <vmm_page_pool.h>
#include <vmm_params.h>
#include <vmm_per_cpu.h>
#include <vmm_profiler.h>
#include <vmm_scheduler.h>
#include <vmm_share_memory.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_threads.h>
#include <vmm_timer.h>
#include <vmm_version.h>
#include <vmm_wall_clock.h>
#include <vmm_workqueue.h>

/* Optional includes */
#include <drv/rtc.h>

/**
 * @brief 系统挂起（无限循环）
 * @return 成功返回VMM_OK，失败返回错误码
 */
void __noreturn vmm_hang(void)
{
    while (1)
        ;
}

static vmm_work_t sys_init;
static vmm_work_t sys_postinit;
static bool       sys_init_done = FALSE;

static char  *console_param     = NULL;
static size_t console_param_len = 0;

/**
 * @brief 处理控制台设备参数，通过引导参数或设备树设置其为标准输入输出设备
 * @param str 控制台设备名称字符串
 */
static void console_param_process(const char *str)
{
    vmm_char_device_t      *cdev;
    vmm_device_tree_node_t *node;

    /* Find character device based on console attribute */
    if (!(cdev = vmm_char_device_find(str))) {
        if ((node = vmm_device_tree_getnode(str))) {
            cdev = vmm_char_device_find(node->name);
            vmm_device_tree_dref_node(node);
        }
    }

    /* Set chosen console device as stdio device */
    if (cdev) {
        vmm_init_printf("change stdio device to %s\n", cdev->name);
        vmm_stdio_change_device(cdev);
    }

    /* Free-up early param */
    if (console_param) {
        vmm_free(console_param);
        console_param     = NULL;
        console_param_len = 0;
    }
}

/**
 * @brief 保存控制台参数字符串
 * @param cdev 控制台设备名称
 * @return 成功返回VMM_OK，否则返回错误码
 */
static int console_param_save(char *cdev)
{
    console_param_len = strlen(cdev) + 1;
    console_param     = vmm_zalloc(console_param_len);

    if (!console_param) {
        return VMM_ERR_NOMEM;
    }

    strncpy(console_param, cdev, console_param_len);
    console_param[console_param_len - 1] = '\0';
    return VMM_OK;
}

vmm_early_param("vmm." VMM_DEVICE_TREE_CONSOLE_ATTR_NAME "=", console_param_save);

static char  *rtcdev_param     = NULL;
static size_t rtcdev_param_len = 0;
#if defined(CONFIG_RTC)
/**
 * @brief 使用RTC设备同步墙钟时间
 * @param rdev RTC设备指针
 */
static void rtcdev_sync_wall_clock(struct rtc_device *rdev)
{
    int ret;

    ret = rtc_device_sync_wall_clock(rdev);

    if (ret) {
        vmm_init_printf(
            "syncup wall_clock using %s "
            "(error %d)\n",
            rdev->name, ret);
    } else {
        vmm_init_printf("syncup wall_clock using %s\n", rdev->name);
    }
}

/**
 * @brief RTC设备迭代函数，使用第一个RTC设备
 * @param rdev RTC设备指针
 * @param data 私有数据指针
 * @return 返回VMM_OK
 */
static int rtcdev_use_first_iter(struct rtc_device *rdev, void *data)
{
    bool *done = data;

    if (*done) {
        goto skip;
    }

    /* Syncup wall_clock time with first rtc device */
    rtcdev_sync_wall_clock(rdev);

    *done = TRUE;

skip:
    return VMM_OK;
}

/**
 * @brief 使用第一个RTC设备同步时间
 */
static void rtcdev_use_first(void)
{
    bool done = FALSE;

    rtc_device_iterate(NULL, &done, rtcdev_use_first_iter);
}
#else
/**
 * @brief 使用第一个可用的RTC设备
 */
static void rtcdev_use_first(void) {}
#endif
/**
 * @brief 处理RTC设备参数
 * @param str RTC设备名称字符串
 */
static void rtcdev_param_process(const char *str)
{
#if defined(CONFIG_RTC)
    struct rtc_device      *rdev;
    vmm_device_tree_node_t *node;

    /* Find rtc device based on rtc_device attribute */
    if (!(rdev = rtc_device_find(str))) {
        if ((node = vmm_device_tree_getnode(str))) {
            rdev = rtc_device_find(node->name); /**< rtc_device_find(node->name)成员 */
            vmm_device_tree_dref_node(node);
        }
    }

    /* Syncup wall_clock time with chosen rtc device */
    if (rdev) {
        rtcdev_sync_wall_clock(rdev);
    }

#endif

    /* Free-up early param */
    if (rtcdev_param) {
        vmm_free(rtcdev_param);
        rtcdev_param     = NULL;
        rtcdev_param_len = 0;
    }
}

/**
 * @brief 保存RTC参数字符串
 * @param rdev RTC设备名称
 * @return 成功返回VMM_OK，否则返回错误码
 */
static int rtcdev_param_save(char *rdev)
{
    rtcdev_param_len = strlen(rdev) + 1;
    rtcdev_param     = vmm_zalloc(rtcdev_param_len);

    if (!rtcdev_param) {
        return VMM_ERR_NOMEM;
    }

    strncpy(rtcdev_param, rdev, rtcdev_param_len);
    rtcdev_param[rtcdev_param_len - 1] = '\0';
    return VMM_OK;
}

vmm_early_param("vmm." VMM_DEVICE_TREE_RTCDEV_ATTR_NAME "=", rtcdev_param_save);

static char  *bootcmd_param     = NULL;
static size_t bootcmd_param_len = 0;

/**
 * @brief 处理引导命令参数，执行一系列引导命令
 * @param str 引导命令字符串
 * @param str_len 字符串长度
 */
static void bootcmd_param_process(const char *str, size_t str_len)
{
#define BOOTCMD_WIDTH 256
    char bcmd[BOOTCMD_WIDTH];

    /* For each boot command */
    while (str_len) {
        /* Print boot command */
        vmm_init_printf("%s: %s\n", VMM_DEVICE_TREE_BOOTCMD_ATTR_NAME, str);
        /* Execute boot command */
        strlcpy(bcmd, str, sizeof(bcmd));
        vmm_command_manager_execute_cmdstr(vmm_stdio_device(), bcmd, NULL);
        /* Next boot command */
        str_len -= strlen(str) + 1;
        str += strlen(str) + 1;
    }

    /* Free-up early param */
    if (bootcmd_param) {
        vmm_free(bootcmd_param);
        bootcmd_param     = NULL;
        bootcmd_param_len = 0;
    }
}

/**
 * @brief 保存引导命令参数字符串
 * @param cmds 引导命令字符串
 * @return 成功返回VMM_OK，否则返回错误码
 */
static int bootcmd_param_save(char *cmds)
{
    size_t i;

    bootcmd_param_len = strlen(cmds) + 1;
    bootcmd_param     = vmm_zalloc(bootcmd_param_len);

    if (!bootcmd_param) {
        return VMM_ERR_NOMEM;
    }

    strncpy(bootcmd_param, cmds, bootcmd_param_len);
    bootcmd_param[bootcmd_param_len - 1] = '\0';

    for (i = 0; i < bootcmd_param_len; i++) {
        if (bootcmd_param[i] == ';') {
            bootcmd_param[i] = '\0';
        }
    }

    return VMM_OK;
}

vmm_early_param("vmm." VMM_DEVICE_TREE_BOOTCMD_ATTR_NAME "=", bootcmd_param_save);

/**
 * @brief 检查系统初始化是否完成
 * @return 如果完成返回true，否则返回false
 */
bool vmm_init_done(void)
{
    return sys_init_done;
}

/**
 * @brief 执行系统后初始化工作，包括处理控制台、RTC和引导命令
 * @param work 工作队列指针
 */
static void system_postinit_work(vmm_work_t *work)
{
    const char             *str;
    uint32_t c;
    uint32_t freed;
    vmm_device_tree_node_t *node = NULL;
    vmm_device_tree_node_t *node1 = NULL;

    /* Print status of present host CPUs */
    for_each_present_cpu(c)
    {
        if (vmm_cpu_online(c)) {
            vmm_init_printf("CPU%d online\n", c);
        } else {
            vmm_init_printf("CPU%d possible\n", c);
        }
    }
    vmm_init_printf("brought-up %d CPUs\n", vmm_num_online_cpus());

    /* Free init memory */
    freed = vmm_host_free_initmem();
    vmm_init_printf("freeing init memory %dK\n", freed);

    /* Find chosen node */
    node = vmm_device_tree_getnode(VMM_DEVICE_TREE_PATH_SEPARATOR_STRING VMM_DEVICE_TREE_CHOSEN_NODE_NAME);

    /* Process console device */
    str  = NULL;

    if (console_param) {
        /* Process console device passed via bootargs */
        console_param_process(console_param);
    } else if (node && vmm_device_tree_read_string(node, VMM_DEVICE_TREE_CONSOLE_ATTR_NAME, &str) == VMM_OK) {
        /* Process console device passed via console DT property */
        console_param_process(str);
    } else if (node && vmm_device_tree_read_string(node, VMM_DEVICE_TREE_STDOUT_ATTR_NAME, &str) == VMM_OK) {
        /* Process console device passed via stdout-path DT property */
        node1 = vmm_device_tree_getnode(str);

        if (node1) {
            console_param_process(node1->name);
            vmm_device_tree_dref_node(node1);
        }
    }

    /* Process rtc device */
    str = NULL;

    if (rtcdev_param) {
        /* Process rtc device passed via bootargs */
        rtcdev_param_process(rtcdev_param);
    } else if (node && vmm_device_tree_read_string(node, VMM_DEVICE_TREE_RTCDEV_ATTR_NAME, &str) == VMM_OK) {
        /* Process rtc device passed via rtcdev DT property */
        rtcdev_param_process(str);
    } else {
        /* Process first rtc device */
        rtcdev_use_first();
    }

    str = NULL;

    if (bootcmd_param) {
        /* Process boot commands passed via bootargs */
        bootcmd_param_process(bootcmd_param, bootcmd_param_len);
    } else if (node && vmm_device_tree_read_string(node, VMM_DEVICE_TREE_BOOTCMD_ATTR_NAME, &str) == VMM_OK) {
        /* Process boot commands passed via bootcmd DT property */
        bootcmd_param_process(str, vmm_device_tree_attrlen(node, VMM_DEVICE_TREE_BOOTCMD_ATTR_NAME));
    }

    /* De-reference chosen node */
    if (node) {
        vmm_device_tree_dref_node(node);
    }

    /* Set system init done flag */
    sys_init_done = TRUE;
}

/**
 * @brief 执行系统初始化工作，初始化各种子系统
 * @param work 工作队列指针
 */
static void system_init_work(vmm_work_t *work)
{
    int ret;
#if defined(CONFIG_SMP)
    uint32_t c;
#endif

    /* Initialize wall_clock */
    vmm_init_printf("wall_clock subsystem\n");
    ret = vmm_wall_clock_init();

    if (ret) {
        goto fail;
    }

#if defined(CONFIG_SMP)
    /* Start each present secondary CPUs */
    vmm_init_printf("start secondary CPUs\n");
    for_each_present_cpu(c)
    {
        if (c == vmm_smp_bootcpu_id()) {
            continue;
        }

        ret = arch_smp_start_cpu(c);

        if (ret) {
            vmm_init_printf("failed to start CPU%d (error %d)\n", c, ret);
        }
    }

#ifdef CONFIG_LOAD_BALANCER
    /* Initialize hypervisor load balancer */
    vmm_init_printf("hypervisor load balancer\n");
    ret = vmm_load_balancer_init();

    if (ret) {
        goto fail;
    }

#endif
#endif

    /* Initialize command manager */
    vmm_init_printf("command manager\n");
    ret = vmm_command_manager_init();

    if (ret) {
        goto fail;
    }

    /* Initialize device driver framework */
    vmm_init_printf("device driver framework\n");
    ret = vmm_device_driver_init();

    if (ret) {
        goto fail;
    }

    /* Initialize device emulation framework */
    vmm_init_printf("device emulation framework\n");
    ret = vmm_device_emulate_init();

    if (ret) {
        goto fail;
    }

    /* Initialize character device framework */
    vmm_init_printf("character device framework\n");
    ret = vmm_char_device_init();

    if (ret) {
        goto fail;
    }

#if defined(CONFIG_SMP)
    /* Poll for all present CPUs to become online */
    /* Note: There is a timeout of 1 second */
    /* Note: The modules might use SMP IPIs or might have per-cpu context
     * so, we do this before vmm_modules_init() in-order to make sure that
     * correct number of online CPUs are visible to all modules.
     */
    ret = 1000;

    while (ret--) {
        int all_cpu_online = 1;

        for_each_present_cpu(c)
        {
            if (!vmm_cpu_online(c)) {
                all_cpu_online = 0;
            }
        }

        if (all_cpu_online) {
            break;
        }

        vmm_mdelay(1);
    }

#endif

    /* Initialize IOMMU framework */
    vmm_init_printf("iommu framework\n");
    ret = vmm_iommu_init();

    if (ret) {
        goto fail;
    }

    /* Initialize hypervisor modules */
    vmm_init_printf("hypervisor modules\n");
    ret = vmm_modules_init();

    if (ret) {
        goto fail;
    }

    /* Initialize cpu final */
    vmm_init_printf("CPU final\n");
    ret = arch_cpu_final_init();

    if (ret) {
        goto fail;
    }

    /* Initialize board final */
    vmm_init_printf("board final\n");
    ret = arch_board_final_init();

    if (ret) {
        goto fail;
    }

    /* Call final init functions */
    vmm_init_printf("final functions\n");
    ret = vmm_initfn_final();

    if (ret) {
        goto fail;
    }

    /* Schedule system post-init work */
    INIT_WORK(&sys_postinit, &system_postinit_work);
    vmm_workqueue_schedule_work(NULL, &sys_postinit);

    return;

fail:
    vmm_panic("%s: error %d\n", __func__, ret);
}

/**
 * @brief 初始化引导CPU，设置系统基础组件
 * @return 无返回值
 */
static void __init init_bootcpu(void)
{
    int                     ret;
    vmm_device_tree_node_t *node;

    /* Sanity check on SMP processor id */
    if (CONFIG_CPU_COUNT <= vmm_smp_processor_id()) {
        vmm_hang();
    }

    /* Mark this CPU possible & present */
    vmm_set_cpu_possible(vmm_smp_processor_id(), TRUE);
    vmm_set_cpu_present(vmm_smp_processor_id(), TRUE);

    /* Print version string */
    vmm_printf("\n");
    vmm_printver();
    vmm_printf("\n");

    /* Initialize host address space */
    vmm_init_printf("host address space\n");
    ret = vmm_host_address_space_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize normal heap */
    vmm_init_printf("heap management\n");
    ret = vmm_heap_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize device tree */
    vmm_init_printf("device tree\n");
    ret = vmm_device_tree_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize device tree based reserved-memory */
    vmm_init_printf("device tree reserved-memory\n");
    ret = vmm_device_tree_reserved_memory_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize DMA heap */
    vmm_init_printf("DMA heap management\n");
    ret = vmm_dma_heap_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize CPU nascent */
    vmm_init_printf("CPU nascent\n");
    ret = arch_cpu_nascent_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize Board nascent */
    vmm_init_printf("board nascent\n");
    ret = arch_board_nascent_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Call nascent init functions */
    vmm_init_printf("nascent funtions\n");
    ret = vmm_initfn_nascent();

    if (ret) {
        goto init_bootcpu_fail;
    }

    vmm_init_printf("page pool\n");
    ret = vmm_page_pool_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize exception table */
    vmm_init_printf("exception table\n");
    ret = vmm_exception_table_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

#if defined(CONFIG_SMP)
    /* Initialize secondary CPUs */
    vmm_init_printf("discover secondary CPUs\n");
    ret = arch_smp_init_cpus();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Prepare secondary CPUs */
    ret = arch_smp_prepare_cpus(vmm_num_possible_cpus());

    if (ret) {
        goto init_bootcpu_fail;
    }

#endif

    /* Initialize per-cpu area */
    vmm_init_printf("per-CPU areas\n");
    ret = vmm_per_cpu_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize per-cpu area */
    vmm_init_printf("CPU hotplug\n");
    ret = vmm_cpu_hotplug_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Set CPU hotplug to online state */
    ret = vmm_cpu_hotplug_set_state(VMM_CPU_HOTPLUG_STATE_ONLINE);

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Make sure /guests and /vmm nodes are present */
    node = vmm_device_tree_getnode(VMM_DEVICE_TREE_PATH_SEPARATOR_STRING VMM_DEVICE_TREE_GUESTINFO_NODE_NAME);

    if (!node) {
        vmm_device_tree_addnode(NULL, VMM_DEVICE_TREE_GUESTINFO_NODE_NAME);
    } else {
        vmm_device_tree_dref_node(node);
    }

    node = vmm_device_tree_getnode(VMM_DEVICE_TREE_PATH_SEPARATOR_STRING VMM_DEVICE_TREE_VMMINFO_NODE_NAME);

    if (!node) {
        vmm_device_tree_addnode(NULL, VMM_DEVICE_TREE_VMMINFO_NODE_NAME);
    } else {
        vmm_device_tree_dref_node(node);
    }

    /* Initialize host interrupts */
    vmm_init_printf("host irq subsystem\n");
    ret = vmm_host_irq_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize CPU early */
    vmm_init_printf("CPU early\n");
    ret = arch_cpu_early_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize Board early */
    vmm_init_printf("board early\n");
    ret = arch_board_early_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Call early init functions */
    vmm_init_printf("early funtions\n");
    ret = vmm_initfn_early();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize standerd input/output */
    vmm_init_printf("standard I/O\n");
    ret = vmm_stdio_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize clocksource manager */
    vmm_init_printf("clocksource manager\n");
    ret = vmm_clocksource_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize clockchip manager */
    vmm_init_printf("clockchip manager\n");
    ret = vmm_clock_chip_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize hypervisor timer */
    vmm_init_printf("hypervisor timer\n");
    ret = vmm_timer_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize hypervisor soft delay */
    vmm_init_printf("hypervisor soft delay\n");
    ret = vmm_delay_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize hypervisor shared memory */
    vmm_init_printf("hypervisor shared memory\n");
    ret = vmm_share_memory_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Initialize hypervisor manager */
    vmm_init_printf("hypervisor manager\n");
    ret = vmm_manager_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

#if defined(CONFIG_SMP)
    /* Initialize synchronus inter-processor interrupts */
    vmm_init_printf("synchronus inter-processor interrupts\n");
    ret = vmm_smp_sync_ipi_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

#endif

    /* Initialize hypervisor scheduler */
    vmm_init_printf("hypervisor scheduler\n");
    ret = vmm_scheduler_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

#if defined(CONFIG_SMP)
    /* Initialize asynchronus inter-processor interrupts */
    vmm_init_printf("asynchronus inter-processor interrupts\n");
    ret = vmm_smp_async_ipi_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

#endif

    /* Initialize hypervisor threads */
    vmm_init_printf("hypervisor threads\n");
    ret = vmm_threads_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

#ifdef CONFIG_PROFILE
    /* Initialize hypervisor profiler */
    vmm_init_printf("hypervisor profiler\n");
    ret = vmm_profiler_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

#endif

    /* Initialize workqueue framework */
    vmm_init_printf("workqueue framework\n");
    ret = vmm_workqueue_init();

    if (ret) {
        goto init_bootcpu_fail;
    }

    /* Schedule system init work */
    INIT_WORK(&sys_init, &system_init_work);
    vmm_workqueue_schedule_work(NULL, &sys_init);

    /* Start timer (Must be last step) */
    vmm_timer_start();

    /* Wait here till scheduler gets invoked by timer */
    vmm_hang();

init_bootcpu_fail:
    vmm_printf("%s: error %d\n", __func__, ret);
    vmm_hang();
}

#if defined(CONFIG_SMP)
/**
 * @brief 初始化辅助CPU
 * @return 无返回值
 */
static void __cpuinit init_secondary(void)
{
    int ret;

    /* Sanity check on SMP processor ID */
    if (CONFIG_CPU_COUNT <= vmm_smp_processor_id()) {
        vmm_hang();
    }

    /* Initialize host virtual address space */
    ret = vmm_host_address_space_init();

    if (ret) {
        vmm_hang();
    }

    /* Set CPU hotplug to online state */
    ret = vmm_cpu_hotplug_set_state(VMM_CPU_HOTPLUG_STATE_ONLINE);

    if (ret) {
        vmm_hang();
    }

    /* Inform architecture code about secondary cpu */
    arch_smp_postboot();

    /* Start timer (Must be last step) */
    vmm_timer_start();

    /* Wait here till scheduler gets invoked by timer */
    vmm_hang();
}
#endif

/**
 * @brief 主初始化函数，根据CPU类型调用相应初始化
 * @return 无返回值
 */
void __cpuinit vmm_init(void)
{
#if defined(CONFIG_SMP)
    /* Try to mark current CPU as Boot CPU
     * Note: This will only work on first CPU.
     */
    vmm_smp_set_bootcpu();

    if (!vmm_init_done() && vmm_smp_is_bootcpu()) {
        /* Boot CPU */
        init_bootcpu();
    } else {
        /* Secondary CPUs */
        init_secondary();
    }

#else
    /* Boot CPU */
    init_bootcpu();
#endif
}

/**
 * @brief 停止系统，停止调度器和定时器
 */
static void system_stop(void)
{
    /* Stop scheduler */
    vmm_printf("Stopping Hypervisor Timer\n");
    vmm_timer_stop();

    /* FIXME: Do other cleanup stuff. */
}

static int (*system_reset)(void) = NULL;

/**
 * @brief 注册系统复位回调函数
 * @param callback 复位回调函数指针
 */
void vmm_register_system_reset(int (*callback)(void))
{
    system_reset = callback;
}

/**
 * @brief 执行系统复位
 */
void vmm_reset(void)
{
    int rc;

    /* Stop the system */
    system_stop();

    /* Issue system reset */
    if (!system_reset) {
        vmm_printf("Error: no system reset callback.\n");
        vmm_printf("Please reset system manually ...\n");
    } else {
        vmm_printf("Issuing System Reset\n");

        if ((rc = system_reset())) {
            vmm_printf("Error: reset failed (error %d)\n", rc);
        }
    }

    /* Wait here. Nothing else to do. */
    vmm_hang();
}

static int (*system_shutdown)(void) = NULL;

/**
 * @brief 注册系统关机回调函数
 * @param callback 关机回调函数指针
 */
void vmm_register_system_shutdown(int (*callback)(void))
{
    system_shutdown = callback;
}

/**
 * @brief 执行系统关机
 */
void vmm_shutdown(void)
{
    int rc;

    /* Stop the system */
    system_stop();

    /* Issue system shutdown */
    if (!system_shutdown) {
        vmm_printf("Error: no system shutdown callback.\n");
        vmm_printf("Please shutdown system manually ...\n");
    } else {
        vmm_printf("Issuing System Shutdown\n");

        if ((rc = system_shutdown())) {
            vmm_printf("Error: shutdown failed (error %d)\n", rc);
        }
    }

    /* Wait here. Nothing else to do. */
    vmm_hang();
}
