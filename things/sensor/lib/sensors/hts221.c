#include "hts221.h"
#include <stdio.h>

int hts221_handler(const struct device *dev, struct sensor_value *temp, struct sensor_value *hum)
{
  if (sensor_sample_fetch(dev) < 0) {
    printk("Sensor sample update error\n");
    return -1;
  }

  if (sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, temp) < 0) {
    printk("Cannot read HTS221 temperature channel\n");
    return -1;
  }

  if (sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, hum) < 0) {
    printk("Cannot read HTS221 humidity channel\n");
    return -1;
  }

  /* display temperature */
  printf("\nTemperature:%.1f C\n", sensor_value_to_double(temp));
  
  /* display humidity */
  printf("\nRelative Humidity:%.1f%%\n", sensor_value_to_double(hum));

  return 0;
}

const struct device *hts221_setup()
{
  const struct device *dev = device_get_binding("HTS221");

  if (dev == NULL) {
    printk("Could not get HTS221 device\n");
  }

  return dev;
}
