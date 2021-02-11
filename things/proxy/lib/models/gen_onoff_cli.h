#ifndef GEN_ONOFF_CLI_H
#define GEN_ONOFF_CLI_H

#include <bluetooth/mesh.h>

#define BT_MESH_MODEL_OP_GENERIC_ONOFF_GET BT_MESH_MODEL_OP_2(0x82, 0x01)
#define BT_MESH_MODEL_OP_GENERIC_ONOFF_SET BT_MESH_MODEL_OP_2(0x82, 0x02)
#define BT_MESH_MODEL_OP_GENERIC_ONOFF_SET_UNACK BT_MESH_MODEL_OP_2(0x82, 0x03)
#define BT_MESH_MODEL_OP_GENERIC_ONOFF_STATUS BT_MESH_MODEL_OP_2(0x82, 0x04)

BT_MESH_MODEL_PUB_DEFINE(gen_onoff_cli, NULL, 2); // 2 = 1+1 = sizeof(on_of_off) + sizeof(tid)

uint8_t onoff[] = {0,1};
static uint8_t onoff_tid = 0;

static void generic_onoff_status(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf) {
	printk("generic_onoff_status\n");
	uint8_t onoff_state = net_buf_simple_pull_u8(buf);
	printk("generic_onoff_status onoff=%d\n", onoff_state);
}

static const struct bt_mesh_model_op gen_onoff_cli_op[] = {
	{BT_MESH_MODEL_OP_GENERIC_ONOFF_STATUS, 1, generic_onoff_status},
	BT_MESH_MODEL_OP_END, // end of definition
};

#define GEN_ONOFF_CLI_MODEL BT_MESH_MODEL(BT_MESH_MODEL_ID_GEN_ONOFF_CLI, gen_onoff_cli_op, &gen_onoff_cli, &onoff[0])

// Generic OnOff Client - TX message producer functions
// -----------------------------------------------------------
int gen_onoff_get(struct bt_mesh_model *model) {
	printk("gen_onoff_get\n");
	int err;

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
	struct bt_mesh_model model = GEN_ONOFF_CLI_MODEL;
	if (model.pub->addr == BT_MESH_ADDR_UNASSIGNED) {
		printk("No publish address associated with the generic onoff client model: add one with a configuration app like nRF Mesh\n");
		return -1;
	}

	struct net_buf_simple *msg = model.pub->msg;
	bt_mesh_model_msg_init(msg, msg_type);
	net_buf_simple_add_u8(msg, on_or_off);
	net_buf_simple_add_u8(msg, onoff_tid);
	onoff_tid++; 
	printk("Publishing set onoff state=0x%02x\n", on_or_off);
	err = bt_mesh_model_publish(&model);
	if (err) {
		printk("bt_mesh_model_publish error %d", err);
	}
	return err;
}

void gen_onoff_set(uint8_t on_or_off) {
	if (send_gen_onoff_set(on_or_off, BT_MESH_MODEL_OP_GENERIC_ONOFF_SET)) {
		printk("Unable to send generic onoff set message\n");
	} else {
		printk("onoff set message %d sent\n", on_or_off);
	}
}

void gen_onoff_set_unack(uint8_t on_or_off) {
	if (send_gen_onoff_set(on_or_off, BT_MESH_MODEL_OP_GENERIC_ONOFF_SET_UNACK)) {
		printk("Unable to send generic onoff set unack message\n");
	} else {
		printk("onoff set unack message %d sent\n", on_or_off);
	}
}

int gen_onoff_autoconf(uint16_t root_addr, uint16_t elem_addr) {
	int err;
	
	err = bt_mesh_cfg_mod_app_bind(0, root_addr, elem_addr, 0, BT_MESH_MODEL_ID_GEN_ONOFF_CLI, NULL);
	if (err) {
		printk("Error binding default app key to generic onoff client model\n");
		return err;
	}

	struct bt_mesh_cfg_mod_pub pub_gen_onoff = {
		.addr = 0xFFFF,
		.app_idx = 0,
		.ttl = 7,
		.period = 0,
		.transmit = BT_MESH_TRANSMIT(0, 0),
	};

	err = bt_mesh_cfg_mod_pub_set(0, root_addr, elem_addr, BT_MESH_MODEL_ID_GEN_ONOFF_CLI, &pub_gen_onoff, NULL);
	if (err) {
		printk("Error setting default publish config to generic onoff model\n");
		return err;
	}

	printk("Successfully configured generic onoff model\n");
	return 0;
}


#endif