#include <zephyr.h>
#include <sys/printk.h>
#include <drivers/gpio.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh.h>
#include <settings/settings.h>
#include <bluetooth/mesh/proxy.h>

#include <../lib/models/sensor_cli.h>
#include <../lib/models/gen_onoff_cli.h>
#include <../lib/devices/led.h>
#include <../lib/devices/button.h>

#define GAS_TRIGGER_THRESHOLD 800

struct k_delayed_work sens_cli_autoconf_work;
struct k_delayed_work gen_onoff_cli_autoconf_work;
extern void sens_cli_autoconf_handler(struct k_work *item);
extern void gen_onoff_cli_autoconf_handler(struct k_work *item);

int op_id = 0;

static const uint8_t dev_uuid[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x00 };

#define GAS_TRIGGERED_NODES_CAPACITY 128
static uint8_t gas_triggered_nodes[GAS_TRIGGERED_NODES_CAPACITY];

static void attention_on(struct bt_mesh_model *model) {
	printk("attention_on\n");
	led_on(0, 255, 0);
}

static void attention_off(struct bt_mesh_model *model) {
	printk("attention_off\n");
	led_off();
}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = attention_on,
	.attn_off = attention_off,
};

// called to output the provisioned number
static int provisioning_output_pin(bt_mesh_output_action_t act, uint32_t number) {
	printk("OOB number: %04d\n", number);
	
	// only 3-digits numbers supported
	if (number > 999) {
		return 0; 
	}

	// Show the pin by blinking the led with 3 different lights
	k_msleep(2000);
	led_pulse((number/100)%10, 500, 200, 255, 0, 0);
	k_msleep(2000);
	led_pulse((number/10)%10, 500, 200, 0, 255, 0);
	k_msleep(2000);
	led_pulse(number%10, 500, 200, 0, 0, 255);	

	return 0;
}

static void provisioning_complete(uint16_t net_idx, uint16_t addr) {
	printk("Provisioning complete\n");
}

static void provisioning_reset(void) {
	bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
}

// provisioning properties and capabilities
static const struct bt_mesh_prov prov = {
	.uuid = dev_uuid,
	// Authenticate the device by displaying a 4 digit PIN
	.output_size = 3,
	.output_actions = BT_MESH_DISPLAY_NUMBER,
	.output_number = provisioning_output_pin, // callback called with a rndm number generated by the bt stack
	.complete = provisioning_complete,
	.reset = provisioning_reset, // called on provisioning abandoned
};


// -------------------------------------------------------------------------------------------------------
// Configuration Server
// --------------------
static struct bt_mesh_cfg_srv cfg_srv = {
	.relay = BT_MESH_RELAY_DISABLED,
	.beacon = BT_MESH_BEACON_DISABLED,
	.frnd = BT_MESH_FRIEND_NOT_SUPPORTED,
	// enabled to allow provisioning over GATT, which is supported by nRF Mesh Application
	.gatt_proxy = BT_MESH_GATT_PROXY_ENABLED,
	.default_ttl = 7,
	// 3 transmissions with 20ms interval
	.net_transmit = BT_MESH_TRANSMIT(2, 20)
};

// -------------------------------------------------------------------------------------------------------
// Config client
// -------------
static struct bt_mesh_cfg_cli cfg_cli = {};

// -------------------------------------------------------------------------------------------------------
// Health Server
// -------------
// This model can publish diagnostic messages. Macro used to define publication context
BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);
static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

// -------------------------------------------------------------------------------------------------------
// Composition
// -----------
static struct bt_mesh_model sig_models[] = {
	BT_MESH_MODEL_CFG_SRV(&cfg_srv),
	BT_MESH_MODEL_CFG_CLI(&cfg_cli),
	BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
	GEN_ONOFF_CLI_MODEL,
	SENSOR_CLIENT_MODEL,
};

// define the element(s) which contain the previously defined models
static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(0, sig_models, BT_MESH_MODEL_NONE), // .._MODEL_NONE means no vendor models
};

// define the node containing the elements (composition)
static const struct bt_mesh_comp comp = {
	.cid = 0xFFFF, // test value company id 
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

// -------------------------------------------------------------------------------------------------------
// Self-configuration
// -------
void sens_cli_autoconf_handler(struct k_work *item) {
	uint8_t err;
	uint16_t root_addr = elements[0].addr;

	err = sensor_cli_autoconf(root_addr, elements[0].addr);
	if (err) {
		printk("Error setting default config of gas sensor model\n");
	}
}

void gen_onoff_cli_autoconf_handler(struct k_work *item) {
	uint8_t err;
	uint16_t root_addr = elements[0].addr;

	err = gen_onoff_autoconf(root_addr, elements[0].addr);
	if (err) {
		printk("Error setting default config of gas sensor model\n");
	}
}

// -------------------------------------------------------------------------------------------------------
// Data callbacks
// -------
void thp_data_callback(float temperature, float humidity, float pressure, uint16_t node_addr) {
	printf("\n\nthp_data_callback received temp: %.2f, hum: %.2f, press: %.2f. Node address: 0x%02x\n\n", temperature, humidity, pressure, node_addr);
}

/** 
 * Track how many nodes detected co2 ppm above threshold. If any (in range [0, 127]): show a green light.
 */ 
void gas_data_callback(uint16_t ppm, uint16_t node_addr) {
	printf("\n\ngas_data_callback received ppm: %d. Node address: 0x%02x\n\n", ppm, node_addr);

	if (node_addr > 127) {
		printk("Node %d trigger not tracked: too high node id\n", node_addr);
		return;
	}

	if (ppm > GAS_TRIGGER_THRESHOLD) {
		if (gas_triggered_nodes[node_addr]) {
			// gas trigger already set, therefore led is already on
			return;
		}

		gas_triggered_nodes[node_addr] = 1;
		led_on(0, 255, 0);
	} else {
		gas_triggered_nodes[node_addr] = 0;

		// check if all tracked nodes in the mesh have co2 below theshold. If so, turn off led
		for (uint16_t i=0; i<GAS_TRIGGERED_NODES_CAPACITY; i++) {
			if (gas_triggered_nodes[i]) {
				return;
			}
		}
		led_off();
	}
}


void button_callback(uint8_t click_type) {
	if (click_type == FAST_CLICK) {

		// show blue feedback
		led_pulse(2, 300, 100, 0, 0, 255);

		if (op_id % 3 == 0) {
			gen_onoff_set_unack(0);
		} else if (op_id % 3 == 1) {
			gen_onoff_set_unack(1);
		} else if (op_id % 3 == 2) {
			sensor_cli_get(&sig_models[3]);
		}
		op_id++;

	} else if (click_type == LONG_CLICK) {
		// Autoconf must be performed with a delay between one model and the next one to allow the bluetooth stack to
		// allocate transmission buffers
		k_delayed_work_submit(&sens_cli_autoconf_work, K_SECONDS(2));
		k_delayed_work_submit(&gen_onoff_cli_autoconf_work, K_SECONDS(6));
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

static void bt_ready(int err) {
	if(err) {
		printk("bt_enable failed with with err %d\n", err);
		return;
	}

	printk("Bluetooth initialized\n");
	
	err = bt_mesh_init(&prov, &comp);
	if(err) {
		printk("bt_mesh_init failed with err %d\n", err);
		return;
	}

	printk("Mesh initialized\n");
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		// restore persisted data including mesh stack variables (e.g. net key and app key if provisioned)
		settings_load();
		printk("Settings loaded\n");
	}

	if (bt_mesh_is_provisioned()) {
		printk("Node has already been provisioned\n");
	} else {
		printk("Node has not been provisioned: beaconing\n");
		bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT); // provision using either gatt or advertising bearer
	}
}


void main(void) {
	printk("\n\n----- THINGY 52 PROXY NODE -----\n\n");
	int err;

	button_setup(&button_callback);
	led_setup();

	err = bt_enable(bt_ready);
	if (err) {
		printk("bt_enable failed with err %d\n", err);
	}
	
	sensor_cli_set_thp_callback(&thp_data_callback);
	sensor_cli_set_gas_callback(&gas_data_callback);

	k_delayed_work_init(&sens_cli_autoconf_work, sens_cli_autoconf_handler);
	k_delayed_work_init(&gen_onoff_cli_autoconf_work, gen_onoff_cli_autoconf_handler);

	// show "ready"
	led_setup();
	led_pulse(1, 500, 0, 0, 255, 0);
}
