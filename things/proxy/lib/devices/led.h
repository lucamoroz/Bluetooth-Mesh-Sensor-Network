#ifndef DEVICES_LED_H
#define DEVICES_LED_H

#include <drivers/gpio.h>

void led_on(int r, int g, int b);

void led_off();

/**
 * Blink the led multiple times.
 * @param times How many times the led will blink.
 * @param on_ms Time in milliseconds the led will be on.
 * @param off_ms Time in milliseconds the led will be off.
 * 
 * @note The calling thread will be suspended: consider offloading the work.
 */
void led_pulse(uint8_t times, uint32_t on_ms, uint32_t off_ms, int r, int g, int b);

void led_setup();

#endif