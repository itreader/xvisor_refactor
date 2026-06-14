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
 * @file kern3.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief kern3 test implementation
 *
 * This tests the scheduling of threads at different priorities, and
 * preemption of lower priority threads by higher priority threads.
 *
 * Much of this functionality is already tested implicitly by the
 * semaphore, mutex tests etc but we repeat it here within the kernel
 * tests for completeness.
 *
 * Two threads are created at different priorities, with each thread
 * setting a running flag whenever it runs. We check that when the
 * higher priority thread is ready to run, only the higher priority
 * thread's running flag is set (even though the lower priority
 * thread should also be setting it at this time). This checks that
 * the scheduler is correctly prioritising thread execution.
 *
 * The test also exercises preemption, by disabling setting of the
 * running flag in the higher priority thread for a period. During
 * this time the higher priority thread repeatedly sleeps for one
 * system tick then wakes up to check the sleep-request flag again.
 * Every time the higher priority thread wakes up, it has preempted
 * the lower priority thread (which is always running). By ensuring
 * that the higher priority thread is able to start running again
 * after one of these periods (through checking the running flag)
 * we prove that the preemption has worked.
 *
 * This source has been largely adapted from Atomthreads Sources:
 * <atomthreads_source>/tests/kern3.c
 *
 * For more info visit: http://atomthreads.com
 */

#include <libs/stringlib.h>
#include <libs/white_box_test.h>
#include <vmm_delay.h>
#include <vmm_error.h>
#include <vmm_manager.h>
#include <vmm_modules.h>
#include <vmm_scheduler.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_threads.h>

#define MODULE_DESC      "kern3 test"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (WBOXTEST_IPRIORITY + 1)
#define MODULE_INIT      kern3_init
#define MODULE_EXIT      kern3_exit

/* Number of threads */
#define NUM_THREADS      2

/* Sleep delay in milliseconds */
#define SLEEP_MSECS      (VMM_THREAD_DEF_TIME_SLICE / 1000000ULL)

/* Global data (one per thread) */
static vmm_thread_t *workers[NUM_THREADS];
static volatile bool running_flag[NUM_THREADS];
static volatile bool sleep_request[NUM_THREADS];

static int kern3_worker_thread_main(void *data)
{
    /* Pull out thread ID */
    int thread_id = (int)(uint64_t)data;

    /* Run forever */
    while (1) {
        /* If this thread is requested to sleep, sleep until told to stop */
        if (sleep_request[thread_id]) {
            /* Clear running flag for this thread */
            running_flag[thread_id] = FALSE;
            /* Sleep for some time */
            vmm_msleep(SLEEP_MSECS);
        } else {
            /* Set running flag for this thread */
            running_flag[thread_id] = TRUE;
        }
    }

    return 0;
}

static int kern3_do_test(vmm_char_device_t *cdev)
{
    int i, failures = 0;

    /* Start workers */
    vmm_threads_start(workers[0]);
    vmm_threads_start(workers[1]);

    /* Give any running threads time to set their flag */
    vmm_msleep(SLEEP_MSECS * 10);

    /* Repeat test a few times */
    for (i = 0; i < 10; i++) {
        /* Make the higher priority thread sleep */
        sleep_request[0] = TRUE;

        /* Give any running threads time to set their flag */
        vmm_msleep(SLEEP_MSECS * 10);

        /*
         * Check only the low priority thread has
         * run since we reset the flags
         */
        if ((running_flag[1] != TRUE) || (running_flag[0] != FALSE)) {
            vmm_cdev_printf(cdev, "error: lo%d %d/%d\n", i, running_flag[0], running_flag[1]);
            failures++;
            break;
        } else {
            /*
             * We have confirmed that only the ready thread
             * has been running. Now check that if we wake up
             * the high priority thread, the low priority one
             * stops running and only the high priority one does.
             */

            /* Tell the higher priority thread to stop sleeping */
            sleep_request[0] = FALSE;

            /* Give any running threads time to set their flag */
            vmm_msleep(SLEEP_MSECS * 10);

            /* Reset the running flag for both threads */
            running_flag[0] = running_flag[1] = FALSE;

            /* Give any running threads time to set their flag */
            vmm_msleep(SLEEP_MSECS * 10);

            /*
             * Check only the high priority thread has run
             * since we reset the flags
             */
            if ((running_flag[0] != TRUE) || (running_flag[1] != FALSE)) {
                vmm_cdev_printf(cdev, "error: hi%d %d/%d\n", i, running_flag[0], running_flag[1]);
                failures++;
                break;
            } else {
                /*
                 * We have confirmed that the high priority
                 * thread has preempted the low priority
                 * thread, and remain running while never
                 * scheduling the lower one back in.
                 */
            }
        }
    }

    /* Stop workers */
    vmm_threads_stop(workers[0]);
    vmm_threads_stop(workers[1]);

    return (failures) ? VMM_ERR_FAIL : 0;
}

static int kern3_run(struct white_box_test *test, vmm_char_device_t *cdev, uint32_t test_hcpu)
{
    int                  i, ret = VMM_OK;
    char                 wname[VMM_FIELD_NAME_SIZE];
    uint8_t              current_priority = vmm_scheduler_current_priority();
    const vmm_cpumask_t *old_mask         = vmm_manager_vcpu_get_affinity(vmm_scheduler_current_vcpu());
    const vmm_cpumask_t *cpu_mask         = vmm_cpumask_of(test_hcpu);

    /* Ensure we have sufficiently higher priority */
    if ((current_priority - VMM_THREAD_MIN_PRIORITY + 1) < NUM_THREADS) {
        vmm_cdev_printf(
            cdev,
            "Current priority %d non-sufficient to "
            "create %d threads of lower priority\n",
            (uint32_t)current_priority, NUM_THREADS);
        return VMM_ERR_INVALID;
    }

    /* Initialise global data */
    memset(workers, 0, sizeof(workers));

    for (i = 0; i < NUM_THREADS; i++) {
        running_flag[i]  = FALSE;
        sleep_request[i] = FALSE;
    }

    /* Create worker threads */
    for (i = 0; i < NUM_THREADS; i++) {
        vmm_snprintf(wname, VMM_FIELD_NAME_SIZE, "kern3_worker%d", i);
        workers[i] = vmm_threads_create(wname, kern3_worker_thread_main, (void *)(uint64_t)i, current_priority - i, VMM_THREAD_DEF_TIME_SLICE);

        if (workers[i] == NULL) {
            ret = VMM_ERR_FAIL;
            goto destroy_workers;
        }

        vmm_threads_set_affinity(workers[i], cpu_mask);
    }

    /* Set current VCPU affinity same as worker thead affinity */
    ret = vmm_manager_vcpu_set_affinity(vmm_scheduler_current_vcpu(), cpu_mask);

    if (ret) {
        goto destroy_workers;
    }

    /* Do the test */
    ret = kern3_do_test(cdev);

    /* Restore current VCPU affinity */
    vmm_manager_vcpu_set_affinity(vmm_scheduler_current_vcpu(), old_mask);

    /* Destroy worker threads */
destroy_workers:

    for (i = 0; i < NUM_THREADS; i++) {
        if (workers[i]) {
            vmm_threads_destroy(workers[i]);
            workers[i] = NULL;
        }
    }

    return ret;
}

static struct white_box_test kern3 = {
    .name = "kern3",
    .run  = kern3_run,
};

static int __init kern3_init(void)
{
    return wboxtest_register("threads", &kern3);
}

static void __exit kern3_exit(void)
{
    wboxtest_unregister(&kern3);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
