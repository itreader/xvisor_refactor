/*
 * Generic PWM backlight driver data - see drivers/video/backlight/pwm_bl.c
 */
#ifndef __LINUX_PWM_BACKLIGHT_H
#define __LINUX_PWM_BACKLIGHT_H

#include <linux/backlight.h>

struct platform_pwm_backlight_data {
    int       pwm_id;
    uint32_t  max_brightness;
    uint32_t  dft_brightness;
    uint32_t  lth_brightness;
    uint32_t  pwm_period_ns;
    uint32_t *levels;
    /* TODO remove once all users are switched to gpiod_* API */
    int       enable_gpio;
    int (*init)(struct device *dev);
    int (*notify)(struct device *dev, int brightness);
    void (*notify_after)(struct device *dev, int brightness);
    void (*exit)(struct device *dev);
    int (*check_fb)(struct device *dev, struct frame_buffer_info *info);
};

#endif
