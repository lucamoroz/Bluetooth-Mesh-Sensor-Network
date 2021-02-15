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

/**
 * Setup the CCS811 sensor with the given threshold and the function that will called.
 * @param cb function that will be executed when the CO2 value goes above/below the threshold.
 * @param trigger_threshold value that will trigger the sensor so that the callback is called.
 * @return CCS811 device pointer.
 */
const struct device *ccs811_setup(gas_data_cb cb, int32_t trigger_threshold);

/**
 * Read current CO2 ppm value.
 * @param dev pointer to the CCS811 device
 * @param ppm variable that will contain the resulting sensor value.
 */
int ccs811_fetch(const struct device *dev, struct sensor_value *ppm);

#endif // CCS811_H
