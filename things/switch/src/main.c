#include <zephyr.h>
#include <sys/printk.h>
#include <drivers/gpio.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh.h>
#include <settings/settings.h>
#include <bluetooth/mesh/proxy.h>

// GPIO for the buttons
#define SW0_NODE	DT_ALIAS(sw0)

#if DT_NODE_HAS_STATUS(SW0_NODE, okay)
#define SW0_GPIO_LABEL	DT_GPIO_LABEL(SW0_NODE, gpios)
#define SW0_GPIO_PIN	DT_GPIO_PIN(SW0_NODE, gpios)
#define SW0_GPIO_FLAGS	(GPIO_INPUT | DT_GPIO_FLAGS(SW0_NODE, gpios) | GPIO_INT_EDGE)
#else
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif

#define BUTTON_DEBOUNCE_DELAY_MS 250

// for debouncing the button
static uint32_t btn_time = 0;
static uint32_t btn_last_time = 0;

static struct gpio_callback button_cb_data;

// GPIO for LED 0
struct device *led;
int op_id = 0;

#define LED0_NODE	DT_ALIAS(led0)

#if DT_NODE_HAS_STATUS(LED0_NODE, okay) && DT_NODE_HAS_PROP(LED0_NODE, gpios)
#define LED0_GPIO_LABEL	DT_GPIO_LABEL(LED0_NODE, gpios)
#define LED0_GPIO_PIN	DT_GPIO_PIN(LED0_NODE, gpios)
#define LED0_GPIO_FLAGS	(GPIO_OUTPUT | DT_GPIO_FLAGS(LED0_NODE, gpios))
#else
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

static const uint8_t dev_uuid[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x00 };

static uint8_t onoff_tid;
static uint8_t hsl_tid;

void ledOn(void) {
	printk("turning led on\n");
	gpio_pin_set(led, LED0_GPIO_PIN, 1);
}

void ledOff(void) {
	printk("turning led off\n");
	gpio_pin_set(led, LED0_GPIO_PIN, 0);
}

static void attention_on(struct bt_mesh_model *model) {
	printk("attention_on\n");
	ledOn();
}

static void attention_off(struct bt_mesh_model *model) {
	printk("attention_off\n");
	ledOff();
}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = attention_on,
	.attn_off = attention_off,
};

// called to output the provisioned number
static int provisioning_output_pin(bt_mesh_output_action_t act, uint32_t num) {
	printk("OOB number: %04d\n", num);
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
	.output_size = 4,
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
// Health Server
// -------------
// This model can publish diagnostic messages. Macro used to define publication context
BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);
static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

// -------------------------------------------------------------------------------------------------------
// Generic OnOff Client Model
// --------------------------

uint8_t onoff[] = {0,1};

// generic on off client - handler functions for this model's RX messages
static void generic_onoff_status(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf) 
{
	// buf contains the message payload (opcode excluded)
	printk("generic_onoff_status\n");
	uint8_t onoff_state = net_buf_simple_pull_u8(buf);
	printk("generic_onoff_status onoff=%d\n", onoff_state);
}

// generic on off client - message types defined by this model.
#define BT_MESH_MODEL_OP_GENERIC_ONOFF_GET BT_MESH_MODEL_OP_2(0x82, 0x01)
#define BT_MESH_MODEL_OP_GENERIC_ONOFF_SET BT_MESH_MODEL_OP_2(0x82, 0x02)
#define BT_MESH_MODEL_OP_GENERIC_ONOFF_SET_UNACK BT_MESH_MODEL_OP_2(0x82, 0x03)
#define BT_MESH_MODEL_OP_GENERIC_ONOFF_STATUS BT_MESH_MODEL_OP_2(0x82, 0x04)

// specifies opcodes which each model is required to be able to receive (i.e. this generic onoff client)
static const struct bt_mesh_model_op gen_onoff_cli_op[] = {
	{BT_MESH_MODEL_OP_GENERIC_ONOFF_STATUS, 1, generic_onoff_status},
	BT_MESH_MODEL_OP_END, // end of definition
};

// -------------------------------------------------------------------------------------------------------
// Light HSL Client Model
// ----------------------

/*
BLACK   : HSL(    0,    0,    0) = RGB(0,0,0)

RED     : HSL(    0,65535,32767) = RGB(255,0,0)

GREEN   : HSL(21845,65535,32767) = RGB(0,255,0)

BLUE    : HSL(43690,65535,32767) = RGB(0,0,255)

YELLOW  : HSL(10922,65535,32767) = RGB(255,255,0)

MAGENTA : HSL(54613,65535,32767) = RGB(255,0,255)

CYAN    : HSL(32768,65535,32767) = RGB(0,255,255)

WHITE   : HSL(    0,    0,65535) = RGB(255,255,255)
*/

#define NUMBER_OF_COLOURS 8

uint16_t hsl[NUMBER_OF_COLOURS][3] = {
	{ 0x0000, 0x0000, 0x0000 }, // black
	{ 0x0000, 0xFFFF, 0x7FFF }, // red 
	{ 0x5555, 0xFFFF, 0x7FFF }, // green
	{ 0xAAAA, 0xFFFF, 0x7FFF }, // blue
	{ 0x2AAA, 0xFFFF, 0x7FFF }, // yellow
	{ 0xD555, 0xFFFF, 0x7FFF }, // magenta
	{ 0x7FFF, 0xFFFF, 0x7FFF }, // cyan
	{ 0x0000, 0x0000, 0xFFFF }  // white
};

uint8_t current_hsl_inx = 1;

// message types defined by this model.
#define BT_MESH_MODEL_OP_LIGHT_HSL_SET_UNACK	BT_MESH_MODEL_OP_2(0x82, 0x77)

// -------------------------------------------------------------------------------------------------------
// Composition
// -----------
// define publication contexts (allocating messages ie net buffers)
BT_MESH_MODEL_PUB_DEFINE(gen_onoff_cli, NULL, 2); // 2 = 1+1 = sizeof(on_of_off) + sizeof(tid)
BT_MESH_MODEL_PUB_DEFINE(light_hsl_cli, NULL, 7); // 7 = 2+2+2+1 = sizeof(hue)+sizeof(sat)+sizeof(light)+sizeof(tid) - see mesh model spec section 6.3.3.2

static struct bt_mesh_model sig_models[] = {
	BT_MESH_MODEL_CFG_SRV(&cfg_srv),
	BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
	BT_MESH_MODEL(BT_MESH_MODEL_ID_GEN_ONOFF_CLI, gen_onoff_cli_op, &gen_onoff_cli, &onoff[0]),
	BT_MESH_MODEL(BT_MESH_MODEL_ID_LIGHT_HSL_CLI, NULL, &light_hsl_cli, &hsl[0]),
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

// Generic OnOff Client - TX message producer functions
// -----------------------------------------------------------
int generic_onoff_get() {
	printk("generic_onoff_get\n");
	int err;
	struct bt_mesh_model *model = &sig_models[2];
	if (model->pub->addr == BT_MESH_ADDR_UNASSIGNED) {
		printk("No publish address associated with the generic onoff client model: add one with a configuration app like nRF Mesh\n");
		return -1;
	}

	// msg was created with the BT_MESH_MODEL_PUB_DEFINE macro
	struct net_buf_simple *msg = model->pub->msg;
	bt_mesh_model_msg_init(msg, BT_MESH_MODEL_OP_GENERIC_ONOFF_GET);
	printk("Publishing get onoff message\n");
	err = bt_mesh_model_publish(model);
	if (err) {
		printk("bt_mesh_model_publish error %d", err);
	}
	return err;
}

// function used for both set ack and unack, distinguished by param msg_type
int send_gen_onoff_set(uint8_t on_or_off, uint16_t msg_type) {
	int err;
	struct bt_mesh_model *model = &sig_models[2];
	if (model->pub->addr == BT_MESH_ADDR_UNASSIGNED) {
		printk("No publish address associated with the generic onoff client model: add one with a configuration app like nRF Mesh\n");
		return -1;
	}

	struct net_buf_simple *msg = model->pub->msg;
	bt_mesh_model_msg_init(msg, msg_type);
	net_buf_simple_add_u8(msg, on_or_off);
	net_buf_simple_add_u8(msg, onoff_tid);
	onoff_tid++; 
	printk("Publishing set onoff state=0x%02x\n", on_or_off);
	err = bt_mesh_model_publish(model);
	if (err) {
		printk("bt_mesh_model_publish error %d", err);
	}
	return err;
}

void generic_onoff_set(uint8_t on_or_off) {
	if (send_gen_onoff_set(on_or_off, BT_MESH_MODEL_OP_GENERIC_ONOFF_SET)) {
		printk("Unable to send generic onoff set message\n");
	} else {
		printk("onoff set message %d sent\n", on_or_off);
	}
}

void generic_onoff_set_unack(uint8_t on_or_off) {
	if (send_gen_onoff_set(on_or_off, BT_MESH_MODEL_OP_GENERIC_ONOFF_SET_UNACK)) {
		printk("Unable to send generic onoff set unack message\n");
	} else {
		printk("onoff set unack message %d sent\n", on_or_off);
	}
}

// Light HSL Client - TX message producer functions
// -----------------------------------------------------------

int send_light_hsl_set(uint16_t msg_type) {
	int err;
	struct bt_mesh_model *model = &sig_models[3];
	if (model->pub->addr == BT_MESH_ADDR_UNASSIGNED) {
		printk("No publishing address associated with the light HSL client model - add one with a config app like nrf mesh\n");
		return -1;
	}

	struct net_buf_simple *msg = model->pub->msg;
	bt_mesh_model_msg_init(msg, msg_type);
	// ordering is lightness, hue, and saturation - see section 6.3.3.2 mesh profile specification
	net_buf_simple_add_le16(msg, hsl[current_hsl_inx][2]);
	net_buf_simple_add_le16(msg, hsl[current_hsl_inx][0]);
	net_buf_simple_add_le16(msg, hsl[current_hsl_inx][1]);
	net_buf_simple_add_u8(msg, hsl_tid);
	
	hsl_tid++;
	
	printk("publishing light HSL set message\n");
	err = bt_mesh_model_publish(model);
	if (err) {
		printk("bt_mesh_model_publish err: %d\n", err);
	} else {
		current_hsl_inx++;
		if (current_hsl_inx == NUMBER_OF_COLOURS) {
			current_hsl_inx = 0;
		}
	}
	
	return err;
}

void light_hsl_set_unack() {
	if (send_light_hsl_set(BT_MESH_MODEL_OP_LIGHT_HSL_SET_UNACK)) {
		printk("Unable to send light hsl set unack message\n");
	}
}


bool debounce() {
	bool ignore = false;
	btn_time = k_uptime_get_32();
	if (btn_time < (btn_last_time + BUTTON_DEBOUNCE_DELAY_MS)) {
		ignore = true;
	} else {
		ignore = false;
	}
	btn_last_time = btn_time;
	return ignore;
}

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {	
	if (!debounce()) {
		printk("Button pressed at %" PRIu32 "\n", k_cycle_get_32());
		
		if (op_id % 4 == 0) {
			generic_onoff_set_unack(0);
		} else if (op_id % 4 == 1) {
			generic_onoff_set_unack(1);
		} else if (op_id % 4 == 2) {
			generic_onoff_get();
		} else if (op_id % 4 == 3) {
			light_hsl_set_unack();
		}
		op_id++;
	}
}

// -------------------------------------------------------------------------------------------------------
// LED
// -------

void initialize_led(void) {
	printk("initializeLED\n");
	int ret;

	led = device_get_binding(LED0_GPIO_LABEL);

	if (led == NULL) {
		printk("Didn't find LED device %s\n", LED0_GPIO_LABEL);
		return;
	}

	ret = gpio_pin_configure(led, LED0_GPIO_PIN, GPIO_OUTPUT_ACTIVE | LED0_GPIO_FLAGS);
	if (ret != 0) {
		printk("Error %d: failed to configure LED device %s pin %d\n",
		       ret, LED0_GPIO_LABEL, LED0_GPIO_PIN);
		return;
	}

	printk("Set up LED at %s pin %d\n", LED0_GPIO_LABEL, LED0_GPIO_PIN);
	ledOff();
}

// -------------------------------------------------------------------------------------------------------
// Buttons
// -------

void configureButtons(void)
{
	int ret;
	printk("configureButtons\n");

	const struct device *button;

	button = device_get_binding(SW0_GPIO_LABEL);
	if (button == NULL) {
		printk("Error: didn't find %s device\n", SW0_GPIO_LABEL);
		return;
	}

	ret = gpio_pin_configure(button, SW0_GPIO_PIN, SW0_GPIO_FLAGS);
	if (ret != 0) {
		printk("Error %d: failed to configure %s pin %d\n",
		       ret, SW0_GPIO_LABEL, SW0_GPIO_PIN);
		return;
	}

	ret = gpio_pin_interrupt_configure(button,
					   SW0_GPIO_PIN,
					   GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, SW0_GPIO_LABEL, SW0_GPIO_PIN);
		return;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(SW0_GPIO_PIN));
	gpio_add_callback(button, &button_cb_data);
	printk("Set up button at %s pin %d\n", SW0_GPIO_LABEL, SW0_GPIO_PIN);

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


void main(void)
{
	int err;
	printk("switch yoyo\n");

	onoff_tid = 0;
	hsl_tid = 0;

	configureButtons();

	initialize_led();
	
	err = bt_enable(bt_ready);
	if (err)
	{
		printk("bt_enable failed with err %d\n", err);
	}
	

	while (1) {
		k_msleep(100);
	}
}
