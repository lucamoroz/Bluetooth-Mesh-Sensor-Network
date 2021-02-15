#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <sys/util.h>
#include <sys/printk.h>

#ifndef HTS221_H
#define HTS221_H

/**
 * Setup the HTS221 sensor.
 * @return HTS221 device pointer.
 */
const struct device *hts221_setup();

/**
 * Read current temperature and humidity.
 * @param device pointer to the HTS221 device.
 * @param temp pointer to the variable that will contain the temperature value.
 * @param hum pointer to the variable that will contain the humidity value
 */
int hts221_handler(const struct device *dev, struct sensor_value *temp, struct sensor_value *hum);

#endif // HTS221_H
