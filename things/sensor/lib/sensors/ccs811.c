#include "ccs811.h"

static bool app_fw_2;

static int ccs811_fetch(const struct device *dev)
{
  struct sensor_value co2;
  int rc = 0;

  if (rc == 0) {
    rc = sensor_sample_fetch(dev);
  }
  if (rc == 0) {
    const struct ccs811_result_type *rp = ccs811_result(dev);

    sensor_channel_get(dev, SENSOR_CHAN_CO2, &co2);
    printk("\nCCS811: %u ppm eCO2;\n", co2.val1);

    if (app_fw_2 && !(rp->status & CCS811_STATUS_DATA_READY)) {
      printk("STALE DATA\n");
    }

    if (rp->status & CCS811_STATUS_ERROR) {
      printk("ERROR: %02x\n", rp->error);
    }
  }
  return rc;
}

static void ccs811_trigger_handler(const struct device *dev, struct sensor_trigger *trig)
{
  int rc = ccs811_fetch(dev);

  if (rc == 0) {
    printk("Triggered fetch got %d\n", rc);
  } else if (-EAGAIN == rc) {
    printk("Triggered fetch got stale data\n");
  } else {
    printk("Triggered fetch failed: %d\n", rc);
  }
}

const struct device *ccs811_setup()
{
  const struct device *dev = device_get_binding(DT_LABEL(DT_INST(0, ams_ccs811)));
  struct ccs811_configver_type cfgver;
  int rc;

  if (!dev) {
    printk("Failed to get device binding");
    return NULL;
  }

  printk("device is %p, name is %s\n", dev, dev->name);

  rc = ccs811_configver_fetch(dev, &cfgver);
  if (rc == 0) {
    printk("HW %02x; FW Boot %04x App %04x ; mode %02x\n",
           cfgver.hw_version, cfgver.fw_boot_version,
           cfgver.fw_app_version, cfgver.mode);
    app_fw_2 = (cfgver.fw_app_version >> 8) > 0x11;
  }

  struct sensor_trigger trig = { 0 };
  printk("Triggering on threshold:\n");
  if (rc == 0) {
    struct sensor_value thr = {
      .val1 = 800,
    };
    rc = sensor_attr_set(dev, SENSOR_CHAN_CO2,
             SENSOR_ATTR_LOWER_THRESH,
             &thr);
    printk("L/M threshold to %d got %d\n", thr.val1, rc);
  }
  if (rc == 0) {
    struct sensor_value thr = {
      .val1 = 1200,
    };
    rc = sensor_attr_set(dev, SENSOR_CHAN_CO2,
             SENSOR_ATTR_UPPER_THRESH,
             &thr);
    printk("M/H threshold to %d got %d\n", thr.val1, rc);
  }
  trig.type = SENSOR_TRIG_THRESHOLD;
  trig.chan = SENSOR_CHAN_CO2;

  if (rc == 0) {
    rc = sensor_trigger_set(dev, &trig, ccs811_trigger_handler);
  }
  printk("Trigger installation got: %d\n", rc);

  if (rc == 0) {
    return dev;
  } else {
    return NULL;
  }
}
