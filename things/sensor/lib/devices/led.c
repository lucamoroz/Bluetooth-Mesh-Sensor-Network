#include "led.h"

// GPIO for the Thingy LED controller
const struct device *led_ctrlr = NULL;

void led_on(int r, int g, int b) {
    // LEDs on Thingy are "active low" so zero means on. Args are expressed as RGB 0-255 values so we map them to GPIO low/high.
	r = !(r / 255);
	g = !(g / 255);
	b = !(b / 255);

	gpio_pin_set(led_ctrlr, LED_R, r);
	gpio_pin_set(led_ctrlr, LED_G, g);
	gpio_pin_set(led_ctrlr, LED_B, b);
}

void led_off() {
    gpio_pin_set(led_ctrlr, LED_R, 1);
	gpio_pin_set(led_ctrlr, LED_G, 1);
	gpio_pin_set(led_ctrlr, LED_B, 1);
}

void led_pulse(uint8_t times, uint32_t on_ms, uint32_t off_ms, int r, int g, int b) {
	for (int i=0; i<times; i++) {
		led_on(r, g, b);
		k_msleep(on_ms);
		led_off();
		if (i < times-1) {
			k_msleep(off_ms);
		}
	}	
}

void led_setup() {
    if (led_ctrlr != NULL) {
        printk("Led already configured!\n");
    }

    printk("Configuring led\n");
    led_ctrlr = device_get_binding(PORT);
	gpio_pin_configure(led_ctrlr, LED_R, GPIO_OUTPUT);
	gpio_pin_configure(led_ctrlr, LED_G, GPIO_OUTPUT);
	gpio_pin_configure(led_ctrlr, LED_B, GPIO_OUTPUT);
}