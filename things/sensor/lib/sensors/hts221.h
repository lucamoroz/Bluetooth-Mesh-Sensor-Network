#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <sys/util.h>
#include <sys/printk.h>

#ifndef HTS221_H
#define HTS221_H

const struct device *hts221_setup();
int hts221_handler(const struct device *dev, struct sensor_value *temp, struct sensor_value *hum);

#endif // HTS221_H
