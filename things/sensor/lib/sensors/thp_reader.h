/**
 * Wrapper for reading temperature, humidity and pressure with one call.
 * */

#ifndef THP_READER_H
#define THP_READER_H

#include <stdint.h>

#include "ccs811.h"
#include "hts221.h"
#include "lps22hb.h"

const struct device *hts221 = NULL;
const struct device *lps22hb = NULL;

int read_thp(float *temperature, float *humidity, float *pressure) {
    struct sensor_value temp_reading, hum_reading, press_reading;
    
    if (hts221 == NULL || lps22hb == NULL) {
        printk("Can't read temperature, humidity and pressure: null ref to devices");
        return -1;
    }

    if (hts221_handler(hts221, &temp_reading, &hum_reading) < 0 || lps22hb_handler(lps22hb, &press_reading) < 0) {
        printk("Failed reading temperature, humidity and pressure\n");
        return -1;
    }

    if (lps22hb_handler(lps22hb, &press_reading) < 0) {
        printk("Failed reading pressure\n");
        return -1;
    }

    // Values are converted to 16-bit integers due to errors sending 
    // 32-bit values using net_buf_simple
    *temperature = (float) sensor_value_to_double(&temp_reading);
    *humidity = (float) sensor_value_to_double(&hum_reading);
    *pressure = (float) sensor_value_to_double(&press_reading);  

    return 0;
}

int thp_reader_setup() {
    hts221 = hts221_setup();
    lps22hb = lps22hb_setup();

    if (hts221 == NULL || lps22hb == NULL) {
        return -1;
    }
    
    return 0;
}


#endif //THP_READER_H