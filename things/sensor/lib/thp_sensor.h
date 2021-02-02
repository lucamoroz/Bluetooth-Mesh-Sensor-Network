#ifndef THP_SENSOR_H
#define THP_SENSOR_H

#include <bluetooth/mesh.h>
#include "thp_reader.h"

#define BT_MESH_MODEL_OP_SENSOR_STATUS	BT_MESH_MODEL_OP_1(0x52)
#define BT_MESH_MODEL_OP_SENSOR_GET	BT_MESH_MODEL_OP_2(0x82, 0x31)

#define ID_TEMP_CELSIUS 0x2A10
#define ID_HUMIDITY		0x2A11
#define ID_PRESSURE		0x2A12

// Timer and work required to periodically publish sensor data
extern void thp_sensor_data_kwork(struct k_work *work);
extern void thp_sensor_timer_handler(struct k_timer *dummy);

K_WORK_DEFINE(thp_sensor_data_work, thp_sensor_data_kwork);
K_TIMER_DEFINE(thp_sensor_pub_timer, thp_sensor_timer_handler, NULL);

#define PUBLISH_INTERVAL 60


BT_MESH_MODEL_PUB_DEFINE(thp_sens_pub, NULL, 2+2+2+2+2+2);

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

void thp_sensor_data_kwork(struct k_work *work) {
	thp_sensor_publish_data();
}

void thp_sensor_timer_handler(struct k_timer *dummy) {
	k_work_submit(&thp_sensor_data_work);
}

void thp_sensor_start() {
	printk("starting thp_sensor");
	k_timer_start(&thp_sensor_pub_timer, K_SECONDS(4), K_SECONDS(PUBLISH_INTERVAL));
}


#endif //THP_SENSOR_H