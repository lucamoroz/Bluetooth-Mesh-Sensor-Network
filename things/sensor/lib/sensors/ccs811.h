#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <sys/printk.h>
#include <sys/util.h>
#include <drivers/sensor/ccs811.h>
#include <stdio.h>

#ifndef CCS811_H
#define CCS811_H

const struct device *ccs811_setup();

#endif // CCS811_H
