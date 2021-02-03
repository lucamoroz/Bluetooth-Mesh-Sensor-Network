#ifndef GAS_SENSOR_H
#define GAS_SENSOR_H

#include <bluetooth/mesh.h>
#include "../sensors/gas_reader.h"

#define BT_MESH_MODEL_OP_SENSOR_STATUS	BT_MESH_MODEL_OP_1(0x52)
#define BT_MESH_MODEL_OP_SENSOR_GET	BT_MESH_MODEL_OP_2(0x82, 0x31)

#define ID_GAS 0x2A13

BT_MESH_MODEL_PUB_DEFINE(gas_sens_pub, NULL, 2+2);

static void gas_sensor_status(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf) {
    uint16_t ppm;
	struct net_buf_simple *msg = model->pub->msg;
	int ret;

	printk("gas_sensor_status\n");

    read_gas(&ppm);

	bt_mesh_model_msg_init(msg, BT_MESH_MODEL_OP_SENSOR_STATUS);
	net_buf_simple_add_le16(msg, ID_GAS);
	net_buf_simple_add_le16(msg, ppm);

	ret = bt_mesh_model_publish(model);
	if (ret) {
		printk("Error publishing sensor status: %d\n", ret);
		return;
	}
	printk("Sensor status published\n");
}

static const struct bt_mesh_model_op gas_sens_srv_op[] = {
	{ BT_MESH_MODEL_OP_SENSOR_GET, 0, gas_sensor_status },
	BT_MESH_MODEL_OP_END,
};

#define GAS_SENSOR_MODEL BT_MESH_MODEL(BT_MESH_MODEL_ID_SENSOR_SRV, gas_sens_srv_op, &gas_sens_pub, NULL)

void gas_sensor_publish_data(uint16_t ppm) {
    int err;
    struct bt_mesh_model model = GAS_SENSOR_MODEL;

    printk("gas_sensor_publish_data\n");

    struct net_buf_simple *msg = model.pub->msg;

    if (model.pub->addr == BT_MESH_ADDR_UNASSIGNED) {
		printk("No publish address associated with the gas sensor model! Add one with a configuration app like nrf mesh\n");
		return;
	}

    net_buf_simple_reset(msg);
	bt_mesh_model_msg_init(msg, BT_MESH_MODEL_OP_SENSOR_STATUS);

    net_buf_simple_add_le16(msg, ID_GAS);
	net_buf_simple_add_le16(msg, ppm);

    printk("publishing sensor_data\n");
	err = bt_mesh_model_publish(&model);
	if (err) {
		printk("bt_mesh_publish error: %d\n", err);
		return;
	}
}

void gas_sensor_start() {
    gas_reader_set_callback(&gas_sensor_publish_data);
}

#endif //GAS_SENSOR_H