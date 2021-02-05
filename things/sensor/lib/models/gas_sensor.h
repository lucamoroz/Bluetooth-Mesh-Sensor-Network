#ifndef GAS_SENSOR_H
#define GAS_SENSOR_H

#include <bluetooth/mesh.h>
#include "../sensors/ccs811.h"

typedef void (*gas_sensor_trigger_callback)(uint16_t ppm);

const struct device *ccs811;
gas_sensor_trigger_callback gas_sensor_trigger_cb = NULL;

#define BT_MESH_MODEL_OP_SENSOR_STATUS	BT_MESH_MODEL_OP_1(0x52)
#define BT_MESH_MODEL_OP_SENSOR_GET	BT_MESH_MODEL_OP_2(0x82, 0x31)

#define ID_GAS 0x2A13

BT_MESH_MODEL_PUB_DEFINE(gas_sens_pub, NULL, 2+2);

static void gas_sensor_status(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf) {
    struct sensor_value ppm_reading;
	struct net_buf_simple *msg = model->pub->msg;
	int ret;

	printk("gas_sensor_status\n");

	if (ccs811 == NULL) {
		printk("Can't read ccs811 sensor value: null ref to device\n");
		return;
	}
    if (ccs811_fetch(ccs811, &ppm_reading) < 0) {
		printk("Couldn't publish gas sensor status: error reading sensor value\n");
		return;
	}
	
	uint16_t ppm = (int) sensor_value_to_double(&ppm_reading);

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

    printk("publishing sensor_data: ppm %d\n", ppm);
	err = bt_mesh_model_publish(&model);
	if (err) {
		printk("bt_mesh_publish error: %d\n", err);
		return;
	}
}

void gas_sensor_trigger_handler(struct sensor_value *ppm_reading) {
	uint16_t ppm = (uint16_t) sensor_value_to_double(ppm_reading);
	gas_sensor_trigger_cb(ppm);
	gas_sensor_publish_data(ppm);
}

int gas_sensor_setup(int32_t trigger_threshold, gas_sensor_trigger_callback cb) {
	ccs811 = ccs811_setup(&gas_sensor_trigger_handler, trigger_threshold);
	gas_sensor_trigger_cb = cb;

	if (ccs811 == NULL) {
		printk("Couldn't setup gas_sensor: error setting device\n");
		return -1;
	}
	return 0;
}

#endif //GAS_SENSOR_H