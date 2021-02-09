#ifndef DEVICES_BUTTON_H
#define DEVICES_BUTTON_H

#include <drivers/gpio.h>

#define BUTTON_DEBOUNCE_DELAY_MS 200

typedef void (*button_cb)();

void button_setup(button_cb cb);

#endif