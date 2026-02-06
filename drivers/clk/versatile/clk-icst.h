#include <asm/hardware/icst.h>

/**
 * struct clock_icst_desc - descriptor for the ICST VCO
 * @params: ICST parameters
 * @vco_offset: offset to the ICST VCO from the provided memory base
 * @lock_offset: offset to the ICST VCO locking register from the provided
 *  memory base
 */
struct clock_icst_desc {
    const struct icst_params *params;
    uint32_t                  vco_offset;
    uint32_t                  lock_offset;
};

struct clk *icst_clock_register(
    struct device *dev, const struct clock_icst_desc *desc, const char *name, const char *parent_name, void __iomem *base);
