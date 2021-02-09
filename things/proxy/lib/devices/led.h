#ifndef DEVICES_LED_H
#define DEVICES_LED_H

#include <drivers/gpio.h>

#define PORT "GPIO_P0"
#define LED_R 7
#define LED_G 5
#define LED_B 6

void led_on(int r, int g, int b);

void led_off();

void led_setup();

#endif