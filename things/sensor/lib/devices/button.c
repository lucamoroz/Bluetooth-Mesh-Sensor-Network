#include <button.h>

// GPIO for the button
#define SW0_NODE	DT_ALIAS(sw0)
#define SW0_GPIO_LABEL	DT_GPIO_LABEL(SW0_NODE, gpios)
#define SW0_GPIO_PIN	DT_GPIO_PIN(SW0_NODE, gpios)
#define SW0_GPIO_FLAGS	(GPIO_INPUT | DT_GPIO_FLAGS(SW0_NODE, gpios) | GPIO_INT_EDGE)

// for debouncing the button
static uint32_t btn_time = 0;
static uint32_t btn_last_time = 0;

static struct gpio_callback button_cb_data;

struct button_work_info {
	struct k_work work;
	bool is_running;
} work_info;


button_cb btn_cb = NULL;

bool debounce() {
	btn_time = k_uptime_get_32();
    bool ignore = btn_time < (btn_last_time + BUTTON_DEBOUNCE_DELAY_MS);
	btn_last_time = btn_time;
	return ignore;
}

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    if (debounce()) {
        return;
    }
	printk("Button pressed\n");

    if (btn_cb == NULL) {
            printk("no callback associated with button!\n");
            return;
    }

	
	if (work_info.is_running) {
		printk("Work not submitted: previously submitted work still running\n");
	} else {
		printk("Submitting work\n");
		k_work_submit(&work_info.work);
	}
}

void button_work_handler(struct k_work *item) {
	struct button_work_info *w_info = CONTAINER_OF(item, struct button_work_info, work);
	w_info->is_running = true;
	btn_cb();
	w_info->is_running = false;
}

void button_setup(button_cb cb) {
    int ret;
	printk("button_setup\n");

	const struct device *button;

	button = device_get_binding(SW0_GPIO_LABEL);
	if (button == NULL) {
		printk("Error: didn't find %s device\n", SW0_GPIO_LABEL);
		return;
	}

	ret = gpio_pin_configure(button, SW0_GPIO_PIN, SW0_GPIO_FLAGS);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n", ret, SW0_GPIO_LABEL, SW0_GPIO_PIN);
		return;
	}

	ret = gpio_pin_interrupt_configure(button, SW0_GPIO_PIN, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, SW0_GPIO_LABEL, SW0_GPIO_PIN);
		return;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(SW0_GPIO_PIN));
	gpio_add_callback(button, &button_cb_data);
	printk("Set up button at %s pin %d\n", SW0_GPIO_LABEL, SW0_GPIO_PIN);

	k_work_init(&work_info.work, button_work_handler);

    btn_cb = cb;
}



