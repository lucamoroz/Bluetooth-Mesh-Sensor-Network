#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <sys/printk.h>
#include <sys/util.h>
#include <drivers/sensor/ccs811.h>
#include <stdio.h>

#ifndef CCS811_H
#define CCS811_H

typedef void (*gas_data_cb)(struct sensor_value *ppm);

const struct device *ccs811_setup(gas_data_cb cb, int32_t trigger_threshold);
int ccs811_fetch(const struct device *dev, struct sensor_value *ppm);

#endif // CCS811_H
