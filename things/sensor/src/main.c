#include <stdlib.h>
#include <bluetooth/bluetooth.h>
#include <settings/settings.h>
#include <bluetooth/mesh.h>

#include <../lib/models/thp_sensor.h>
#include <../lib/models/gas_sensor.h>
#include <../lib/models/generic_onoff.h>
#include <../lib/devices/led.h>
#include <../lib/devices/button.h>

#define GAS_TRIGGER_THRESHOLD 800
#define THP_MODEL_PUB_PERIOD 60

struct k_delayed_work thp_autoconf_work;
struct k_delayed_work gas_autoconf_work;
struct k_delayed_work gen_onoff_autoconf_work;
extern void thp_autoconf_handler(struct k_work *item);
extern void gas_autoconf_handler(struct k_work *item);
extern void gen_onoff_autoconf_handler(struct k_work *item);

// usually set by the manufacturer - hard coded here for convenience
// device UUID
static const uint8_t dev_uuid[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x01 };


// provisioning callback functions
static void attention_on(struct bt_mesh_model *model) {
	printk("attention_on()\n");
	led_on(0,255,0);
}

static void attention_off(struct bt_mesh_model *model) {
	printk("attention_off()\n");
	led_off();
}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = attention_on,
	.attn_off = attention_off,
};

static int provisioning_output_pin(bt_mesh_output_action_t action, uint32_t number) {
	printk("OOB Number: %u\n", number);
	return 0;
}

static void provisioning_complete(uint16_t net_idx, uint16_t addr) {
    printk("Provisioning completed: address = %d\n", addr);
}

static void provisioning_reset(void) {
	bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
}

// provisioning properties and capabilities
static const struct bt_mesh_prov prov = {
	.uuid = dev_uuid,
	.output_size = 4,
	.output_actions = BT_MESH_DISPLAY_NUMBER,
	.output_number = provisioning_output_pin,
	.complete = provisioning_complete,
	.reset = provisioning_reset,
};


// -------------------------------------------------------------------------------------------------------
// Configuration Server
// --------------------
static struct bt_mesh_cfg_srv cfg_srv = {
		.relay = BT_MESH_RELAY_DISABLED,
		.beacon = BT_MESH_BEACON_DISABLED,
		.frnd = BT_MESH_FRIEND_NOT_SUPPORTED,
		.gatt_proxy = BT_MESH_GATT_PROXY_ENABLED,
		.default_ttl = 7,
		/* 3 transmissions with 20ms interval */
		.net_transmit = BT_MESH_TRANSMIT(2, 20),
};

// -------------------------------------------------------------------------------------------------------
// Config client
// -------------
static struct bt_mesh_cfg_cli cfg_cli = {};

// -------------------------------------------------------------------------------------------------------
// Health Server
// -------------
BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);
static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

// -------------------------------------------------------------------------------------------------------
// Composition
// -----------
static struct bt_mesh_model root_models[] = {
		BT_MESH_MODEL_CFG_SRV(&cfg_srv),
		BT_MESH_MODEL_CFG_CLI(&cfg_cli),
		BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
        GENERIC_ONOFF_MODEL,
		THP_SENSOR_MODEL,
};

static struct bt_mesh_model sens_gas_model[] = {
	GAS_SENSOR_MODEL,
};

static struct bt_mesh_elem elements[] = {
		BT_MESH_ELEM(0, root_models, BT_MESH_MODEL_NONE),
		BT_MESH_ELEM(1, sens_gas_model, BT_MESH_MODEL_NONE),
};

// node
static const struct bt_mesh_comp comp = {
		.cid = 0xFFFF,
		.elem = elements,
		.elem_count = ARRAY_SIZE(elements),
};

// -------------------------------------------------------------------------------------------------------
// Self-configuration
// -----------

void thp_autoconf_handler(struct k_work *item) {
	uint8_t err;
	uint16_t root_addr = elements[0].addr;

	err = thp_sensor_autoconf(root_addr, elements[0].addr, THP_MODEL_PUB_PERIOD);
	if (err) {
		printk("Error setting default config of thp sensor model\n");
	}
}

void gas_autoconf_handler(struct k_work *item) {
	uint8_t err;
	uint16_t root_addr = elements[0].addr;

	err = gas_sensor_autoconf(root_addr, elements[1].addr);
	if (err) {
		printk("Error setting default config of gas sensor model\n");
	}
}

void gen_onoff_autoconf_handler(struct k_work *item) {
	uint8_t err;
	uint16_t root_addr = elements[0].addr;

	err = generic_onoff_autoconf(root_addr, elements[0].addr);
	if (err) {
		printk("Error setting default config of generic onoff model\n");
	}
}

void button_callback(uint8_t click_type) {
	if (click_type == FAST_CLICK) {
		// Display first element address, which is set during provisioning. 
		// The address of the second element is always the address of the first element plus one, 
		// therefore only the first is shown with led_pulse.
		if (bt_mesh_is_provisioned()) {
			printk("First element address: %d, second element address: %d\n", elements[0].addr, elements[1].addr);
			k_msleep(200);
			led_pulse(elements[0].addr, 500, 200, 255, 255, 255);	
		} else {
			printk("Node not yet provisioned!\n");
			k_msleep(200);
			led_pulse(4, 100, 100, 255, 0, 0);
		}
	} else if (click_type == LONG_CLICK) {
		// Autoconf must be performed with a delay between one model and the next one to allow the bluetooth stack to
		// allocate transmission buffers
		k_delayed_work_submit(&gas_autoconf_work, K_SECONDS(2));
		k_delayed_work_submit(&gen_onoff_autoconf_work, K_SECONDS(6));
		k_delayed_work_submit(&thp_autoconf_work, K_SECONDS(10));
		// show green feedback
		led_pulse(2, 300, 100, 0, 255, 0);

	} else if (click_type == LONG_LONG_CLICK) {
		printk("Resetting node to unprovisioned\n");
		// show red feedback
		led_pulse(2, 300, 100, 255, 0, 0);
		bt_mesh_reset();
	} else {
		printk("Button callback warning: unknown click type");
	}
}

void gas_cb(uint16_t ppm) {
	printk("gas_cb\n");
	if (ppm > GAS_TRIGGER_THRESHOLD) {
		printk("warning user: ppm above threshold\n");
		led_on(255, 0, 0);
	} else {
		printk("removing user warning: ppm below threshold\n");
		led_off();
	}
}

static void bt_ready(int err) {
	if (err) {
		printk("bt_enable init failed with err %d\n", err);
		return;
	}
    printk("Bluetooth initialised OK\n");
	err = bt_mesh_init(&prov, &comp);

	if (err) {
		printk("bt_mesh_init failed with err %d\n", err);
		return;
	}

	printk("Mesh initialised OK\n");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	    printk("Settings loaded\n");
	}

    if (!bt_mesh_is_provisioned()) {
    	printk("Node has not been provisioned - beaconing\n");
		bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
	} else {
    	printk("Node has already been provisioned\n");
	}

}

void main(void) {
	printk("\n\n----- THINGY 52 SENSOR NODE -----\n\n");
	
	button_setup(&button_callback);

	int err = bt_enable(bt_ready);
	if (err) {
		printk("bt_enable failed with err %d\n", err);
	}
	
	err = thp_sensor_setup();
	if (err) {
		printk("Error starting thp sensor\n");
	}

	err = gas_sensor_setup(GAS_TRIGGER_THRESHOLD, &gas_cb);
	if (err) {
		printk("Error starting gas sensor\n");
	}

	generic_onoff_setup();

	k_delayed_work_init(&thp_autoconf_work, thp_autoconf_handler);
	k_delayed_work_init(&gas_autoconf_work, gas_autoconf_handler);
	k_delayed_work_init(&gen_onoff_autoconf_work, gen_onoff_autoconf_handler);

	// show "ready"
	led_setup();
	led_pulse(1, 500, 0, 0, 0, 255);
}