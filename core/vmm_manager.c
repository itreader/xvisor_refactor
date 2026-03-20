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
 * @file vmm_manager.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for hypervisor manager
 */

#include <arch_guest.h>
#include <arch_vcpu.h>
#include <libs/stringlib.h>
#include <vmm_compiler.h>
#include <vmm_error.h>
#include <vmm_guest_address_space.h>
#include <vmm_heap.h>
#include <vmm_manager.h>
#include <vmm_mutex.h>
#include <vmm_scheduler.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_vcpu_irq.h>
#include <vmm_waitqueue.h>
#include <vmm_workqueue.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...) vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

/** Control structure for manager */
struct vmm_manager_ctrl {
    /* Guest & VCPU management */
    vmm_mutex_t       lock;
    uint32_t          vcpu_count;
    uint32_t          guest_count;
    vmm_cpumask_t    *vcpu_affinity_mask;
    vmm_vcpu_t       *vcpu_array;
    bool             *vcpu_avail_array;
    struct vmm_guest *guest_array;
    bool             *guest_avail_array;
    double_list_t     orphan_vcpu_list;
    double_list_t     guest_list;
    /* Work structs to process guest request */
    vmm_work_t       *guest_work_array;
};

static struct vmm_manager_ctrl mngr;

void vmm_manager_lock(void)
{
    vmm_mutex_lock(&mngr.lock);
}

void vmm_manager_unlock(void)
{
    vmm_mutex_unlock(&mngr.lock);
}

uint32_t vmm_manager_max_vcpu_count(void)
{
    return CONFIG_MAX_VCPU_COUNT;
}

uint32_t vmm_manager_vcpu_count(void)
{
    uint32_t ret;

    vmm_manager_lock();
    ret = mngr.vcpu_count;
    vmm_manager_unlock();

    return ret;
}

vmm_vcpu_t *vmm_manager_vcpu(uint32_t vcpu_id)
{
    vmm_vcpu_t *ret = NULL;

    if (vcpu_id < CONFIG_MAX_VCPU_COUNT) {
        vmm_manager_lock();

        if (!mngr.vcpu_avail_array[vcpu_id]) {
            ret = &mngr.vcpu_array[vcpu_id];
        }

        vmm_manager_unlock();
    }

    return ret;
}

static void manager_vcpu_ipi_reset(void *vcpu_ptr, void *dummy1, void *dummy2)
{
    vmm_scheduler_state_change(vcpu_ptr, VMM_VCPU_STATE_RESET);
}

int vmm_manager_vcpu_iterate(int (*iter)(vmm_vcpu_t *, void *), void *private)
{
    int         rc, v;
    vmm_vcpu_t *vcpu;

    /* If no iteration callback then return */
    if (!iter) {
        return VMM_EINVALID;
    }

    /* Acquire manager lock */
    vmm_manager_lock();

    /* Iterate over each used VCPU instance */
    rc = VMM_OK;

    for (v = 0; v < CONFIG_MAX_VCPU_COUNT; v++) {
        if (mngr.vcpu_avail_array[v]) {
            continue;
        }

        vcpu = &mngr.vcpu_array[v];

        rc   = iter(vcpu, private);

        if (rc) {
            break;
        }
    }

    /* Release manager lock */
    vmm_manager_unlock();

    return rc;
}

/* Note: Must be called with manager lock held */
static uint32_t __vmm_manager_good_hcpu(uint8_t priority, const vmm_cpumask_t *affinity)
{
    vmm_vcpu_t *vcpu;
    uint32_t    count[CONFIG_CPU_COUNT];
    uint32_t    v, c, min, host_cpu = vmm_cpumask_first(affinity);

    if (!vmm_timer_started() || (vmm_cpumask_weight(affinity) < 1)) {
        return vmm_smp_processor_id();
    }

    for (c = 0; c < CONFIG_CPU_COUNT; c++) {
        count[c] = 0;
    }

    for (v = 0; v < CONFIG_MAX_VCPU_COUNT; v++) {
        if (mngr.vcpu_avail_array[v]) {
            continue;
        }

        vcpu = &mngr.vcpu_array[v];

        if ((vcpu->priority != priority) || !vmm_cpumask_test_cpu(vcpu->host_cpu, affinity)) {
            continue;
        }

        count[vcpu->host_cpu]++;
    }

    min = count[host_cpu];

    for (c = 0; c < CONFIG_CPU_COUNT; c++) {
        if (!vmm_cpumask_test_cpu(c, affinity)) {
            continue;
        }

        if (count[c] < min) {
            min      = count[c];
            host_cpu = c;
        }
    }

    return host_cpu;
}

uint32_t vmm_manager_vcpu_get_state(vmm_vcpu_t *vcpu)
{
    if (!vcpu) {
        return VMM_VCPU_STATE_UNKNOWN;
    }

    return (uint32_t)arch_atomic_read(&vcpu->state);
}

int vmm_manager_vcpu_set_state(vmm_vcpu_t *vcpu, uint32_t new_state)
{
    uint32_t    vhcpu;
    irq_flags_t flags;

    if (!vcpu) {
        return VMM_EFAIL;
    }

    /* If new_state == VMM_VCPU_STATE_RESET then
     * we use sync IPI for proper working of VCPU reset.
     *
     * For all other states we can directly call
     * scheduler state change
     */

    if (new_state == VMM_VCPU_STATE_RESET) {
        vmm_read_lock_irq_save_lite(&vcpu->sched_lock, flags);
        vhcpu = vcpu->host_cpu;
        vmm_read_unlock_irq_restore_lite(&vcpu->sched_lock, flags);
        return vmm_smp_ipi_sync_call(vmm_cpumask_of(vhcpu), 1000, manager_vcpu_ipi_reset, vcpu, NULL, NULL);
    }

    return vmm_scheduler_state_change(vcpu, new_state);
}

int vmm_manager_vcpu_get_hcpu(vmm_vcpu_t *vcpu, uint32_t *host_cpu)
{
    return vmm_scheduler_get_hcpu(vcpu, host_cpu);
}

bool vmm_manager_vcpu_check_current_hcpu(vmm_vcpu_t *vcpu)
{
    return vmm_scheduler_check_current_hcpu(vcpu);
}

int vmm_manager_vcpu_set_hcpu(vmm_vcpu_t *vcpu, uint32_t host_cpu)
{
    return vmm_scheduler_set_hcpu(vcpu, host_cpu);
}

int vmm_manager_vcpu_hcpu_resched(vmm_vcpu_t *vcpu)
{
    int         rc;
    irq_flags_t flags;

    if (!vcpu) {
        return VMM_EINVALID;
    }

    vmm_read_lock_irq_save_lite(&vcpu->sched_lock, flags);
    rc = vmm_scheduler_force_resched(vcpu->host_cpu);
    vmm_read_unlock_irq_restore_lite(&vcpu->sched_lock, flags);

    return rc;
}

static void manager_vcpu_hcpu_func(void *fptr, void *vptr, void *data)
{
    void (*func)(vmm_vcpu_t *, void *) = fptr;
    vmm_vcpu_t *vcpu                   = vptr;

    if (func && vcpu) {
        func(vcpu, data);
    }
}

int vmm_manager_vcpu_hcpu_func(vmm_vcpu_t *vcpu, uint32_t state_mask, void (*func)(vmm_vcpu_t *, void *), void *data, bool use_async)
{
    irq_flags_t          flags;
    const vmm_cpumask_t *cpu_mask = NULL;

    if (!vcpu || !func) {
        return VMM_EINVALID;
    }

    vmm_read_lock_irq_save_lite(&vcpu->sched_lock, flags);

    if (arch_atomic_read(&vcpu->state) & state_mask) {
        cpu_mask = vmm_cpumask_of(vcpu->host_cpu);
    }

    vmm_read_unlock_irq_restore_lite(&vcpu->sched_lock, flags);

    if (cpu_mask) {
        if (use_async) {
            vmm_smp_ipi_async_call(cpu_mask, manager_vcpu_hcpu_func, func, vcpu, data);
        } else {
            vmm_smp_ipi_sync_call(cpu_mask, 0, manager_vcpu_hcpu_func, func, vcpu, data);
        }
    }

    return VMM_OK;
}

const vmm_cpumask_t *vmm_manager_vcpu_get_affinity(vmm_vcpu_t *vcpu)
{
    irq_flags_t          flags;
    const vmm_cpumask_t *cpu_mask = NULL;

    if (!vcpu) {
        return NULL;
    }

    vmm_read_lock_irq_save_lite(&vcpu->sched_lock, flags);
    cpu_mask = vcpu->cpu_affinity;
    vmm_read_unlock_irq_restore_lite(&vcpu->sched_lock, flags);

    return cpu_mask;
}

int vmm_manager_vcpu_set_affinity(vmm_vcpu_t *vcpu, const vmm_cpumask_t *cpu_mask)
{
    int           rc;
    bool          locked;
    uint32_t      new_hcpu;
    irq_flags_t   flags;
    vmm_cpumask_t and_mask;

    if (!vcpu || !cpu_mask) {
        return VMM_EFAIL;
    }

    /* Lock load balancing */
    vmm_write_lock_irq_save_lite(&vcpu->sched_lock, flags);

    /* New affinity must overlap current affinity */
    vmm_cpumask_and(&and_mask, vcpu->cpu_affinity, cpu_mask);

    if (!vmm_cpumask_weight(&and_mask)) {
        vmm_write_unlock_irq_restore_lite(&vcpu->sched_lock, flags);
        return VMM_EINVALID;
    }

    /* Make sure current host_cpu is set in both current and new affinity */
    if (!vmm_cpumask_test_cpu(vcpu->host_cpu, &and_mask)) {
        vmm_write_unlock_irq_restore_lite(&vcpu->sched_lock, flags);

        /* Acquire manager lock */
        /* NOTE: We only touch manager lock if timer subsystem
         * has started on current host CPU. This check helps
         * create boot-time orphan VCPUs.
         */
        if (vmm_timer_started()) {
            locked = TRUE;
            vmm_manager_lock();
        } else {
            locked = FALSE;
        }

        /* Find good host CPU */
        new_hcpu = __vmm_manager_good_hcpu(vcpu->priority, &and_mask);

        /* Change host CPU */
        rc       = vmm_manager_vcpu_set_hcpu(vcpu, new_hcpu);

        /* Release manager lock */
        if (locked) {
            vmm_manager_unlock();
        }

        /* If set_hcpu failed then return failure */
        if (rc) {
            return rc;
        }

        vmm_write_lock_irq_save_lite(&vcpu->sched_lock, flags);
    }

    /* Update affinity */
    memcpy(&mngr.vcpu_affinity_mask[vcpu->id], cpu_mask, sizeof(*cpu_mask));
    vcpu->cpu_affinity = &mngr.vcpu_affinity_mask[vcpu->id];

    /* Unlock load balancing */
    vmm_write_unlock_irq_restore_lite(&vcpu->sched_lock, flags);

    return VMM_OK;
}

int vmm_manager_vcpu_resource_add(vmm_vcpu_t *vcpu, vmm_vcpu_resource_t *res)
{
    irq_flags_t flags;

    if (!vcpu || !res || !res->name || !res->cleanup) {
        return VMM_EINVALID;
    }

    INIT_LIST_HEAD(&res->head);
    vmm_spin_lock_irq_save_lite(&vcpu->res_lock, flags);
    list_add_tail(&res->head, &vcpu->res_head);
    vmm_spin_unlock_irq_restore_lite(&vcpu->res_lock, flags);

    return VMM_OK;
}

int vmm_manager_vcpu_resource_remove(vmm_vcpu_t *vcpu, vmm_vcpu_resource_t *res)
{
    irq_flags_t flags;

    if (!vcpu || !res) {
        return VMM_EINVALID;
    }

    vmm_spin_lock_irq_save_lite(&vcpu->res_lock, flags);
    list_del(&res->head);
    vmm_spin_unlock_irq_restore_lite(&vcpu->res_lock, flags);

    return VMM_OK;
}

static void vmm_manager_vcpu_resource_flush(vmm_vcpu_t *vcpu)
{
    irq_flags_t          flags;
    vmm_vcpu_resource_t *res;

    if (!vcpu) {
        return;
    }

    vmm_spin_lock_irq_save_lite(&vcpu->res_lock, flags);

    while (!list_empty(&vcpu->res_head)) {
        res = list_entry(list_pop_tail(&vcpu->res_head), vmm_vcpu_resource_t, head);

        vmm_spin_unlock_irq_restore_lite(&vcpu->res_lock, flags);

        if (res->cleanup) {
            res->cleanup(vcpu, res);
        }

        vmm_spin_lock_irq_save_lite(&vcpu->res_lock, flags);
    }

    vmm_spin_unlock_irq_restore_lite(&vcpu->res_lock, flags);
}

vmm_vcpu_t *vmm_manager_vcpu_orphan_create(
    const char *name, virtual_addr_t start_pc, virtual_size_t stack_size, uint8_t priority, uint64_t time_slice_nsecs, uint64_t deadline,
    uint64_t periodicity, const vmm_cpumask_t *affinity)
{
    bool                 locked;
    uint32_t             vnum, host_cpu;
    vmm_vcpu_t          *vcpu = NULL;
    const vmm_cpumask_t *aff  = (affinity) ? affinity : cpu_online_mask;

    /* Sanity checks */
    if (name == NULL || start_pc == 0 || time_slice_nsecs == 0) {
        return NULL;
    }

    if (VMM_VCPU_MAX_PRIORITY < priority) {
        return NULL;
    }

    if (priority < VMM_VCPU_MIN_PRIORITY) {
        return NULL;
    }

    /* Acquire manager lock */
    /* NOTE: We only touch manager lock if timer subsystem
     * has started on current host CPU. This check helps
     * create boot-time orphan VCPUs.
     */
    if (vmm_timer_started()) {
        locked = TRUE;
        vmm_manager_lock();
    } else {
        locked = FALSE;
    }

    /* Find good host CPU */
    host_cpu = __vmm_manager_good_hcpu(priority, aff);

    /* Find the next available vcpu */
    for (vnum = 0; vnum < CONFIG_MAX_VCPU_COUNT; vnum++) {
        if (!mngr.vcpu_avail_array[vnum]) {
            continue;
        }

        vcpu           = &mngr.vcpu_array[vnum];

        /* Update priority */
        vcpu->priority = priority;

        /* Update host CPU and affinity */
        vcpu->host_cpu = host_cpu;
        memcpy(&mngr.vcpu_affinity_mask[vcpu->id], aff, sizeof(*aff));
        vcpu->cpu_affinity              = &mngr.vcpu_affinity_mask[vcpu->id];

        mngr.vcpu_avail_array[vcpu->id] = FALSE;
        break;
    }

    if (!vcpu) {
        goto fail;
    }

    INIT_LIST_HEAD(&vcpu->head);

    /* Update general info and state */
    vcpu->subid = 0;

    if (strlcpy(vcpu->name, name, sizeof(vcpu->name)) >= sizeof(vcpu->name)) {
        goto fail_avail;
    }

    vcpu->node        = NULL;
    vcpu->is_normal   = FALSE;
    vcpu->is_poweroff = FALSE;
    vcpu->guest       = NULL;
    arch_atomic_write(&vcpu->state, VMM_VCPU_STATE_UNKNOWN);

    /* Add VCPU to orphan list */
    list_add_tail(&vcpu->head, &mngr.orphan_vcpu_list);

    /* Increment vcpu count */
    mngr.vcpu_count++;

    /* Release manager lock */
    if (locked) {
        vmm_manager_unlock();
    }

    /* Setup start program counter and stack */
    vcpu->start_pc              = start_pc;
    vcpu->stack_virtual_address = (virtual_addr_t)vmm_malloc(stack_size);

    if (!vcpu->stack_virtual_address) {
        goto fail_list_del;
    }

    vcpu->stack_size = stack_size;

    /* Intialize dynamic scheduling context */
    INIT_RW_LOCK(&vcpu->sched_lock);
    vcpu->state_tstamp        = vmm_timer_timestamp();
    vcpu->state_ready_nsecs   = 0;
    vcpu->state_running_nsecs = 0;
    vcpu->state_paused_nsecs  = 0;
    vcpu->state_halted_nsecs  = 0;
    vcpu->system_nsecs        = 0;
    vcpu->reset_count         = 0;
    vcpu->reset_timestamp     = 0;
    vcpu->preempt_count       = 0;
    vcpu->resumed             = FALSE;
    vcpu->sched_private       = NULL;

    /* Intialize static scheduling context */
    vcpu->time_slice          = time_slice_nsecs;
    vcpu->deadline            = deadline;

    if (vcpu->deadline < vcpu->time_slice) {
        vcpu->deadline = vcpu->time_slice;
    }

    vcpu->periodicity = periodicity;

    if (vcpu->periodicity < vcpu->deadline) {
        vcpu->periodicity = vcpu->deadline;
    }

    /* Initialize architecture specific context */
    vcpu->arch_private = NULL;

    if (arch_vcpu_init(vcpu)) {
        goto fail_free_stack;
    }

    /* Initialize resource list */
    INIT_SPIN_LOCK(&vcpu->res_lock);
    INIT_LIST_HEAD(&vcpu->res_head);

    /* Initialize waitqueue context and cleanup callback */
    INIT_LIST_HEAD(&vcpu->wq_head);
    vcpu->wq_lock    = NULL;
    vcpu->wq_private = NULL;
    vcpu->wq_cleanup = NULL;

    /* Notify scheduler about new VCPU */
    if (vmm_manager_vcpu_set_state(vcpu, VMM_VCPU_STATE_RESET)) {
        goto fail_vcpu_deinit;
    }

    return vcpu;

fail_vcpu_deinit:
    arch_vcpu_deinit(vcpu);
fail_free_stack:
    vmm_free((void *)vcpu->stack_virtual_address);
fail_list_del:

    if (vmm_timer_started()) {
        vmm_manager_lock();
        locked = TRUE;
    } else {
        locked = FALSE;
    }

    mngr.vcpu_count--;
    list_del(&vcpu->head);
fail_avail:
    mngr.vcpu_avail_array[vcpu->id] = TRUE;
fail:

    if (locked) {
        vmm_manager_unlock();
    }

    return NULL;
}

int vmm_manager_vcpu_orphan_destroy(vmm_vcpu_t *vcpu)
{
    int rc = VMM_EFAIL;

    /* Sanity checks */
    if (!vcpu) {
        return rc;
    }

    if (vcpu->is_normal) {
        return rc;
    }

    /* Force VCPU out of waitqueue */
    vmm_waitqueue_forced_remove(vcpu);

    /* Reset the VCPU */
    if ((rc = vmm_manager_vcpu_set_state(vcpu, VMM_VCPU_STATE_RESET))) {
        return rc;
    }

    /* Flush all resources acquired by this VCPU */
    vmm_manager_vcpu_resource_flush(vcpu);

    /* Set VCPU to unknown state (This will clean scheduling context) */
    if ((rc = vmm_manager_vcpu_set_state(vcpu, VMM_VCPU_STATE_UNKNOWN))) {
        return rc;
    }

    vcpu->sched_private = NULL;

    /* Deinit architecture specific context */
    if ((rc = arch_vcpu_deinit(vcpu))) {
        return rc;
    }

    /* Free stack pages */
    if (vcpu->stack_virtual_address) {
        vmm_free((void *)vcpu->stack_virtual_address);
    }

    /* Acquire manager lock */
    vmm_manager_lock();

    /* Decrement vcpu count */
    mngr.vcpu_count--;

    /* Remove VCPU from orphan list */
    list_del(&vcpu->head);

    /* Mark this VCPU as available */
    mngr.vcpu_avail_array[vcpu->id] = TRUE;

    /* Release manager lock */
    vmm_manager_unlock();

    return VMM_OK;
}

uint32_t vmm_manager_max_guest_count(void)
{
    return CONFIG_MAX_GUEST_COUNT;
}

uint32_t vmm_manager_guest_count(void)
{
    uint32_t ret;

    vmm_manager_lock();
    ret = mngr.guest_count;
    vmm_manager_unlock();

    return ret;
}

struct vmm_guest *vmm_manager_guest(uint32_t guest_id)
{
    struct vmm_guest *ret = NULL;

    if (guest_id < CONFIG_MAX_GUEST_COUNT) {
        vmm_manager_lock();

        if (!mngr.guest_avail_array[guest_id]) {
            ret = &mngr.guest_array[guest_id];
        }

        vmm_manager_unlock();
    }

    return ret;
}

struct vmm_guest *vmm_manager_guest_find(const char *guest_name)
{
    uint32_t          g;
    struct vmm_guest *ret;

    if (!guest_name) {
        return NULL;
    }

    /* Acquire manager lock */
    vmm_manager_lock();

    /* Iterate over each used VCPU instance */
    ret = NULL;

    for (g = 0; g < CONFIG_MAX_GUEST_COUNT; g++) {
        if (!mngr.guest_avail_array[g]) {
            if (!strcmp(mngr.guest_array[g].name, guest_name)) {
                ret = &mngr.guest_array[g];
                break;
            }
        }
    }

    /* Release manager lock */
    vmm_manager_unlock();

    return ret;
}

int vmm_manager_guest_iterate(int (*iter)(struct vmm_guest *, void *), void *private)
{
    int               rc, g;
    struct vmm_guest *guest;

    /* If no iteration callback then return */
    if (!iter) {
        return VMM_EINVALID;
    }

    /* Acquire manager lock */
    vmm_manager_lock();

    /* Iterate over each used VCPU instance */
    rc = VMM_OK;

    for (g = 0; g < CONFIG_MAX_GUEST_COUNT; g++) {
        if (mngr.guest_avail_array[g]) {
            continue;
        }

        guest = &mngr.guest_array[g];

        rc    = iter(guest, private);

        if (rc) {
            break;
        }
    }

    /* Release manager lock */
    vmm_manager_unlock();

    return rc;
}

uint32_t vmm_manager_guest_vcpu_count(struct vmm_guest *guest)
{
    if (!guest) {
        return 0;
    }

    return guest->vcpu_count;
}

vmm_vcpu_t *vmm_manager_guest_vcpu(struct vmm_guest *guest, uint32_t subid)
{
    bool        found = FALSE;
    irq_flags_t flags;
    vmm_vcpu_t *vcpu = NULL;

    if (!guest) {
        return NULL;
    }

    vmm_read_lock_irq_save_lite(&guest->vcpu_lock, flags);

    list_for_each_entry(vcpu, &guest->vcpu_list, head)
    {
        if (vcpu->subid == subid) {
            found = TRUE;
            break;
        }
    }

    vmm_read_unlock_irq_restore_lite(&guest->vcpu_lock, flags);

    if (!found) {
        vcpu = NULL;
    }

    return vcpu;
}

vmm_vcpu_t *vmm_manager_guest_next_vcpu(const struct vmm_guest *guest, vmm_vcpu_t *current)
{
    irq_flags_t       flags;
    vmm_vcpu_t       *ret = NULL;
    struct vmm_guest *g   = (struct vmm_guest *)guest;

    if (!g) {
        return NULL;
    }

    vmm_read_lock_irq_save_lite(&g->vcpu_lock, flags);

    if (!current) {
        if (!list_empty(&g->vcpu_list)) {
            ret = list_first_entry(&g->vcpu_list, vmm_vcpu_t, head);
        }
    } else if (!list_is_last(&current->head, &g->vcpu_list)) {
        ret = list_first_entry(&current->head, vmm_vcpu_t, head);
    }

    vmm_read_unlock_irq_restore_lite(&g->vcpu_lock, flags);

    return ret;
}

int vmm_manager_guest_vcpu_iterate(struct vmm_guest *guest, int (*iter)(vmm_vcpu_t *, void *), void *private)
{
    int         rc = VMM_OK;
    irq_flags_t flags;
    vmm_vcpu_t *vcpu;

    if (!guest || !iter) {
        return VMM_EFAIL;
    }

    vmm_read_lock_irq_save_lite(&guest->vcpu_lock, flags);

    list_for_each_entry(vcpu, &guest->vcpu_list, head)
    {
        rc = iter(vcpu, private);

        if (rc) {
            break;
        }
    }

    vmm_read_unlock_irq_restore_lite(&guest->vcpu_lock, flags);

    return rc;
}

static int manager_guest_reset_iter(vmm_vcpu_t *vcpu, void *private)
{
    return vmm_manager_vcpu_reset(vcpu);
}

int vmm_manager_guest_reset(struct vmm_guest *guest)
{
    int rc;

    if (!guest) {
        return VMM_EFAIL;
    }

    guest->reset_count++;
    guest->reset_timestamp = vmm_timer_timestamp();

    rc                     = vmm_manager_guest_vcpu_iterate(guest, manager_guest_reset_iter, NULL);

    if (rc) {
        return rc;
    }

    if (!(rc = arch_guest_init(guest))) {
        rc = vmm_guest_address_space_reset(guest);
    }

    return rc;
}

uint64_t vmm_manager_guest_reset_timestamp(struct vmm_guest *guest)
{
    return (guest) ? guest->reset_timestamp : 0;
}

static int manager_guest_kick_iter(vmm_vcpu_t *vcpu, void *private)
{
    /* Do not kick VCPU with poweroff flag set
     * when Guest is kicked.
     */
    if (vcpu->is_poweroff) {
        return VMM_OK;
    }

    return vmm_manager_vcpu_kick(vcpu);
}

int vmm_manager_guest_kick(struct vmm_guest *guest)
{
    return vmm_manager_guest_vcpu_iterate(guest, manager_guest_kick_iter, NULL);
}

static int manager_guest_pause_iter(vmm_vcpu_t *vcpu, void *private)
{
    return vmm_manager_vcpu_pause(vcpu);
}

int vmm_manager_guest_pause(struct vmm_guest *guest)
{
    return vmm_manager_guest_vcpu_iterate(guest, manager_guest_pause_iter, NULL);
}

static int manager_guest_resume_iter(vmm_vcpu_t *vcpu, void *private)
{
    return vmm_manager_vcpu_resume(vcpu);
}

int vmm_manager_guest_resume(struct vmm_guest *guest)
{
    return vmm_manager_guest_vcpu_iterate(guest, manager_guest_resume_iter, NULL);
}

static int manager_guest_halt_iter(vmm_vcpu_t *vcpu, void *private)
{
    return vmm_manager_vcpu_halt(vcpu);
}

int vmm_manager_guest_halt(struct vmm_guest *guest)
{
    return vmm_manager_guest_vcpu_iterate(guest, manager_guest_halt_iter, NULL);
}

static bool manager_have_req(struct vmm_guest *guest)
{
    bool        ret = FALSE;
    irq_flags_t flags;

    vmm_spin_lock_irq_save_lite(&guest->request_lock, flags);

    if (!list_empty(&guest->operation_request_list)) {
        ret = TRUE;
    }

    vmm_spin_unlock_irq_restore_lite(&guest->request_lock, flags);

    return ret;
}

static void manager_enqueue_request(struct vmm_guest *guest, struct vmm_guest_request *req)
{
    irq_flags_t flags;

    vmm_spin_lock_irq_save_lite(&guest->request_lock, flags);
    list_add_tail(&req->head, &guest->operation_request_list);
    vmm_spin_unlock_irq_restore_lite(&guest->request_lock, flags);

    vmm_workqueue_schedule_work(NULL, &mngr.guest_work_array[guest->id]);
}

static struct vmm_guest_request *manager_dequeue_req(struct vmm_guest *guest)
{
    irq_flags_t               flags;
    struct vmm_guest_request *req = NULL;

    vmm_spin_lock_irq_save_lite(&guest->request_lock, flags);

    if (!list_empty(&guest->operation_request_list)) {
        req = list_entry(list_pop(&guest->operation_request_list), struct vmm_guest_request, head);
    }

    vmm_spin_unlock_irq_restore_lite(&guest->request_lock, flags);

    return req;
}

static void manager_flush_req(struct vmm_guest *guest)
{
    irq_flags_t               flags;
    struct vmm_guest_request *req;

    vmm_spin_lock_irq_save_lite(&guest->request_lock, flags);

    while (!list_empty(&guest->operation_request_list)) {
        req = list_entry(list_pop(&guest->operation_request_list), struct vmm_guest_request, head);
        vmm_free(req);
    }

    vmm_spin_unlock_irq_restore_lite(&guest->request_lock, flags);
}

static void manager_req_work(vmm_work_t *work)
{
    uint32_t                  id;
    void                     *start, *end, *ptr;
    struct vmm_guest         *guest;
    struct vmm_guest_request *req;

    /* Determine guest pointer from work pointer */
    ptr   = work;
    start = &mngr.guest_work_array[0];
    end   = &mngr.guest_work_array[CONFIG_MAX_GUEST_COUNT - 1];

    if (ptr < start || end <= ptr) {
        return;
    }

    id    = ptr - start;
    id    = id / sizeof(*work);
    guest = vmm_manager_guest(id);

    if (!guest) {
        return;
    }

    /* Process one request if available */
    if ((req = manager_dequeue_req(guest))) {
        req->func(guest, req->data);
        vmm_free(req);

        /* Reschedule work if we more request */
        if (manager_have_req(guest)) {
            vmm_workqueue_schedule_work(NULL, &mngr.guest_work_array[guest->id]);
        }
    }
}

int vmm_manager_guest_operation_request(struct vmm_guest *guest, void (*req_func)(struct vmm_guest *, void *), void *req_data)
{
    struct vmm_guest_request *req;

    if (!guest || !req_func) {
        return VMM_EINVALID;
    }

    req = vmm_zalloc(sizeof(*req));

    if (!req) {
        return VMM_ENOMEM;
    }

    INIT_LIST_HEAD(&req->head);
    req->func = req_func;
    req->data = req_data;

    manager_enqueue_request(guest, req);

    return VMM_OK;
}

static void manager_reboot_request(struct vmm_guest *guest, void *data)
{
    vmm_manager_guest_reset(guest);
    vmm_manager_guest_kick(guest);
}

int vmm_manager_guest_reboot_request(struct vmm_guest *guest)
{
    vmm_vcpu_t *cvcpu;

    if (!guest) {
        return VMM_EINVALID;
    }

    /* If current VCPU belongs to the Guest then
     * pause the VCPU so that we don't return back
     * to the VCPU after submitting request.
     */
    cvcpu = vmm_scheduler_current_vcpu();

    if (cvcpu && (cvcpu->guest == guest) && vmm_scheduler_normal_context()) {
        vmm_manager_vcpu_pause(cvcpu);
    }

    return vmm_manager_guest_operation_request(guest, manager_reboot_request, NULL);
}

static void manager_shutdown_request(struct vmm_guest *guest, void *data)
{
    vmm_manager_guest_reset(guest);
}

int vmm_manager_guest_shutdown_request(struct vmm_guest *guest)
{
    vmm_vcpu_t *cvcpu;

    if (!guest) {
        return VMM_EINVALID;
    }

    /* If current VCPU belongs to the Guest then
     * pause the VCPU so that we don't return back
     * to the VCPU after submitting request.
     */
    cvcpu = vmm_scheduler_current_vcpu();

    if (cvcpu && (cvcpu->guest == guest) && vmm_scheduler_normal_context()) {
        vmm_manager_vcpu_pause(cvcpu);
    }

    return vmm_manager_guest_operation_request(guest, manager_shutdown_request, NULL);
}

struct vmm_guest *vmm_manager_guest_create(vmm_device_tree_node_t *gnode)
{
    uint32_t                val, vnum, gnum;
    const char             *str;
    irq_flags_t             flags;
    vmm_device_tree_node_t *vsnode;
    vmm_device_tree_node_t *vnode;
    struct vmm_guest       *guest = NULL;
    vmm_vcpu_t             *vcpu  = NULL;

    /* Sanity checks */
    if (!gnode) {
        return NULL;
    }

    if (vmm_device_tree_read_string(gnode, VMM_DEVICE_TREE_DEVICE_TYPE_ATTR_NAME, &str)) {
        return NULL;
    }

    if (strcmp(str, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_GUEST) != 0) {
        return NULL;
    }

    /* Acquire manager lock */
    vmm_manager_lock();

    /* Ensure guest node uniqueness */
    list_for_each_entry(guest, &mngr.guest_list, head)
    {
        if ((guest->node == gnode) || (strcmp(guest->name, gnode->name) == 0)) {
            vmm_manager_unlock();
            vmm_printf("%s: Duplicate Guest %s detected\n", __func__, gnode->name);
            return NULL;
        }
    }

    /* Find next available guest instance */
    for (gnum = 0; gnum < CONFIG_MAX_GUEST_COUNT; gnum++) {
        if (mngr.guest_avail_array[gnum]) {
            guest                             = &mngr.guest_array[gnum];
            mngr.guest_avail_array[guest->id] = FALSE;
            break;
        }
    }

    if (!guest) {
        vmm_manager_unlock();
        vmm_printf("%s: No available Guest instance found\n", __func__);
        return NULL;
    }

    /* Add guest instance to guest list */
    list_add_tail(&guest->head, &mngr.guest_list);

    /* Increment guest count */
    mngr.guest_count++;

    /* Initialize guest instance */
    strlcpy(guest->name, gnode->name, sizeof(guest->name));
    vmm_device_tree_ref_node(gnode);
    guest->node = gnode;
#ifdef CONFIG_CPU_BE
    guest->is_big_endian = TRUE;
#else
    guest->is_big_endian = FALSE;
#endif
    guest->reset_count     = 0;
    guest->reset_timestamp = vmm_timer_timestamp();
    INIT_SPIN_LOCK(&guest->request_lock);
    INIT_LIST_HEAD(&guest->operation_request_list);
    INIT_RW_LOCK(&guest->vcpu_lock);
    guest->vcpu_count = 0;
    INIT_LIST_HEAD(&guest->vcpu_list);
    memset(&guest->aspace, 0, sizeof(guest->aspace));
    guest->aspace.initialized = FALSE;
    INIT_RW_LOCK(&guest->aspace.reg_iotree_lock);
    INIT_LIST_HEAD(&guest->aspace.reg_ioprobe_list);
    guest->aspace.reg_iotree = RB_ROOT;
    INIT_RW_LOCK(&guest->aspace.reg_memory_tree_lock);
    INIT_LIST_HEAD(&guest->aspace.reg_memprobe_list);
    guest->aspace.reg_memtree = RB_ROOT;
    guest->arch_private       = NULL;

    /* Determine guest endianness from guest node */
    if (vmm_device_tree_read_string(gnode, VMM_DEVICE_TREE_ENDIANNESS_ATTR_NAME, &str) == VMM_OK) {
        if (!strcmp(str, VMM_DEVICE_TREE_ENDIANNESS_VAL_LITTLE)) {
            guest->is_big_endian = FALSE;
        } else if (!strcmp(str, VMM_DEVICE_TREE_ENDIANNESS_VAL_BIG)) {
            guest->is_big_endian = TRUE;
        }
    }

    /* Release manager lock */
    vmm_manager_unlock();

    vsnode = vmm_device_tree_getchild(gnode, VMM_DEVICE_TREE_VCPUS_NODE_NAME);

    if (!vsnode) {
        vmm_printf("%s: vcpus node not found for Guest %s\n", __func__, gnode->name);
        goto fail_destroy_guest;
    }

    vmm_device_tree_for_each_child(vnode, vsnode)
    {
        int           index;
        uint32_t      cpu;
        vmm_cpumask_t mask;

        /* Sanity checks */
        if (CONFIG_MAX_VCPU_COUNT <= mngr.vcpu_count) {
            vmm_printf(
                "%s: No more free VCPUs\n"
                "for Guest %s VCPU %s\n",
                __func__, gnode->name, vnode->name);
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode;
        }

        if (vmm_device_tree_read_string(vnode, VMM_DEVICE_TREE_DEVICE_TYPE_ATTR_NAME, &str)) {
            vmm_printf(
                "%s: No device_type attribute\n"
                "for Guest %s VCPU %s\n",
                __func__, gnode->name, vnode->name);
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode;
        }

        if (strcmp(str, VMM_DEVICE_TREE_DEVICE_TYPE_VAL_VCPU) != 0) {
            vmm_printf(
                "%s: Invalid device_type attribute\n"
                "for Guest %s VCPU %s\n",
                __func__, gnode->name, vnode->name);
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode;
        }

        /* Setup VCPU affinity mask */
        if (vmm_device_tree_getattr(vnode, VMM_DEVICE_TREE_VCPU_AFFINITY_ATTR_NAME)) {
            /* Start with empty affinity mask */
            mask  = VMM_CPU_MASK_NONE;

            /* Set all assigned CPU in the mask */
            index = 0;

            while (vmm_device_tree_read_u32_atindex(vnode, VMM_DEVICE_TREE_VCPU_AFFINITY_ATTR_NAME, &cpu, index) == VMM_OK) {
                if ((cpu < CONFIG_CPU_COUNT) && vmm_cpu_online(cpu)) {
                    vmm_cpumask_set_cpu(cpu, &mask);
                } else {
                    vmm_printf(
                        "%s: CPU%d is out of bound"
                        " (%d <) or not online for"
                        " Guest %s VCPU %s\n",
                        __func__, cpu, CONFIG_CPU_COUNT, gnode->name, vnode->name);
                    vmm_device_tree_dref_node(vnode);
                    goto fail_dref_vsnode;
                }

                index++;
            }

            /* If affinity mask turns-out to be empty then fail */
            if (vmm_cpumask_weight(&mask) < 1) {
                vmm_printf(
                    "%s: Empty affinity mask\n"
                    "for Guest %s VCPU %s\n",
                    __func__, gnode->name, vnode->name);
                vmm_device_tree_dref_node(vnode);
                goto fail_dref_vsnode;
            }
        } else {
            memcpy(&mask, cpu_online_mask, sizeof(mask));
        }

        /* Acquire manager lock */
        vmm_manager_lock();

        /* Find next available VCPU instance */
        vcpu = NULL;

        for (vnum = 0; vnum < CONFIG_MAX_VCPU_COUNT; vnum++) {
            if (!mngr.vcpu_avail_array[vnum]) {
                continue;
            }

            vcpu = &mngr.vcpu_array[vnum];

            /* Update priority */
            if (vmm_device_tree_read_u32(vnode, VMM_DEVICE_TREE_PRIORITY_ATTR_NAME, &val)) {
                vcpu->priority = VMM_VCPU_DEF_PRIORITY;
            } else {
                vcpu->priority = val;
            }

            if (VMM_VCPU_MAX_PRIORITY < vcpu->priority) {
                vcpu->priority = VMM_VCPU_MAX_PRIORITY;
            }

            if (vcpu->priority < VMM_VCPU_MIN_PRIORITY) {
                vcpu->priority = VMM_VCPU_MIN_PRIORITY;
            }

            /* Update host CPU and affinity */
            memcpy(&mngr.vcpu_affinity_mask[vcpu->id], &mask, sizeof(mask));
            vcpu->host_cpu                  = __vmm_manager_good_hcpu(vcpu->priority, &mask);
            vcpu->cpu_affinity              = &mngr.vcpu_affinity_mask[vcpu->id];

            mngr.vcpu_avail_array[vcpu->id] = FALSE;
            break;
        }

        if (!vcpu) {
            vmm_printf(
                "%s: No available VCPU instance found \n"
                "for Guest %s VCPU %s\n",
                __func__, gnode->name, vnode->name);
            vmm_manager_unlock();
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode;
        }

        /* Update general info and state */
        vcpu->subid = guest->vcpu_count;
        strlcpy(vcpu->name, gnode->name, sizeof(vcpu->name));
        strlcat(vcpu->name, VMM_DEVICE_TREE_PATH_SEPARATOR_STRING, sizeof(vcpu->name));

        if (strlcat(vcpu->name, vnode->name, sizeof(vcpu->name)) >= sizeof(vcpu->name)) {
            vmm_printf(
                "%s: name concatination failed "
                "for Guest %s VCPU %s\n",
                __func__, gnode->name, vnode->name);
            mngr.vcpu_avail_array[vcpu->id] = TRUE;
            vmm_manager_unlock();
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode;
        }

        vmm_device_tree_ref_node(vnode);
        vcpu->node        = vnode;
        vcpu->is_normal   = TRUE;
        vcpu->is_poweroff = FALSE;
        vcpu->guest       = guest;
        arch_atomic_write(&vcpu->state, VMM_VCPU_STATE_UNKNOWN);

        /* Increment VCPU count */
        mngr.vcpu_count++;

        /* Release manager lock */
        vmm_manager_unlock();

        /* Setup start program counter and stack */
        vmm_device_tree_read_virtaddr(vnode, VMM_DEVICE_TREE_START_PC_ATTR_NAME, &vcpu->start_pc);
        vcpu->stack_virtual_address = (virtual_addr_t)vmm_malloc(CONFIG_IRQ_STACK_SIZE);

        if (!vcpu->stack_virtual_address) {
            vmm_printf(
                "%s: stack alloc failed "
                "for VCPU %s\n",
                __func__, vcpu->name);
            vmm_device_tree_dref_node(vcpu->node);
            vcpu->node = NULL;
            vmm_manager_lock();
            mngr.vcpu_count--;
            mngr.vcpu_avail_array[vcpu->id] = TRUE;
            vmm_manager_unlock();
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode;
        }

        vcpu->stack_size = CONFIG_IRQ_STACK_SIZE;

        /* Initialize dynamic scheduling context */
        INIT_RW_LOCK(&vcpu->sched_lock);
        vcpu->state_tstamp        = vmm_timer_timestamp();
        vcpu->state_ready_nsecs   = 0;
        vcpu->state_running_nsecs = 0;
        vcpu->state_paused_nsecs  = 0;
        vcpu->state_halted_nsecs  = 0;
        vcpu->system_nsecs        = 0;
        vcpu->reset_count         = 0;
        vcpu->reset_timestamp     = 0;
        vcpu->preempt_count       = 0;
        vcpu->resumed             = FALSE;
        vcpu->sched_private       = NULL;

        /* Initialize static scheduling context */
        if (vmm_device_tree_read_u64(vnode, VMM_DEVICE_TREE_TIME_SLICE_ATTR_NAME, &vcpu->time_slice)) {
            vcpu->time_slice = VMM_VCPU_DEF_TIME_SLICE;
        }

        if (vcpu->time_slice == 0) {
            vcpu->time_slice = VMM_VCPU_DEF_TIME_SLICE;
        }

        if (vmm_device_tree_read_u64(vnode, VMM_DEVICE_TREE_DEADLINE_ATTR_NAME, &vcpu->deadline)) {
            vcpu->deadline = VMM_VCPU_DEF_DEADLINE;
        }

        if (vcpu->deadline < vcpu->time_slice) {
            vcpu->deadline = vcpu->time_slice;
        }

        if (vmm_device_tree_read_u64(vnode, VMM_DEVICE_TREE_PERIODICITY_ATTR_NAME, &vcpu->periodicity)) {
            vcpu->periodicity = VMM_VCPU_DEF_PERIODICITY;
        }

        if (vcpu->periodicity < vcpu->deadline) {
            vcpu->periodicity = vcpu->deadline;
        }

        /* Initialize architecture specific context */
        vcpu->arch_private = NULL;

        if (arch_vcpu_init(vcpu)) {
            vmm_free((void *)vcpu->stack_virtual_address);
            vmm_printf(
                "%s: arch_vcpu_init() failed "
                "for VCPU %s\n",
                __func__, vcpu->name);
            vmm_device_tree_dref_node(vcpu->node);
            vcpu->node = NULL;
            vmm_manager_lock();
            mngr.vcpu_count--;
            mngr.vcpu_avail_array[vcpu->id] = TRUE;
            vmm_manager_unlock();
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode;
        }

        /* Initialize virtual IRQ context */
        if (vmm_vcpu_irq_init(vcpu)) {
            arch_vcpu_deinit(vcpu);
            vmm_free((void *)vcpu->stack_virtual_address);
            vmm_printf(
                "%s: vmm_vcpu_irq_init() failed "
                "for VCPU %s\n",
                __func__, vcpu->name);
            vmm_device_tree_dref_node(vcpu->node);
            vcpu->node = NULL;
            vmm_manager_lock();
            mngr.vcpu_count--;
            mngr.vcpu_avail_array[vcpu->id] = TRUE;
            vmm_manager_unlock();
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode;
        }

        /* Initialize resource list */
        INIT_SPIN_LOCK(&vcpu->res_lock);
        INIT_LIST_HEAD(&vcpu->res_head);

        /* Initialize waitqueue context and cleanup callback */
        INIT_LIST_HEAD(&vcpu->wq_head);
        vcpu->wq_lock    = NULL;
        vcpu->wq_private = NULL;
        vcpu->wq_cleanup = NULL;

        /* Notify scheduler about new VCPU */
        if (vmm_manager_vcpu_set_state(vcpu, VMM_VCPU_STATE_RESET)) {
            vmm_vcpu_irq_deinit(vcpu);
            arch_vcpu_deinit(vcpu);
            vmm_free((void *)vcpu->stack_virtual_address);
            vmm_printf(
                "%s: Setting RESET state failed "
                "for VCPU %s\n",
                __func__, vcpu->name);
            vmm_device_tree_dref_node(vcpu->node);
            vcpu->node = NULL;
            vmm_manager_lock();
            mngr.vcpu_count--;
            mngr.vcpu_avail_array[vcpu->id] = TRUE;
            vmm_manager_unlock();
            vmm_device_tree_dref_node(vnode);
            goto fail_dref_vsnode;
        }

        /* Get poweroff flag from device tree */
        if (vmm_device_tree_getattr(vnode, VMM_DEVICE_TREE_VCPU_POWEROFF_ATTR_NAME)) {
            vcpu->is_poweroff = TRUE;
        }

        /* Add VCPU to Guest child list */
        vmm_write_lock_irq_save_lite(&guest->vcpu_lock, flags);
        list_add_tail(&vcpu->head, &guest->vcpu_list);
        guest->vcpu_count++;
        vmm_write_unlock_irq_restore_lite(&guest->vcpu_lock, flags);
    }

    /* Release vcpus node */
    vmm_device_tree_dref_node(vsnode);

    /* Fail if no VCPU is associated to the guest */
    vmm_read_lock_irq_save_lite(&guest->vcpu_lock, flags);

    if (list_empty(&guest->vcpu_list)) {
        vmm_read_unlock_irq_restore_lite(&guest->vcpu_lock, flags);
        goto fail_destroy_guest;
    }

    vmm_read_unlock_irq_restore_lite(&guest->vcpu_lock, flags);

    /* Initialize arch guest context */
    if (arch_guest_init(guest)) {
        goto fail_destroy_guest;
    }

    /* Initialize guest address space */
    if (vmm_guest_address_space_init(guest)) {
        goto fail_destroy_guest;
    }

    /* Reset guest address space */
    if (vmm_guest_address_space_reset(guest)) {
        goto fail_destroy_guest;
    }

    return guest;

fail_dref_vsnode:
    vmm_device_tree_dref_node(vsnode);
fail_destroy_guest:
    vmm_manager_guest_destroy(guest);
    return NULL;
}

int vmm_manager_guest_destroy(struct vmm_guest *guest)
{
    int         rc;
    irq_flags_t flags;
    vmm_vcpu_t *vcpu;

    /* Sanity Check */
    if (!guest) {
        return VMM_EFAIL;
    }

    /* For sanity reset guest (ignore reture value) */
    vmm_manager_guest_reset(guest);

    /* Flush all request for this guest */
    manager_flush_req(guest);

    /* Deinit the guest aspace */
    if ((rc = vmm_guest_address_space_deinit(guest))) {
        return rc;
    }

    /* Deinit arch guest context */
    if ((rc = arch_guest_deinit(guest))) {
        return rc;
    }

    /* Acquire Guest VCPU lock */
    vmm_write_lock_irq_save_lite(&guest->vcpu_lock, flags);

    /* Destroy each VCPU of guest */
    while (!list_empty(&guest->vcpu_list)) {
        vcpu = list_first_entry(&guest->vcpu_list, vmm_vcpu_t, head);

        /* Remove from guest->vcpu_list */
        guest->vcpu_count--;
        list_del(&vcpu->head);

        /* Release Guest VCPU lock */
        vmm_write_unlock_irq_restore_lite(&guest->vcpu_lock, flags);

        /* Flush all resources acquired by this VCPU */
        vmm_manager_vcpu_resource_flush(vcpu);

        /* Set VCPU state to unknown
         * (This will clean scheduling context)
         */
        if ((rc = vmm_manager_vcpu_set_state(vcpu, VMM_VCPU_STATE_UNKNOWN))) {
            return rc;
        }

        vcpu->sched_private = NULL;

        /* Deinit Virtual IRQ context */
        if ((rc = vmm_vcpu_irq_deinit(vcpu))) {
            return rc;
        }

        /* Deinit architecture specific context */
        if ((rc = arch_vcpu_deinit(vcpu))) {
            return rc;
        }

        /* Free stack pages */
        if (vcpu->stack_virtual_address) {
            vmm_free((void *)vcpu->stack_virtual_address);
        }

        /* De-reference VCPU node */
        vmm_device_tree_dref_node(vcpu->node);
        vcpu->node = NULL;

        /* Acquire manager lock */
        vmm_manager_lock();

        /* Decrement vcpu count */
        mngr.vcpu_count--;

        /* Mark this VCPU as available */
        mngr.vcpu_avail_array[vcpu->id] = TRUE;

        /* Release manager lock */
        vmm_manager_unlock();

        /* Acquire Guest VCPU lock */
        vmm_write_lock_irq_save_lite(&guest->vcpu_lock, flags);
    }

    /* Release Guest VCPU lock */
    vmm_write_unlock_irq_restore_lite(&guest->vcpu_lock, flags);

    /* Acquire manager lock */
    vmm_manager_lock();

    /* Reset guest instance members */
    vmm_device_tree_dref_node(guest->node);
    guest->node    = NULL;
    guest->name[0] = '\0';
    INIT_LIST_HEAD(&guest->vcpu_list);

    /* Decrement guest count */
    mngr.guest_count--;

    /* Remove from guest list */
    list_del(&guest->head);
    INIT_LIST_HEAD(&guest->head);

    /* Mark this guest instance as available */
    mngr.guest_avail_array[guest->id] = TRUE;

    /* Release manager lock */
    vmm_manager_unlock();

    return VMM_OK;
}

int __init vmm_manager_init(void)
{
    uint32_t vnum, gnum;

    /* Reset the manager control structure */
    memset(&mngr, 0, sizeof(mngr));

    /* Intialize guest & vcpu management parameters */
    INIT_MUTEX(&mngr.lock);
    mngr.vcpu_count         = 0;
    mngr.guest_count        = 0;
    mngr.vcpu_affinity_mask = NULL;
    mngr.vcpu_array         = NULL;
    mngr.vcpu_avail_array   = NULL;
    mngr.guest_array        = NULL;
    mngr.guest_avail_array  = NULL;
    INIT_LIST_HEAD(&mngr.orphan_vcpu_list);
    INIT_LIST_HEAD(&mngr.guest_list);
    mngr.guest_work_array   = NULL;

    /* Alloc memory for guest & vcpu management */
    mngr.vcpu_affinity_mask = vmm_zalloc(CONFIG_MAX_VCPU_COUNT * sizeof(*mngr.vcpu_affinity_mask));

    if (!mngr.vcpu_affinity_mask) {
        return VMM_ENOMEM;
    }

    mngr.vcpu_array = vmm_zalloc(CONFIG_MAX_VCPU_COUNT * sizeof(*mngr.vcpu_array));

    if (!mngr.vcpu_array) {
        vmm_free(mngr.vcpu_affinity_mask);
        return VMM_ENOMEM;
    }

    mngr.vcpu_avail_array = vmm_zalloc(CONFIG_MAX_VCPU_COUNT * sizeof(*mngr.vcpu_avail_array));

    if (!mngr.vcpu_avail_array) {
        vmm_free(mngr.vcpu_array);
        vmm_free(mngr.vcpu_affinity_mask);
        return VMM_ENOMEM;
    }

    mngr.guest_array = vmm_zalloc(CONFIG_MAX_GUEST_COUNT * sizeof(*mngr.guest_array));

    if (!mngr.guest_array) {
        vmm_free(mngr.vcpu_avail_array);
        vmm_free(mngr.vcpu_array);
        vmm_free(mngr.vcpu_affinity_mask);
        return VMM_ENOMEM;
    }

    mngr.guest_avail_array = vmm_zalloc(CONFIG_MAX_GUEST_COUNT * sizeof(*mngr.guest_avail_array));

    if (!mngr.guest_avail_array) {
        vmm_free(mngr.guest_array);
        vmm_free(mngr.vcpu_avail_array);
        vmm_free(mngr.vcpu_array);
        vmm_free(mngr.vcpu_affinity_mask);
        return VMM_ENOMEM;
    }

    mngr.guest_work_array = vmm_zalloc(CONFIG_MAX_GUEST_COUNT * sizeof(*mngr.guest_work_array));

    if (!mngr.guest_work_array) {
        vmm_free(mngr.guest_avail_array);
        vmm_free(mngr.guest_array);
        vmm_free(mngr.vcpu_avail_array);
        vmm_free(mngr.vcpu_array);
        vmm_free(mngr.vcpu_affinity_mask);
        return VMM_ENOMEM;
    }

    /* Initialze memory for guest instances */
    for (gnum = 0; gnum < CONFIG_MAX_GUEST_COUNT; gnum++) {
        INIT_LIST_HEAD(&mngr.guest_array[gnum].head);
        mngr.guest_array[gnum].id   = gnum;
        mngr.guest_array[gnum].node = NULL;
        INIT_RW_LOCK(&mngr.guest_array[gnum].vcpu_lock);
        mngr.guest_array[gnum].vcpu_count = 0;
        INIT_LIST_HEAD(&mngr.guest_array[gnum].vcpu_list);
        mngr.guest_avail_array[gnum] = TRUE;
        INIT_WORK(&mngr.guest_work_array[gnum], manager_req_work);
    }

    /* Initialze memory for vcpu instances */
    for (vnum = 0; vnum < CONFIG_MAX_VCPU_COUNT; vnum++) {
        INIT_LIST_HEAD(&mngr.vcpu_array[vnum].head);
        mngr.vcpu_array[vnum].id        = vnum;
        mngr.vcpu_array[vnum].name[0]   = 0;
        mngr.vcpu_array[vnum].node      = NULL;
        mngr.vcpu_array[vnum].is_normal = FALSE;
        arch_atomic_write(&mngr.vcpu_array[vnum].state, VMM_VCPU_STATE_UNKNOWN);
        mngr.vcpu_array[vnum].state_tstamp        = 0;
        mngr.vcpu_array[vnum].state_ready_nsecs   = 0;
        mngr.vcpu_array[vnum].state_running_nsecs = 0;
        mngr.vcpu_array[vnum].state_paused_nsecs  = 0;
        mngr.vcpu_array[vnum].state_halted_nsecs  = 0;
        mngr.vcpu_array[vnum].system_nsecs        = 0;
        mngr.vcpu_array[vnum].reset_count         = 0;
        mngr.vcpu_array[vnum].reset_timestamp     = 0;
        INIT_RW_LOCK(&mngr.vcpu_array[vnum].sched_lock);
        mngr.vcpu_avail_array[vnum] = TRUE;
    }

    return VMM_OK;
}
