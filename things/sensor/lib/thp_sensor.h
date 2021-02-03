#ifndef THP_SENSOR_H
#define THP_SENSOR_H

#include <bluetooth/mesh.h>
#include "thp_reader.h"

#define BT_MESH_MODEL_OP_SENSOR_STATUS	BT_MESH_MODEL_OP_1(0x52)
#define BT_MESH_MODEL_OP_SENSOR_GET	BT_MESH_MODEL_OP_2(0x82, 0x31)

#define ID_TEMP_CELSIUS 0x2A10
#define ID_HUMIDITY		0x2A11
#define ID_PRESSURE		0x2A12

/**
 * This callback will be executed right before the periodic publish step, it populates
 * the network buffer.
 * The publish period can be set using an app like nRF Mesh (e.g. model -> set publication).
 * Note that the publication will be ignored if no publish address is set!
 * */
int thp_sensor_update_cb(struct bt_mesh_model *mod) {
	printk("thp_sensor_update_cb\n");

	uint16_t temperature, humidity, pressure;
	struct net_buf_simple *msg = mod->pub->msg;

	read_thp(&temperature, &humidity, &pressure);

	bt_mesh_model_msg_init(msg, BT_MESH_MODEL_OP_SENSOR_STATUS);
	net_buf_simple_add_le16(msg, ID_TEMP_CELSIUS);
	net_buf_simple_add_le16(msg, temperature); 
	net_buf_simple_add_le16(msg, ID_HUMIDITY);
	net_buf_simple_add_le16(msg, humidity);
	net_buf_simple_add_le16(msg, ID_PRESSURE);
	net_buf_simple_add_le16(msg, pressure);
	
	return 0;
}

BT_MESH_MODEL_PUB_DEFINE(thp_sens_pub, thp_sensor_update_cb, 2+2+2+2+2+2);

static void sensor_thp_status(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf) {
	uint16_t temperature, humidity, pressure;
	struct net_buf_simple *msg = model->pub->msg;
	int ret;

	printk("sensor_thp_status\n");

	if (buf->len > 0) {
		printk("sensor_thp_status with Property ID not supported!\n");
		return;
	}

	read_thp(&temperature, &humidity, &pressure);

	bt_mesh_model_msg_init(msg, BT_MESH_MODEL_OP_SENSOR_STATUS);
	net_buf_simple_add_le16(msg, ID_TEMP_CELSIUS);
	net_buf_simple_add_le16(msg, temperature); 
	net_buf_simple_add_le16(msg, ID_HUMIDITY);
	net_buf_simple_add_le16(msg, humidity);
	net_buf_simple_add_le16(msg, ID_PRESSURE);
	net_buf_simple_add_le16(msg, pressure);

	ret = bt_mesh_model_publish(model);
	if (ret) {
		printk("Error publishing sensor status: %d\n", ret);
		return;
	}
	printk("Sensor status published\n");
}

static const struct bt_mesh_model_op thp_sens_srv_op[] = {
	{ BT_MESH_MODEL_OP_SENSOR_GET, 0, sensor_thp_status },
	BT_MESH_MODEL_OP_END,
};

#define THP_SENSOR_MODEL BT_MESH_MODEL(BT_MESH_MODEL_ID_SENSOR_SRV, thp_sens_srv_op, &thp_sens_pub, NULL)

/**
 * Can be used to force publication.
 * */
static void thp_sensor_publish_data() {
	uint16_t temperature, humidity, pressure;
	int err;
	struct bt_mesh_model model = THP_SENSOR_MODEL;

	printk("thp_sensor_publish_data\n");
	
	struct net_buf_simple *msg = model.pub->msg;

	if (model.pub->addr == BT_MESH_ADDR_UNASSIGNED) {
		printk("No publish address associated with the thp sensor model! Add one with a configuration app like nrf mesh\n");
		return;
	}

	read_thp(&temperature, &humidity, &pressure);
	
	net_buf_simple_reset(msg);
	bt_mesh_model_msg_init(msg, BT_MESH_MODEL_OP_SENSOR_STATUS);

	net_buf_simple_add_le16(msg, ID_TEMP_CELSIUS);
	net_buf_simple_add_le16(msg, temperature);

	net_buf_simple_add_le16(msg, ID_HUMIDITY);
	net_buf_simple_add_le16(msg, humidity);

	net_buf_simple_add_le16(msg, ID_PRESSURE);
	net_buf_simple_add_le16(msg, pressure);

	printk("publishing sensor_data\n");
	err = bt_mesh_model_publish(&model);
	if (err) {
		printk("bt_mesh_publish error: %d\n", err);
		return;
	}
}

#endif //THP_SENSOR_H