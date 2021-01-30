/**
 * This will worked upon to create a partial implementation of the generic on off server and light HSL server models.
 * It will not be 100% complete and therefore not compliant with the applicable specifications.
 * Provided for education purposes only.
 * 
 * Coded for and tested with Nordic Thingy
 * 
 **/

#include <stdlib.h>
#include <bluetooth/bluetooth.h>
#include <settings/settings.h>
#include <drivers/gpio.h>
#include <bluetooth/mesh.h>

// GPIO for the Thingy LED controller
struct device *led_ctrlr;

#define PORT "GPIO_P0"
#define LED_R 7
#define LED_G 5
#define LED_B 6

// states and state changes
uint8_t onoff_state;

uint16_t hsl_lightness;
uint16_t hsl_hue;
uint16_t hsl_saturation;
uint16_t rgb_r;
uint16_t rgb_g;
uint16_t rgb_b;

bool publish = false;
uint16_t reply_addr;
uint8_t reply_net_idx;
uint8_t reply_app_idx;
struct bt_mesh_model *reply_model;

// usually set by the manufacturer - hard coded here for convenience

// device UUID
static const uint8_t dev_uuid[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x01 };

void thingy_led_on(int r, int g, int b)
{
	// LEDs on Thingy are "active low" so zero means on. Args are expressed as RGB 0-255 values so we map them to GPIO low/high.
	r = !(r / 255);
	g = !(g / 255);
	b = !(b / 255);

	gpio_pin_set(led_ctrlr, LED_R, r);
	gpio_pin_set(led_ctrlr, LED_G, g);
	gpio_pin_set(led_ctrlr, LED_B, b);
}

void thingy_led_off()
{
	gpio_pin_set(led_ctrlr, LED_R, 1);
	gpio_pin_set(led_ctrlr, LED_G, 1);
	gpio_pin_set(led_ctrlr, LED_B, 1);
}

// provisioning callback functions
static void attention_on(struct bt_mesh_model *model)
{
	printk("attention_on()\n");
	thingy_led_on(255,0,0);
}

static void attention_off(struct bt_mesh_model *model)
{
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

static void provisioning_reset(void)
{
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

/*
 * The following two functions were converted from the pseudocode provided in the mesh models specification, section 6.1.1 Introduction
 */

double Hue_2_RGB(double v1, double v2, double vH ) {

	// printf("Hue_2_RGB: v1=%f v2=%f vH=%f\n",v1,v2,vH);

    if ( vH < 0.0f ) {
		vH += 1.0f;
	}
    if ( vH > 1.0f ) {
		vH -= 1.0f;
	}
    if (( 6.0f * vH ) < 1.0f ) {
		return ( v1 + ( v2 - v1 ) * 6.0f * vH );
	}
    if (( 2.0f * vH ) < 1.0f ) {
		return ( v2 );
	}
    if (( 3.0f * vH ) < 2.0f ) {
		return ( v1 + ( v2 - v1 ) * ( ( 2.0f / 3.0f ) - vH ) * 6.0f );
	}
    return ( v1 );
}	

void convert_hsl_to_rgb(unsigned short hsl_h,unsigned short hsl_s,unsigned short hsl_l ) {
	// printf("hsl_h=%d hsl_s=%d hsl_l=%d\n",hsl_h,hsl_s,hsl_l);
    double H = hsl_h / 65535.0f;
    double S = hsl_s / 65535.0f;
    double L = hsl_l / 65535.0f;
	double var_1 = 0.0f;
	double var_2 = 0.0f;
	
    if ( S == 0 ) {
      rgb_r = L * 255;
      rgb_g = L * 255;
      rgb_b = L * 255;
    } else {
      if ( L < 0.5f ) {
	      var_2 = L * ( 1.0f + S );
	  } else { 
		  var_2 = ( L + S ) - ( S * L );
	  }
      var_1 = 2.0f * L - var_2;
	  
      double R = Hue_2_RGB( var_1, var_2, H + ( 1.0f / 3.0f ));
      double G = Hue_2_RGB( var_1, var_2, H );
      double B = Hue_2_RGB( var_1, var_2, H - ( 1.0f / 3.0f ));
	  
	  // printf("R=%f G=%f B=%f\n",R,G,B);
	  
	  rgb_r = 256 * R;
	  rgb_g = 256 * G;
	  rgb_b = 256 * B;
    }
}

// generic onoff server message opcodes
#define BT_MESH_MODEL_OP_GENERIC_ONOFF_GET BT_MESH_MODEL_OP_2(0x82, 0x01)
#define BT_MESH_MODEL_OP_GENERIC_ONOFF_SET BT_MESH_MODEL_OP_2(0x82, 0x02)
#define BT_MESH_MODEL_OP_GENERIC_ONOFF_SET_UNACK BT_MESH_MODEL_OP_2(0x82, 0x03)
#define BT_MESH_MODEL_OP_GENERIC_ONOFF_STATUS BT_MESH_MODEL_OP_2(0x82, 0x04)


static void set_onoff_state(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf, bool ack) {
	uint8_t msg_onoff_state = net_buf_simple_pull_u8(buf);
	if (msg_onoff_state == onoff_state) {
		printk("ignoring set_onoff_state request: state already set\n");
		return;
	}
		
	onoff_state = msg_onoff_state;
	uint8_t tid = net_buf_simple_pull_u8(buf);
	printk("set_onoff_state: onoff=%u, TID=%u\n", onoff_state, tid);
	if (onoff_state == 0) {
		thingy_led_off();
	} else {
		thingy_led_on(rgb_r, rgb_b, rgb_g);
	}

	// See mesh profile spec - section 3.7.7.2
	if (ack) {
		generic_onoff_status(false, onoff_state);
	}

	// publish status on a state change if the server has a publish addr- see mesh profile spec section 3.7.6.1.2
	if (model->pub->addr != BT_MESH_ADDR_UNASSIGNED) {
		generic_onoff_status(true, onoff_state);
	}
}

// generic onoff server functions

static void generic_onoff_get(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf) {
    printk("generic_onoff_get\n");
	// todo remove - for interest only
	printk("ctx net_idx=0x%02x\n", ctx->net_idx);
	printk("ctx app_idx=0x%02x\n", ctx->app_idx);
	printk("ctx addr=0x%02x\n", ctx->addr);
	printk("ctx recv_dst=0x%02x\n", ctx->recv_dst);

	reply_addr = ctx->addr;
	reply_net_idx = ctx->net_idx;
	reply_app_idx = ctx->app_idx;
	generic_onoff_status(false, onoff_state);
}

static void generic_onoff_set(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf) {
    printk("generic_onoff_set\n");
	set_onoff_state(model, ctx, buf, true);
}

static void generic_onoff_set_unack(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf) {
    printk("generic_onoff_set_unack\n");
	set_onoff_state(model, ctx, buf, false);
}

static const struct bt_mesh_model_op generic_onoff_op[] = {
    {BT_MESH_MODEL_OP_GENERIC_ONOFF_GET, 0, generic_onoff_get},
    {BT_MESH_MODEL_OP_GENERIC_ONOFF_SET, 2, generic_onoff_set},
    {BT_MESH_MODEL_OP_GENERIC_ONOFF_SET_UNACK, 2, generic_onoff_set_unack},
    BT_MESH_MODEL_OP_END,
};

// generic onoff server model publication context
BT_MESH_MODEL_PUB_DEFINE(gen_onoff_pub, NULL, 2+1);

// Light HSL Server Model - minimal subset only - would not be deemed compliant
// -------------------------------------------------------------------------------------------------------

// light HSL server message opcodes
#define BT_MESH_MODEL_OP_LIGHT_HSL_SET_UNACK BT_MESH_MODEL_OP_2(0x82, 0x77)

// light HSL server functions
static void set_hsl_state(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf) {
	uint16_t msg_hsl_lightness = net_buf_simple_pull_le16(buf);
	uint16_t msg_hsl_hue = net_buf_simple_pull_le16(buf);
	uint16_t msg_hsl_saturation = net_buf_simple_pull_le16(buf);

	if (msg_hsl_lightness == hsl_lightness && msg_hsl_hue == hsl_hue && msg_hsl_saturation == hsl_saturation) {
		printk("ignoring set_hsl_state request: state already set\n");
		return;
	}

	hsl_lightness = msg_hsl_lightness;
	hsl_hue = msg_hsl_hue;
	hsl_saturation = msg_hsl_saturation;

	printk("set hsl state: lightness=%u hue=%u saturation=%u\n", hsl_lightness, hsl_hue, hsl_saturation);
	if (onoff_state == 1) {
		thingy_led_on(rgb_r, rgb_g, rgb_b);
	}

	// If a server has a publish address, it's required to publish its status after any state change 
	// see mesh profile spec section 3.7.6.1.2

	if (model->pub->addr != BT_MESH_ADDR_UNASSIGNED) {
		printk("todo: send HSL status message\n");
	}	
}

static void light_hsl_set_unack(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf) {
	printk("light_hsl_set_unack\n");
	set_hsl_state(model, ctx, buf);
}

static const struct bt_mesh_model_op light_hsl_op[] = {
	{BT_MESH_MODEL_OP_LIGHT_HSL_SET_UNACK, 7, light_hsl_set_unack},
	BT_MESH_MODEL_OP_END,
};

// light HSL server model publication context
BT_MESH_MODEL_PUB_DEFINE(light_hsl_pub, NULL, 2+6);

// -------------------------------------------------------------------------------------------------------
// Sensor server model
// -----------

#define BT_MESH_MODEL_OP_SENSOR_STATUS	BT_MESH_MODEL_OP_1(0x52)
#define BT_MESH_MODEL_OP_SENSOR_GET	BT_MESH_MODEL_OP_2(0x82, 0x31)

#define ID_TEMP_CELSIUS 0x2A1F

BT_MESH_MODEL_PUB_DEFINE(sens_pup, NULL, 2+2);

static void sensor_temp_status(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf) {
	struct net_buf_simple *msg = model->pub->msg;
	int ret;

	printk("sensor_temp_status\n");

	bt_mesh_model_msg_init(msg, BT_MESH_MODEL_OP_SENSOR_STATUS);
	net_buf_simple_add_le16(msg, ID_TEMP_CELSIUS);
	net_buf_simple_add_le16(msg, 23); // TODO use real measurement

	ret = bt_mesh_model_publish(model);
	if (ret) {
		printk("Error publishing sensor status: %d\n", ret);
		return;
	}
	printk("Sensor status published\n");
}

static const struct bt_mesh_model_op sens_temp_srv_op[] = {
	{ BT_MESH_MODEL_OP_SENSOR_GET, 2, sensor_temp_status },
	BT_MESH_MODEL_OP_END,
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

static struct bt_mesh_model sig_models[] = {
		BT_MESH_MODEL_CFG_SRV(&cfg_srv),
		BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
        BT_MESH_MODEL(BT_MESH_MODEL_ID_GEN_ONOFF_SRV, generic_onoff_op, &gen_onoff_pub, NULL),
		BT_MESH_MODEL(BT_MESH_MODEL_ID_LIGHT_HSL_SRV, light_hsl_op, &light_hsl_pub, NULL),
		BT_MESH_MODEL(BT_MESH_MODEL_ID_SENSOR_SRV, sens_temp_srv_op, &sens_pup, NULL),
};

// node contains elements.note that BT_MESH_MODEL_NONE means "none of this type" ands here means "no vendor models"
static struct bt_mesh_elem elements[] = {
		BT_MESH_ELEM(0, sig_models, BT_MESH_MODEL_NONE),
};

// node
static const struct bt_mesh_comp comp = {
		.cid = 0xFFFF,
		.elem = elements,
		.elem_count = ARRAY_SIZE(elements),
};

// ----------------------------------------------------------------------------------------------------
// generic onoff status TX message producer
void generic_onoff_status(bool publish, uint8_t on_or_off) {
	// the status message can either be published to the configured model publish address 
	// or sent to an application specified destination addr (e.g. in case of set with ack or status get)
	int err;
	struct bt_mesh_model *model = &sig_models[2];
	if (publish && model->pub->addr == BT_MESH_ADDR_UNASSIGNED) {
		printk("No publish address associated with the generic onoff model! Add one with a configuration app like nrf mesh\n");
		return;
	}

	if (publish) {
		struct net_buf_simple *msg = model->pub->msg;
		net_buf_simple_reset(msg);
		bt_mesh_model_msg_init(msg, BT_MESH_MODEL_OP_GENERIC_ONOFF_STATUS);
		net_buf_simple_add_u8(msg, on_or_off);
		printk("publishing onoff status message\n");
		err = bt_mesh_model_publish(model);
		if (err) {
			printk("bt_mesh_model_publish error: %d\n", err);
		} 
	} else {
		uint8_t buflen = 7;
		NET_BUF_SIMPLE_DEFINE(msg, buflen);
		bt_mesh_model_msg_init(&msg, BT_MESH_MODEL_OP_GENERIC_ONOFF_STATUS);
		net_buf_simple_add_u8(&msg, on_or_off);
		struct bt_mesh_msg_ctx ctx = {
			.net_idx = reply_net_idx,
			.app_idx = reply_app_idx,
			.addr = reply_addr,
			.send_ttl = BT_MESH_TTL_DEFAULT,
		};
		printk("sending onoff status message\n");
		if (bt_mesh_model_send(model, &ctx, &msg, NULL, NULL)) {
			printk("Unable to send generic onoff status message\n");
		}
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

	if (err)
	{
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

static void configure_thingy_led_controller() {
	led_ctrlr = device_get_binding(PORT);
	gpio_pin_configure(led_ctrlr, LED_R, GPIO_OUTPUT);
	gpio_pin_configure(led_ctrlr, LED_G, GPIO_OUTPUT);
	gpio_pin_configure(led_ctrlr, LED_B, GPIO_OUTPUT);
}

void indicate_on() {
	int r = 0, g = 0, b = 255;
	thingy_led_on(r, g, b);
    k_msleep(1000);
    r = 0, g = 0, b = 0;
	thingy_led_on(r, g, b);	
}

void main(void) {
	printk("thingy light node\n");

	configure_thingy_led_controller();

    indicate_on();

	// set default colour to white
	rgb_r = 255;
	rgb_g = 255;
	rgb_b = 255;

	int err = bt_enable(bt_ready);
	if (err)
	{
		printk("bt_enable failed with err %d\n", err);
	}
}
