#ifndef _LINUX_OF_DEVICE_H
#define _LINUX_OF_DEVICE_H

#include <linux/of.h>
#include <linux/platform_device.h>

static inline int of_driver_match_device(struct device *dev, struct device_driver *drv)
{
    const struct vmm_device_tree_nodeid *match;

    if (!dev || !dev->of_node || !drv || !drv->match_table) {
        return 0;
    }

    match = vmm_device_tree_match_node(drv->match_table, dev->of_node);

    return (match) ? 1 : 0;
}

#endif
