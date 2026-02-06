#ifndef MC146818RTC_H
#define MC146818RTC_H

#include <vmm_timer.h>
#include "emu/rtc/mc146818rtc_regs.h"

struct cmos_rtc_state;
typedef struct cmos_rtc_state cmos_rtc_state_t;

struct cmos_rtc_state {
    uint8_t             cmos_data[128];
    uint8_t             cmos_index;
    int32_t             base_year;
    uint64_t            base_rtc;
    uint64_t            last_update;
    int64_t             offset;
    uint32_t            irq;
    int                 it_shift;
    /* periodic timer */
    vmm_timer_event_t   periodic_timer;
    int64_t             next_periodic_time;
    /* update-ended timer */
    vmm_timer_event_t   update_timer;
    uint64_t            next_alarm_time;
    uint16_t            irq_reinject_on_ack_count;
    uint32_t            period;
    struct vmm_guest   *guest;
    struct vmm_spinlock lock;
    uint8_t (*rtc_cmos_read)(struct cmos_rtc_state *state, uint32_t offset);
    int (*rtc_cmos_write)(struct cmos_rtc_state *state, uint32_t offset, uint8_t value);
};

extern void __weak arch_guest_set_cmos(struct vmm_guest *guest, struct cmos_rtc_state *s);

#endif /* !MC146818RTC_H */
