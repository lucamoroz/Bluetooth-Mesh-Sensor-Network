#ifndef GENERIC_ONOFF_H
#define GENERIC_ONOFF_H

#include <bluetooth/mesh.h>
#include <drivers/gpio.h>

#define PORT "GPIO_P0"
#define LED_R 7
#define LED_G 5
#define LED_B 6

// states and state changes
uint8_t onoff_state;
// GPIO for the Thingy LED controller
const struct device *led_ctrlr;

uint16_t rgb_r = 255;
uint16_t rgb_g = 255;
uint16_t rgb_b = 255;

bool publish = false;
uint16_t reply_addr;
uint8_t reply_net_idx;
uint8_t reply_app_idx;
struct bt_mesh_model *reply_model;

extern void generic_onoff_status(bool publish, uint8_t on_or_off);

void thingy_led_on(int r, int g, int b) {
	// LEDs on Thingy are "active low" so zero means on. Args are expressed as RGB 0-255 values so we map them to GPIO low/high.
	r = !(r / 255);
	g = !(g / 255);
	b = !(b / 255);

	gpio_pin_set(led_ctrlr, LED_R, r);
	gpio_pin_set(led_ctrlr, LED_G, g);
	gpio_pin_set(led_ctrlr, LED_B, b);
}

void thingy_led_off() {
	gpio_pin_set(led_ctrlr, LED_R, 1);
	gpio_pin_set(led_ctrlr, LED_G, 1);
	gpio_pin_set(led_ctrlr, LED_B, 1);
}

#define BT_MESH_MODEL_OP_GENERIC_ONOFF_GET BT_MESH_MODEL_OP_2(0x82, 0x01)
#define BT_MESH_MODEL_OP_GENERIC_ONOFF_SET BT_MESH_MODEL_OP_2(0x82, 0x02)
#define BT_MESH_MODEL_OP_GENERIC_ONOFF_SET_UNACK BT_MESH_MODEL_OP_2(0x82, 0x03)
#define BT_MESH_MODEL_OP_GENERIC_ONOFF_STATUS BT_MESH_MODEL_OP_2(0x82, 0x04)

// generic onoff server model publication context
BT_MESH_MODEL_PUB_DEFINE(gen_onoff_pub, NULL, 2+1);

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
	printk("ctx recv_dst=0x%02x\n", ctx->recv_dst);  // Note: this is the Publish Address

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

#define GENERIC_ONOFF_MODEL BT_MESH_MODEL(BT_MESH_MODEL_ID_GEN_ONOFF_SRV, generic_onoff_op, &gen_onoff_pub, NULL)

// generic onoff status TX message producer
void generic_onoff_status(bool publish, uint8_t on_or_off) {
	// the status message can either be published to the configured model publish address 
	// or sent to an application specified destination addr (e.g. in case of set with ack or status get)
	int err;
	struct bt_mesh_model model = GENERIC_ONOFF_MODEL;
	if (publish && model.pub->addr == BT_MESH_ADDR_UNASSIGNED) {
		printk("No publish address associated with the generic onoff model! Add one with a configuration app like nrf mesh\n");
		return;
	}

	if (publish) {
		struct net_buf_simple *msg = model.pub->msg;
		net_buf_simple_reset(msg);
		bt_mesh_model_msg_init(msg, BT_MESH_MODEL_OP_GENERIC_ONOFF_STATUS);
		net_buf_simple_add_u8(msg, on_or_off);
		printk("publishing onoff status message\n");
		err = bt_mesh_model_publish(&model);
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
		if (bt_mesh_model_send(&model, &ctx, &msg, NULL, NULL)) {
			printk("Unable to send generic onoff status message\n");
		}
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

void generic_onoff_setup() {
	configure_thingy_led_controller();
	indicate_on();
}

#endif //GENERIC_ONOFF_H