#include <button.h>

// GPIO for the button
#define SW0_NODE	DT_ALIAS(sw0)
#define SW0_GPIO_LABEL	DT_GPIO_LABEL(SW0_NODE, gpios)
#define SW0_GPIO_PIN	DT_GPIO_PIN(SW0_NODE, gpios)
#define SW0_GPIO_FLAGS	(GPIO_INPUT | DT_GPIO_FLAGS(SW0_NODE, gpios) | GPIO_INT_EDGE)

const struct device *button;

#define BTN_DOWN 2148744
#define BTN_UP 2146696

// for debouncing the button
static uint32_t btn_time = 0;
static uint32_t btn_last_time = 0;

// for reading quick clicks, long clicks or long long clicks
static uint32_t btn_down_time;

static struct gpio_callback button_cb_data;

struct button_work_info {
	struct k_work work;
	bool is_running;
	uint8_t click_type;
} work_info;


button_cb btn_cb = NULL;

bool debounce() {
	btn_time = k_uptime_get_32();
    bool ignore = btn_time < (btn_last_time + BUTTON_DEBOUNCE_DELAY_MS);
	btn_last_time = btn_time;
	return ignore;
}

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
	int err;

	if (btn_cb == NULL) {
        printk("no callback associated with button!\n");
    	return;
    }

	gpio_port_value_t value;
	err = gpio_port_get(button, &value);
	if (err) {
		printk("Error reading button state\n");
		return;
	}
	
    if (value == BTN_DOWN && debounce()) {
        return; // Ignore fast consecutive clicks
    }

	if (value == BTN_DOWN) {
		printk("Button down\n");
		btn_down_time = k_uptime_get_32();
	} else if (value == BTN_UP) {
		printk("Button up\n");
		uint32_t delta_time = k_uptime_get_32() - btn_down_time;

		if (work_info.is_running) {
			printk("Work not submitted: previously submitted work still running\n");
			return;
		}

		work_info.click_type = FAST_CLICK;
		if (delta_time > LONG_CLICK_MS && delta_time < LONG_LONG_CLICK_MS) {
			work_info.click_type = LONG_CLICK;
		} else if (delta_time > LONG_LONG_CLICK_MS) {
			work_info.click_type = LONG_LONG_CLICK;
		}

		printk("Submitting work with click type %d\n", work_info.click_type);
		k_work_submit(&work_info.work);

	} else {
		printk("Unknown button value\n");
		return;
	}
}

void button_work_handler(struct k_work *item) {
	struct button_work_info *w_info = CONTAINER_OF(item, struct button_work_info, work);
	w_info->is_running = true;
	btn_cb(w_info->click_type);
	w_info->is_running = false;
}

void button_setup(button_cb cb) {
    int ret;
	printk("button_setup\n");

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

	ret = gpio_pin_interrupt_configure(button, SW0_GPIO_PIN, GPIO_INT_EDGE_BOTH);
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