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
 * @file semaphore5.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief semaphore5 test implementation
 *
 * This test exercise the semaphore wait timeout feature for a thread
 * that blocks on semaphore.
 */

#include <libs/stringlib.h>
#include <libs/white_box_test.h>
#include <vmm_delay.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_scheduler.h>
#include <vmm_semaphore.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_threads.h>
#include <vmm_timer.h>

#define MODULE_DESC      "semaphore5 test"
#define MODULE_AUTHOR    "Anup Patel"
#define MODULE_LICENSE   "GPL"
#define MODULE_IPRIORITY (WBOXTEST_IPRIORITY + 1)
#define MODULE_INIT      semaphore5_init
#define MODULE_EXIT      semaphore5_exit

/* Number of threads */
#define NUM_THREADS      1

/* Sleep delay in milliseconds */
#define SLEEP_MSECS      (VMM_THREAD_DEF_TIME_SLICE / 1000000ULL)

/* Global data */
static vmm_thread_t *workers[NUM_THREADS];
static DEFINE_SEMAPHORE(s1, 3, 3);
static int shared_data[NUM_THREADS];

static int semaphore5_worker_thread_main(void *data)
{
    int rc;
    int i, thread_id = (int)(uint64_t)data;

    for (i = 0; i < 3; i++) {
        rc = vmm_semaphore_down(&s1);

        if (rc) {
            return rc;
        }
    }

    shared_data[thread_id] = 1;

    while (1) {
        vmm_msleep(SLEEP_MSECS);
    }

    return 0;
}

static int semaphore5_do_test(vmm_char_device_t *cdev)
{
    int      i, rc, failures = 0;
    uint64_t timeout, etimeout, tstamp;

    /* Clear shared data */
    for (i = 0; i < NUM_THREADS; i++) {
        shared_data[i] = 0;
    }

    /* s1 semaphore should be available */
    if (vmm_semaphore_avail(&s1) != 3) {
        vmm_cdev_printf(cdev, "error: initial semaphore not available\n");
        failures++;
    }

    /* Start worker0 */
    vmm_threads_start(workers[0]);

    /* Wait for worker0 to acquire s1 semaphore */
    vmm_msleep(SLEEP_MSECS * 10);

    /* Check worker0 shared data */
    if (shared_data[0] != 1) {
        vmm_cdev_printf(cdev, "error: worker0 shared data not updated\n");
        failures++;
    }

    /* s1 semaphore should not be available */
    if (vmm_semaphore_avail(&s1) != 0) {
        vmm_cdev_printf(cdev, "error: semaphore available\n");
        failures++;
    }

    /* Try semaphore down with timeout few times */
    for (i = 1; i <= 10; i++) {
        /* Save current timestamp */
        tstamp   = vmm_timer_timestamp();

        /* Down s1 semaphore with some timeout */
        etimeout = i * SLEEP_MSECS * 1000000ULL;
        timeout  = etimeout;
        rc       = vmm_semaphore_down_timeout(&s1, &timeout);

        if (rc != VMM_ERR_TIMEDOUT) {
            vmm_cdev_printf(cdev, "error: semaphore down did not timeout\n");
            failures++;
        }

        /* Check elapsed time */
        tstamp = vmm_timer_timestamp() - tstamp;

        if (tstamp < etimeout) {
            vmm_cdev_printf(cdev, "error: time elapsed %" PRIu64 " nanosecs instead of %" PRIu64 " nanosecs", tstamp, etimeout);
            failures++;
        }
    }

    /* Release s1 acquired by worker0 */
    for (i = 0; i < 3; i++) {
        rc = vmm_semaphore_up(&s1);

        if (rc) {
            vmm_cdev_printf(cdev, "error: semaphore not released\n");
            failures++;
        }
    }

    /* Release s1 which is already realeased which will fail */
    for (i = 0; i < 3; i++) {
        rc = vmm_semaphore_up(&s1);

        if (rc == VMM_OK) {
            vmm_cdev_printf(cdev, "error: semaphore released\n");
            failures++;
        }
    }

    /* s1 semaphore should be available */
    if (vmm_semaphore_avail(&s1) != 3) {
        vmm_cdev_printf(cdev, "error: semaphore not available\n");
        failures++;
    }

    return (failures) ? VMM_ERR_FAIL : 0;
}

static int semaphore5_run(struct white_box_test *test, vmm_char_device_t *cdev, uint32_t test_hcpu)
{
    int                  i, ret = VMM_OK;
    char                 wname[VMM_FIELD_NAME_SIZE];
    uint8_t              current_priority = vmm_scheduler_current_priority();
    const vmm_cpumask_t *cpu_mask         = vmm_cpumask_of(test_hcpu);

    /* Initialise global data */
    memset(workers, 0, sizeof(workers));

    /* Create worker threads */
    for (i = 0; i < NUM_THREADS; i++) {
        vmm_snprintf(wname, VMM_FIELD_NAME_SIZE, "semaphore5_worker%d", i);
        workers[i] = vmm_threads_create(wname, semaphore5_worker_thread_main, (void *)(uint64_t)i, current_priority, VMM_THREAD_DEF_TIME_SLICE);

        if (workers[i] == NULL) {
            ret = VMM_ERR_FAIL;
            goto destroy_workers;
        }

        vmm_threads_set_affinity(workers[i], cpu_mask);
    }

    /* Do the test */
    ret = semaphore5_do_test(cdev);

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

static struct white_box_test semaphore5 = {
    .name = "semaphore5",
    .run  = semaphore5_run,
};

static int __init semaphore5_init(void)
{
    return wboxtest_register("threads", &semaphore5);
}

static void __exit semaphore5_exit(void)
{
    wboxtest_unregister(&semaphore5);
}

VMM_DECLARE_MODULE(MODULE_DESC, MODULE_AUTHOR, MODULE_LICENSE, MODULE_IPRIORITY, MODULE_INIT, MODULE_EXIT);
