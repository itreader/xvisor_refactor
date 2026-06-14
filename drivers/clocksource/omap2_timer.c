/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
 * Copyright (c) 2011 Sukanto Ghosh.
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
 * @file omap2_timer.c
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief OMAP2+ general purpose timers
 *
 * Parts of this source code has been taken from u-boot
 */

#include <vmm_clock_chip.h>
#include <vmm_device_tree.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_host_address_space.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_main.h>
#include <vmm_stdio.h>

#define GPT_TIDR                      0x000
#define GPT_TIDR_TID_REV_S            0
#define GPT_TIDR_TID_REV_M            0x000000FF

#define GPT_TIOCP_CFG                 0x010
#define GPT_TIOCP_CFG_CLOCKACTIVITY_S 8
#define GPT_TIOCP_CFG_CLOCKACTIVITY_M 0x00000300
#define GPT_TIOCP_CFG_EMUFREE_S       5
#define GPT_TIOCP_CFG_EMUFREE_M       0x00000020
#define GPT_TIOCP_CFG_IDLEMODE_S      3
#define GPT_TIOCP_CFG_IDLEMODE_M      0x00000018
#define GPT_TIOCP_CFG_ENAWAKEUP_S     2
#define GPT_TIOCP_CFG_ENAWAKEUP_M     0x00000004
#define GPT_TIOCP_CFG_SOFTRESET_S     1
#define GPT_TIOCP_CFG_SOFTRESET_M     0x00000002
#define GPT_TIOCP_CFG_AUTOIDLE_S      0
#define GPT_TIOCP_CFG_AUTOIDLE_M      0x00000001

#define GPT_TISTAT                    0x014
#define GPT_TISTAT_RESETDONE_S        0
#define GPT_TISTAT_RESETDONE_M        0x00000001

#define GPT_TISR                      0x018
#define GPT_TISR_TCAR_IT_FLAG_S       2
#define GPT_TISR_TCAR_IT_FLAG_M       0x00000004
#define GPT_TISR_OVF_IT_FLAG_S        1
#define GPT_TISR_OVF_IT_FLAG_M        0x00000002
#define GPT_TISR_MAT_IT_FLAG_S        0
#define GPT_TISR_MAT_IT_FLAG_M        0x00000001

#define GPT_TIER                      0x01C
#define GPT_TIER_TCAR_IT_ENA_S        2
#define GPT_TIER_TCAR_IT_ENA_M        0x00000004
#define GPT_TIER_OVF_IT_ENA_S         1
#define GPT_TIER_OVF_IT_ENA_M         0x00000002
#define GPT_TIER_MAT_IT_ENA_S         0
#define GPT_TIER_MAT_IT_ENA_M         0x00000001

#define GPT_TWER                      0x020
#define GPT_TWER_TCAR_WUP_ENA_S       2
#define GPT_TWER_TCAR_WUP_ENA_M       0x00000004
#define GPT_TWER_OVF_WUP_ENA_S        1
#define GPT_TWER_OVF_WUP_ENA_M        0x00000002
#define GPT_TWER_MAT_WUP_ENA_S        0
#define GPT_TWER_MAT_WUP_ENA_M        0x00000001

#define GPT_TCLR                      0x024
#define GPT_TCLR_GPO_CFG_S            14
#define GPT_TCLR_GPO_CFG_M            0x00004000
#define GPT_TCLR_CAPT_MODE_S          13
#define GPT_TCLR_CAPT_MODE_M          0x00002000
#define GPT_TCLR_PT_S                 12
#define GPT_TCLR_PT_M                 0x00001000
#define GPT_TCLR_TRG_S                10
#define GPT_TCLR_TRG_M                0x00000C00
#define GPT_TCLR_TCM_S                8
#define GPT_TCLR_TCM_M                0x00000300
#define GPT_TCLR_SCPWM_S              7
#define GPT_TCLR_SCPWM_M              0x00000080
#define GPT_TCLR_CE_S                 6
#define GPT_TCLR_CE_M                 0x00000040
#define GPT_TCLR_PRE_S                5
#define GPT_TCLR_PRE_M                0x00000020
#define GPT_TCLR_PTV_S                2
#define GPT_TCLR_PTV_M                0x0000001C
#define GPT_TCLR_AR_S                 1
#define GPT_TCLR_AR_M                 0x00000002
#define GPT_TCLR_ST_S                 0
#define GPT_TCLR_ST_M                 0x00000001

#define GPT_TCRR                      0x028
#define GPT_TCRR_TIMER_COUNTER_S      0
#define GPT_TCRR_TIMER_COUNTER_M      0xFFFFFFFF

#define GPT_TLDR                      0x02C
#define GPT_TLDR_LOAD_VALUE_S         0
#define GPT_TLDR_LOAD_VALUE_M         0xFFFFFFFF

#define GPT_TTGR                      0x030
#define GPT_TTGR_TRIGGER_VALUE_S      0
#define GPT_TTGR_TRIGGER_VALUE_M      0xFFFFFFFF

#define GPT_TWPS                      0x034
#define GPT_TWPS_W_PEND_TOWR_S        9
#define GPT_TWPS_W_PEND_TOWR_M        0x00000200
#define GPT_TWPS_W_PEND_TOCR_S        8
#define GPT_TWPS_W_PEND_TOCR_M        0x00000100
#define GPT_TWPS_W_PEND_TCVR_S        7
#define GPT_TWPS_W_PEND_TCVR_M        0x00000080
#define GPT_TWPS_W_PEND_TNIR_S        6
#define GPT_TWPS_W_PEND_TNIR_M        0x00000040
#define GPT_TWPS_W_PEND_TPIR_S        5
#define GPT_TWPS_W_PEND_TPIR_M        0x00000020
#define GPT_TWPS_W_PEND_TMAR_S        4
#define GPT_TWPS_W_PEND_TMAR_M        0x00000010
#define GPT_TWPS_W_PEND_TTGR_S        3
#define GPT_TWPS_W_PEND_TTGR_M        0x00000008
#define GPT_TWPS_W_PEND_TLDR_S        2
#define GPT_TWPS_W_PEND_TLDR_M        0x00000004
#define GPT_TWPS_W_PEND_TCRR_S        1
#define GPT_TWPS_W_PEND_TCRR_M        0x00000002
#define GPT_TWPS_W_PEND_TCLR_S        0
#define GPT_TWPS_W_PEND_TCLR_M        0x00000001

#define GPT_TMAR                      0x038
#define GPT_TMAR_COMPARE_VALUE_S      0
#define GPT_TMAR_COMPARE_VALUE_M      0xFFFFFFFF

#define GPT_TCAR1                     0x03C
#define GPT_TCAR1_CAPTURE_VALUE1_S    0
#define GPT_TCAR1_CAPTURE_VALUE1_M    0xFFFFFFFF

#define GPT_TSICR                     0x040
#define GPT_TSICR_POSTED_S            2
#define GPT_TSICR_POSTED_M            0x00000004
#define GPT_TSICR_SFT_S               1
#define GPT_TSICR_SFT_M               0x00000002

#define GPT_TCAR2                     0x044
#define GPT_TCAR2_CAPTURE_VALUE2_S    0
#define GPT_TCAR2_CAPTURE_VALUE2_M    0xFFFFFFFF

#define GPT_TPIR                      0x048
#define GPT_TPIR_POSITIVE_INC_VALUE_S 0
#define GPT_TPIR_POSITIVE_INC_VALUE_M 0xFFFFFFFF

#define GPT_TNIR                      0x04C
#define GPT_TNIR_NEGATIVE_INC_VALUE_S 0
#define GPT_TNIR_NEGATIVE_INC_VALUE_M 0xFFFFFFFF

#define GPT_TCVR                      0x050
#define GPT_TCVR_COUNTER_VALUE_S      0
#define GPT_TCVR_COUNTER_VALUE_M      0xFFFFFFFF

#define GPT_TOCR                      0x054
#define GPT_TOCR_OVF_COUNTER_VALUE_S  0
#define GPT_TOCR_OVF_COUNTER_VALUE_M  0x00FFFFFF

#define GPT_TOWR                      0x058
#define GPT_TOWR_OVF_WRAPPING_VALUE_S 0
#define GPT_TOWR_OVF_WRAPPING_VALUE_M 0x00FFFFFF

static void gpt_write(virtual_addr_t base, uint32_t reg, uint32_t val)
{
    vmm_writel(val, (void *)(base + reg));
}

static uint32_t gpt_read(virtual_addr_t base, uint32_t reg)
{
    return vmm_readl((void *)(base + reg));
}

static void gpt_oneshot(virtual_addr_t base)
{
    uint32_t regval;
    /* Disable AR (auto-reload) */
    regval = gpt_read(base, GPT_TCLR);
    regval &= ~GPT_TCLR_AR_M;
    gpt_write(base, GPT_TCLR, regval);
    /* Enable Overflow Interrupt TIER[OVF_IT_ENA] */
    gpt_write(base, GPT_TIER, GPT_TIER_OVF_IT_ENA_M);
}

#if 0
static void gpt_continuous(virtual_addr_t base)
{
    uint32_t regval;
    /* Enable AR (auto-reload) */
    regval = gpt_read(base, GPT_TCLR);
    regval |= GPT_TCLR_AR_M;
    gpt_write(base, GPT_TCLR, regval);
    /* Disable interrupts TIER[OVF_IT_ENA] */
    gpt_write(base, GPT_TIER, 0);
    /* Auto reload value set to 0 */
    gpt_write(base, GPT_TLDR, 0);
    gpt_write(base, GPT_TCRR, 0);
    /* Start Timer (TCLR[ST] = 1) */
    regval = gpt_read(base, GPT_TCLR);
    regval |= GPT_TCLR_ST_M;
    gpt_write(base, GPT_TCLR, regval);
}
#endif

struct gpt_clock_chip {
    virtual_addr_t   gpt_va;
    vmm_clock_chip_t clock_chip;
};

static vmm_irq_return_t gpt_clockevent_irq_handler(int irq_no, void *dev)
{
    uint32_t               regval;
    struct gpt_clock_chip *tcc = dev;

    gpt_write(tcc->gpt_va, GPT_TISR, GPT_TISR_OVF_IT_FLAG_M);

    /* Stop Timer (TCLR[ST] = 0) */
    regval = gpt_read(tcc->gpt_va, GPT_TCLR);
    regval &= ~GPT_TCLR_ST_M;
    gpt_write(tcc->gpt_va, GPT_TCLR, regval);

    tcc->clock_chip.event_handler(&tcc->clock_chip);

    return VMM_IRQ_HANDLED;
}

static void gpt_clock_chip_set_mode(vmm_clock_chip_mode_e mode, vmm_clock_chip_t *cc)
{
    uint32_t               regval;
    struct gpt_clock_chip *tcc = cc->private;

    switch (mode) {
        case VMM_CLOCKCHIP_MODE_ONESHOT:
            gpt_oneshot(tcc->gpt_va);
            break;

        case VMM_CLOCKCHIP_MODE_SHUTDOWN:
            /* Stop Timer (TCLR[ST] = 0) */
            regval = gpt_read(tcc->gpt_va, GPT_TCLR);
            regval &= ~GPT_TCLR_ST_M;
            gpt_write(tcc->gpt_va, GPT_TCLR, regval);
            break;

        case VMM_CLOCKCHIP_MODE_PERIODIC:
        case VMM_CLOCKCHIP_MODE_UNUSED:
        default:
            break;
    }
}

static int gpt_clock_chip_set_next_event(uint64_t next, vmm_clock_chip_t *cc)
{
    uint32_t               regval;
    struct gpt_clock_chip *tcc = cc->private;

    gpt_write(tcc->gpt_va, GPT_TCRR, 0xFFFFFFFF - next);
    /* Start Timer (TCLR[ST] = 1) */
    regval = gpt_read(tcc->gpt_va, GPT_TCLR);
    regval |= GPT_TCLR_ST_M;
    gpt_write(tcc->gpt_va, GPT_TCLR, regval);

    return VMM_OK;
}

static int __init gpt_clock_chip_init(vmm_device_tree_node_t *node)
{
    int                    rc;
    uint32_t               clock, hirq;
    struct gpt_clock_chip *cc;

    /* Read clock frequency */
    rc = vmm_device_tree_clock_frequency(node, &clock);

    if (rc) {
        return rc;
    }

    /* Read irq attribute */
    hirq = vmm_device_tree_irq_parse_map(node, 0);

    if (!hirq) {
        return VMM_ERR_NODEV;
    }

    /* Alloc GPT clockchip */
    cc = vmm_zalloc(sizeof(struct gpt_clock_chip));

    if (!cc) {
        return VMM_ERR_FAIL;
    }

    cc->clock_chip.name = "omap3430-timer";
    cc->clock_chip.hirq = hirq;

    /* Map timer registers */
    rc                  = vmm_device_tree_request_regmap(node, &cc->gpt_va, 0, cc->clock_chip.name);

    if (rc) {
        vmm_free(cc);
        return rc;
    }

    /* Register interrupt handler */
    rc = vmm_host_irq_register(cc->clock_chip.hirq, cc->clock_chip.name, &gpt_clockevent_irq_handler, cc);

    if (rc) {
        vmm_device_tree_regunmap_release(node, cc->gpt_va, 0);
        vmm_free(cc);
        return rc;
    }

    cc->clock_chip.rating   = 200;
    cc->clock_chip.cpumask  = cpu_all_mask;
    cc->clock_chip.features = VMM_CLOCKCHIP_FEAT_ONESHOT;
    vmm_clocks_calc_mult_shift(&cc->clock_chip.mult, &cc->clock_chip.shift, VMM_NSEC_PER_SEC, clock, 10);
    cc->clock_chip.min_delta_ns   = vmm_clock_chip_delta2ns(0xFF, &cc->clock_chip);
    cc->clock_chip.max_delta_ns   = vmm_clock_chip_delta2ns(0xFFFFFFFF, &cc->clock_chip);
    cc->clock_chip.set_mode       = &gpt_clock_chip_set_mode;
    cc->clock_chip.set_next_event = &gpt_clock_chip_set_next_event;
    cc->clock_chip.private        = cc;

    gpt_write(cc->gpt_va, GPT_TCLR, 0);

    rc = vmm_clock_chip_register(&cc->clock_chip);

    if (rc) {
        vmm_host_irq_unregister(cc->clock_chip.hirq, cc);
        vmm_device_tree_regunmap_release(node, cc->gpt_va, 0);
        vmm_free(cc);
        return rc;
    }

    return VMM_OK;
}

VMM_CLOCKCHIP_INIT_DECLARE(omapgptclock_chip, "ti,omap3430-timer", gpt_clock_chip_init);
