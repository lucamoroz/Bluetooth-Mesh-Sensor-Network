#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <sys/util.h>
#include <sys/printk.h>

#ifndef LPS22HB_H
#define LPS22HB_H

const struct device *lps22hb_setup();
void lps22hb_handler(const struct device *dev);

#endif // LPS22HB_H
