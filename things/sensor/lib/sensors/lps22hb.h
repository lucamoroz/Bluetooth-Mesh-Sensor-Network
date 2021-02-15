#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <sys/util.h>
#include <sys/printk.h>

#ifndef LPS22HB_H
#define LPS22HB_H


/**
 * Setup the LPS22HB sensor.
 * @return LPS22HB device pointer.
 */
const struct device *lps22hb_setup();

/**
 * Read the current pressure.
 * @param pressure pointer to the variable that will contain the pressure.
 */
int lps22hb_handler(const struct device *dev, struct sensor_value *pressure);

#endif // LPS22HB_H
