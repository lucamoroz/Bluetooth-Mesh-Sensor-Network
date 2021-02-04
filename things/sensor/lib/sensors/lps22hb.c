#include "lps22hb.h"

void lps22hb_handler(const struct device *dev)
{
  struct sensor_value pressure;

  if (sensor_sample_fetch(dev) < 0) {
    printk("Sensor sample update error\n");
    return;
  }

  if (sensor_channel_get(dev, SENSOR_CHAN_PRESS, &pressure) < 0) {
    printk("Cannot read LPS22HB pressure channel\n");
    return;
  }

  /* display pressure */
  printk("Pressure:%.1f kPa\n", sensor_value_to_double(&pressure));
}

const struct device *lps22hb_setup()
{
  const struct device *dev = device_get_binding(DT_LABEL(DT_INST(0, st_lps22hb_press)));

  if (dev == NULL) {
    printk("Could not get LPS22HB device\n");
  }

  return dev;
}
