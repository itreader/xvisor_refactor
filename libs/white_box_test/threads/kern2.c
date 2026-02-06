/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file kern2.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief kern2 test implementation
 *
 * This is a basic test of the thread context-switch functionality. It
 * creates fourty local variables and schedules the thread out for one
 * second. If context-switch save/restore is not implemented correctly,
 * you might expect one or more of these local variables to be corrupted
 * by the time the thread is scheduled back in.
 *
 * Note that this is a fairly unsophisticated test, and a lot depends
 * on how the compiler deals with the variables, as well as what code
 * is executed while the thread is scheduled out. It should flag up any
 * major problems with the context-switch, however.
 *
 * This source has been largely adapted from Atomthreads Sources:
 * <atomthreads_source>/tests/kern2.c
 *
 * For more info visit: http://atomthreads.com
 */

#include <libs/stringlib.h>
#include <libs/white_box_test.h>
#include <vmm_delay.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_scheduler.h>
#include <vmm_stdio.h>
#include <vmm_threads.h>

#define MODULE_DESC      "kern2 test"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (WBOXTEST_IPRIORITY + 1)
#define MODULE_INIT      kern2_init
#define MODULE_EXIT      kern2_exit

enum kern2_delay_type {
    KERN2_YIELD  = 1,
    KERN2_UDELAY = 2,
    KERN2_MDELAY = 3,
    KERN2_SDELAY = 4,
    KERN2_USLEEP = 5,
    KERN2_MSLEEP = 6,
    KERN2_SSLEEP = 7,
};

static int kern2_do_test(vmm_char_device_t *cdev, uint64_t darg, enum kern2_delay_type dtype)
{
    int      failures;
    uint8_t  one         = 1;
    uint8_t  two         = 2;
    uint8_t  three       = 3;
    uint8_t  four        = 4;
    uint8_t  five        = 5;
    uint8_t  six         = 6;
    uint8_t  seven       = 7;
    uint8_t  eight       = 8;
    uint8_t  nine        = 9;
    uint8_t  ten         = 10;
    uint16_t eleven      = 11;
    uint16_t twelve      = 12;
    uint16_t thirteen    = 13;
    uint16_t fourteen    = 14;
    uint16_t fifteen     = 15;
    uint16_t sixteen     = 16;
    uint16_t seventeen   = 17;
    uint16_t eighteen    = 18;
    uint16_t nineteen    = 19;
    uint16_t twenty      = 20;
    uint32_t twentyone   = 21;
    uint32_t twentytwo   = 22;
    uint32_t twentythree = 23;
    uint32_t twentyfour  = 24;
    uint32_t twentyfive  = 25;
    uint32_t twentysix   = 26;
    uint32_t twentyseven = 27;
    uint32_t twentyeight = 28;
    uint32_t twentynine  = 29;
    uint32_t thirty      = 30;
    uint64_t thirtyone   = 31;
    uint64_t thirtytwo   = 32;
    uint64_t thirtythree = 33;
    uint64_t thirtyfour  = 34;
    uint64_t thirtyfive  = 35;
    uint64_t thirtysix   = 36;
    uint64_t thirtyseven = 37;
    uint64_t thirtyeight = 38;
    uint64_t thirtynine  = 39;
    uint64_t fourty      = 40;

    /* Default to zero failures */
    failures             = 0;

    /* some delay for scheduler to get invoked */
    switch (dtype) {
        case KERN2_YIELD:
            while (darg) {
                vmm_scheduler_yield();
                darg--;
            }

            break;

        case KERN2_UDELAY:
            vmm_udelay(darg);
            break;

        case KERN2_MDELAY:
            vmm_mdelay(darg);
            break;

        case KERN2_SDELAY:
            vmm_sdelay(darg);
            break;

        case KERN2_USLEEP:
            vmm_usleep(darg);
            break;

        case KERN2_MSLEEP:
            vmm_msleep(darg);
            break;

        case KERN2_SSLEEP:
            vmm_ssleep(darg);
            break;
    };

    /* Check all variables contain expected values */
    if (one != 1) {
        vmm_cdev_printf(cdev, "1(%d)\n", (int)one);
        failures++;
    }

    if (two != 2) {
        vmm_cdev_printf(cdev, "2(%d)\n", (int)two);
        failures++;
    }

    if (three != 3) {
        vmm_cdev_printf(cdev, "3(%d)\n", (int)three);
        failures++;
    }

    if (four != 4) {
        vmm_cdev_printf(cdev, "4(%d)\n", (int)four);
        failures++;
    }

    if (five != 5) {
        vmm_cdev_printf(cdev, "5(%d)\n", (int)five);
        failures++;
    }

    if (six != 6) {
        vmm_cdev_printf(cdev, "6(%d)\n", (int)six);
        failures++;
    }

    if (seven != 7) {
        vmm_cdev_printf(cdev, "7(%d)\n", (int)seven);
        failures++;
    }

    if (eight != 8) {
        vmm_cdev_printf(cdev, "8(%d)\n", (int)eight);
        failures++;
    }

    if (nine != 9) {
        vmm_cdev_printf(cdev, "9(%d)\n", (int)nine);
        failures++;
    }

    if (ten != 10) {
        vmm_cdev_printf(cdev, "10(%d)\n", (int)ten);
        failures++;
    }

    if (eleven != 11) {
        vmm_cdev_printf(cdev, "11(%d)\n", (int)eleven);
        failures++;
    }

    if (twelve != 12) {
        vmm_cdev_printf(cdev, "12(%d)\n", (int)twelve);
        failures++;
    }

    if (thirteen != 13) {
        vmm_cdev_printf(cdev, "13(%d)\n", (int)thirteen);
        failures++;
    }

    if (fourteen != 14) {
        vmm_cdev_printf(cdev, "14(%d)\n", (int)fourteen);
        failures++;
    }

    if (fifteen != 15) {
        vmm_cdev_printf(cdev, "15(%d)\n", (int)fifteen);
        failures++;
    }

    if (sixteen != 16) {
        vmm_cdev_printf(cdev, "16(%d)\n", (int)sixteen);
        failures++;
    }

    if (seventeen != 17) {
        vmm_cdev_printf(cdev, "17(%d)\n", (int)seventeen);
        failures++;
    }

    if (eighteen != 18) {
        vmm_cdev_printf(cdev, "18(%d)\n", (int)eighteen);
        failures++;
    }

    if (nineteen != 19) {
        vmm_cdev_printf(cdev, "19(%d)\n", (int)nineteen);
        failures++;
    }

    if (twenty != 20) {
        vmm_cdev_printf(cdev, "20(%d)\n", (int)twenty);
        failures++;
    }

    if (twentyone != 21) {
        vmm_cdev_printf(cdev, "21(%d)\n", (int)twentyone);
        failures++;
    }

    if (twentytwo != 22) {
        vmm_cdev_printf(cdev, "22(%d)\n", (int)twentytwo);
        failures++;
    }

    if (twentythree != 23) {
        vmm_cdev_printf(cdev, "23(%d)\n", (int)twentythree);
        failures++;
    }

    if (twentyfour != 24) {
        vmm_cdev_printf(cdev, "24(%d)\n", (int)twentyfour);
        failures++;
    }

    if (twentyfive != 25) {
        vmm_cdev_printf(cdev, "25(%d)\n", (int)twentyfive);
        failures++;
    }

    if (twentysix != 26) {
        vmm_cdev_printf(cdev, "26(%d)\n", (int)twentysix);
        failures++;
    }

    if (twentyseven != 27) {
        vmm_cdev_printf(cdev, "27(%d)\n", (int)twentyseven);
        failures++;
    }

    if (twentyeight != 28) {
        vmm_cdev_printf(cdev, "28(%d)\n", (int)twentyeight);
        failures++;
    }

    if (twentynine != 29) {
        vmm_cdev_printf(cdev, "29(%d)\n", (int)twentynine);
        failures++;
    }

    if (thirty != 30) {
        vmm_cdev_printf(cdev, "30(%d)\n", (int)thirty);
        failures++;
    }

    if (thirtyone != 31) {
        vmm_cdev_printf(cdev, "31(%d)\n", (int)thirtyone);
        failures++;
    }

    if (thirtytwo != 32) {
        vmm_cdev_printf(cdev, "32(%d)\n", (int)thirtytwo);
        failures++;
    }

    if (thirtythree != 33) {
        vmm_cdev_printf(cdev, "33(%d)\n", (int)thirtythree);
        failures++;
    }

    if (thirtyfour != 34) {
        vmm_cdev_printf(cdev, "34(%d)\n", (int)thirtyfour);
        failures++;
    }

    if (thirtyfive != 35) {
        vmm_cdev_printf(cdev, "35(%d)\n", (int)thirtyfive);
        failures++;
    }

    if (thirtysix != 36) {
        vmm_cdev_printf(cdev, "36(%d)\n", (int)thirtysix);
        failures++;
    }

    if (thirtyseven != 37) {
        vmm_cdev_printf(cdev, "37(%d)\n", (int)thirtyseven);
        failures++;
    }

    if (thirtyeight != 38) {
        vmm_cdev_printf(cdev, "38(%d)\n", (int)thirtyeight);
        failures++;
    }

    if (thirtynine != 39) {
        vmm_cdev_printf(cdev, "39(%d)\n", (int)thirtynine);
        failures++;
    }

    if (fourty != 40) {
        vmm_cdev_printf(cdev, "40(%d)\n", (int)fourty);
        failures++;
    }

    return (failures) ? VMM_EFAIL : VMM_OK;
}

static int kern2_run(struct white_box_test *test, vmm_char_device_t *cdev, uint32_t test_hcpu)
{
    int rc;

    rc = kern2_do_test(cdev, 10, KERN2_YIELD);

    if (rc) {
        vmm_cdev_printf(cdev, "kern2 yield() failed\n");
        return rc;
    };

    rc = kern2_do_test(cdev, 1000000, KERN2_UDELAY);

    if (rc) {
        vmm_cdev_printf(cdev, "kern2 udelay() failed\n");
        return rc;
    };

    rc = kern2_do_test(cdev, 1000, KERN2_MDELAY);

    if (rc) {
        vmm_cdev_printf(cdev, "kern2 mdelay() failed\n");
        return rc;
    };

    rc = kern2_do_test(cdev, 1, KERN2_SDELAY);

    if (rc) {
        vmm_cdev_printf(cdev, "kern2 sdelay() failed\n");
        return rc;
    };

    rc = kern2_do_test(cdev, 1000000, KERN2_USLEEP);

    if (rc) {
        vmm_cdev_printf(cdev, "kern2 usleep() failed\n");
        return rc;
    };

    rc = kern2_do_test(cdev, 1000, KERN2_MSLEEP);

    if (rc) {
        vmm_cdev_printf(cdev, "kern2 msleep() failed\n");
        return rc;
    };

    rc = kern2_do_test(cdev, 1, KERN2_SSLEEP);

    if (rc) {
        vmm_cdev_printf(cdev, "kern2 ssleep() failed\n");
        return rc;
    };

    return 0;
}

static struct white_box_test kern2 = {
    .name = "kern2",
    .run  = kern2_run,
};

static int __init kern2_init(void)
{
    return wboxtest_register("threads", &kern2);
}

static void __exit kern2_exit(void)
{
    wboxtest_unregister(&kern2);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
