#ifndef VERSATILE_CLOCK_H
#define VERSATILE_CLOCK_H

#include <icst.h>

struct clock_ops;

struct clk {
    uint64_t                  rate;
    const struct clock_ops   *ops;
    const struct icst_params *params;
    void                     *vcoreg;
};

struct clock_ops {
    long (*round)(struct clk *, uint64_t);
    int (*set)(struct clk *, uint64_t);
    void (*setvco)(struct clk *, struct icst_vco);
};

int  icst_clock_set(struct clk *, uint64_t);
long icst_clock_round(struct clk *, uint64_t);

#endif
