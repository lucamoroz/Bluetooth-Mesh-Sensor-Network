#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <sys/util.h>
#include <sys/printk.h>

#ifndef HTS221_H
#define HTS221_H

const struct device *hts221_setup();
void hts221_handler(const struct device *dev);

#endif // HTS221_H
