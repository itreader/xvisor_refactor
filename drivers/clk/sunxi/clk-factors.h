#ifndef __MACH_SUNXI_CLK_FACTORS_H
#define __MACH_SUNXI_CLK_FACTORS_H

#include <linux/clk-provider.h>
#include <linux/clkdev.h>

#define SUNXI_FACTORS_NOT_APPLICABLE (0)

struct clock_factors_config {
    uint8_t nshift;
    uint8_t nwidth;
    uint8_t kshift;
    uint8_t kwidth;
    uint8_t mshift;
    uint8_t mwidth;
    uint8_t pshift;
    uint8_t pwidth;
};

struct clock_factors {
    struct clock_hw              hw;
    void __iomem                *reg;
    struct clock_factors_config *config;
    void (*get_factors)(uint32_t *rate, uint32_t parent, uint8_t *n, uint8_t *k, uint8_t *m, uint8_t *p);
    spinlock_t *lock;
};

extern const struct clock_ops clock_factors_ops;
#endif
