#include "led.h"

#define PORT "GPIO_P0"
#define LED_R 7
#define LED_G 5
#define LED_B 6

// GPIO for the Thingy LED controller
const struct device *led_ctrlr = NULL;
K_MUTEX_DEFINE(led_mutex);
int curr_r, curr_g, curr_b ;

void led_on(int r, int g, int b) {

	k_mutex_lock(&led_mutex, K_FOREVER);

	curr_r = r;
	curr_g = g;
	curr_b = b;

    // LEDs on Thingy are "active low" so zero means on. Args are expressed as RGB 0-255 values so we map them to GPIO low/high.
	r = !(r / 255);
	g = !(g / 255);
	b = !(b / 255);

	gpio_pin_set(led_ctrlr, LED_R, r);
	gpio_pin_set(led_ctrlr, LED_G, g);
	gpio_pin_set(led_ctrlr, LED_B, b);

	k_mutex_unlock(&led_mutex);
}

void led_off() {
	led_on(0, 0, 0);
}

void led_pulse(uint8_t times, uint32_t on_ms, uint32_t off_ms, int r, int g, int b) {
	int last_r, last_g, last_b;

	k_mutex_lock(&led_mutex, K_FOREVER);
	
	last_r = curr_r;
	last_g = curr_g;
	last_b = curr_b;
	
	for (int i=0; i<times; i++) {
		led_on(r, g, b);
		k_msleep(on_ms);
		led_off();
		if (i < times-1) {
			k_msleep(off_ms);
		}
	}

	// restore previous state
	led_on(last_r, last_g, last_b);

	k_mutex_unlock(&led_mutex);
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