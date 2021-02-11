#ifndef DEVICES_BUTTON_H
#define DEVICES_BUTTON_H

#include <drivers/gpio.h>

#define BUTTON_DEBOUNCE_DELAY_MS 200
#define LONG_CLICK_MS 3000
#define LONG_LONG_CLICK_MS 10000

#define FAST_CLICK 1
#define LONG_CLICK 2
#define LONG_LONG_CLICK 3

/**
 * Button click called.
 * @param type can be one of FAST_CLICK, LONG_CLICK, or LONG_LONG_CLICK depending on
 *             how long the user has pressed the button.
 */
typedef void (*button_cb)(uint8_t click_type);

void button_setup(button_cb cb);

#endif