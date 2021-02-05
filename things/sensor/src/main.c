#include <stdlib.h>
#include <bluetooth/bluetooth.h>
#include <settings/settings.h>
#include <bluetooth/mesh.h>

#include <../lib/models/thp_sensor.h>
#include <../lib/models/gas_sensor.h>
#include <../lib/models/generic_onoff.h>

#define GAS_TRIGGER_THRESHOLD 800

// usually set by the manufacturer - hard coded here for convenience
// device UUID
static const uint8_t dev_uuid[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x01 };


// provisioning callback functions
static void attention_on(struct bt_mesh_model *model) {
	printk("attention_on()\n");
	thingy_led_on(255,0,0);
}

static void attention_off(struct bt_mesh_model *model) {
	printk("attention_off()\n");
	thingy_led_off();
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
    printk("Provisioning completed\n");
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

gas_sensor_trigger_callback gas_cb(uint16_t ppm) {
	printk("gas_cb\n");
	if (ppm > GAS_TRIGGER_THRESHOLD) {
		printk("warning user: ppm above threshold\n");
		thingy_led_on(255, 0, 0);
	} else {
		printk("removing user warning: ppm below threshold\n");
		thingy_led_off();
	}
}

static void bt_ready(int err) {
	if (err)
	{
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

	/* This will be a no-op if settings_load() loaded provisioning info */
	/* run nrfjprog -e against your board (assuming it's a Nordic board) to clear provisioning data and start again */

    if (!bt_mesh_is_provisioned()) {
    	printk("Node has not been provisioned - beaconing\n");
		bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);
	} else {
    	printk("Node has already been provisioned\n");
	}

}

void main(void) {
	printk("thingy sensor node\n");

	configure_thingy_led_controller();

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
}