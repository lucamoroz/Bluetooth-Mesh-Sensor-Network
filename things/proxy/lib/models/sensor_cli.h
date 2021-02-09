#ifndef SENSOR_CLI_H
#define SENSOR_CLI_H

#include <bluetooth/mesh.h>
#include <stdio.h>
#include <sensor_server.h>

typedef void (*thp_data_cb)(float temperature, float humidity, float pressure, uint16_t recv_dest);
typedef void (*gas_data_cb)(uint16_t ppm, uint16_t recv_dest);

thp_data_cb thp_callback = NULL;
gas_data_cb gas_callback = NULL;

extern void replay_to_all(struct bt_mesh_model *mdl, struct bt_mesh_msg_ctx *original_ctx, struct net_buf_simple *buf);

void sensor_cli_set_thp_callback(thp_data_cb cb) {
    thp_callback = cb;
}

void sensor_cli_set_gas_callback(gas_data_cb cb) {
    gas_callback = cb;
}

#define BT_MESH_MODEL_OP_SENSOR_STATUS	BT_MESH_MODEL_OP_1(0x52)
#define BT_MESH_MODEL_OP_SENSOR_GET	BT_MESH_MODEL_OP_2(0x82, 0x31)

#define ID_TEMP_CELSIUS 0x2A10
#define ID_HUMIDITY		0x2A11
#define ID_PRESSURE		0x2A12

BT_MESH_MODEL_PUB_DEFINE(sensor_cli_pub, NULL, 0); // Property ID not supported

static void sensor_cli_status(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf) {
	printk("sensor_cli_status - buf len:%d\n",  buf->len);
    if (!(buf->len == 12 || buf->len == 4)) {
        printk("ignoring sensor_status message: unrecognized len\n");
        return;
    }
    
    NET_BUF_SIMPLE_DEFINE(clone, buf->len);
    net_buf_simple_clone(buf, &clone);
    sensor_srv_pub_to_all(&clone, ctx->recv_dst);

    if (buf->len == 4) {
        uint16_t gas_sensor_id = net_buf_simple_pull_le16(buf);
        uint16_t ppm = net_buf_simple_pull_le16(buf);

        printk("Sensor ID: 0x%04x\n", gas_sensor_id);
	    printk("Sensor value: %d\n", ppm);

        if (gas_callback != NULL) {
            gas_callback(ppm, ctx->recv_dst);
        } else {
            printk("Please set gas callback\n");
        }
        
    } else if (buf->len == 12) {
        
        // Sensor values are sent as unsigned integers due to zephyr problems when sending 32-bit values
        // on net_buf_simple
        uint16_t temp_sensor_id = net_buf_simple_pull_le16(buf);
        float temperature = ((float) net_buf_simple_pull_le16(buf)) / 100;
        uint16_t hum_sensor_id = net_buf_simple_pull_le16(buf);
        float humidity = ((float) net_buf_simple_pull_le16(buf)) / 100;
        uint16_t pres_sensor_id = net_buf_simple_pull_le16(buf);
        float pressure = ((float) net_buf_simple_pull_le16(buf)) / 100;

        printf("\nSensor ID: 0x%04x", temp_sensor_id);
	    printf("\nSensor value: %.2f", temperature);
        printf("\nSensor ID: 0x%04x", hum_sensor_id);
	    printf("\nSensor value: %.2f", humidity);
        printf("\nSensor ID: 0x%04x", pres_sensor_id);
	    printf("\nSensor value: %.2f\n", pressure);

        if (thp_callback != NULL) {
            thp_callback(temperature, humidity, pressure, ctx->recv_dst);
        } else {
            printk("Please set thp callback\n");
        }
        
    }
}

static const struct bt_mesh_model_op sensor_cli_op[] = {
	{ BT_MESH_MODEL_OP_SENSOR_STATUS, 4, sensor_cli_status },
	BT_MESH_MODEL_OP_END,
};

#define SENSOR_CLIENT_MODEL BT_MESH_MODEL(BT_MESH_MODEL_ID_SENSOR_CLI, sensor_cli_op, &sensor_cli_pub, NULL)

// Sensor Client - TX message producer function
void sensor_cli_get(struct bt_mesh_model *model) {
    int err;

    // for some readon this causes the error "illegal use of the epsr zephyr", therefore
    // the model is passed as parameter
	// struct bt_mesh_model model = SENSOR_CLIENT_MODEL;

	if (model->pub->addr == BT_MESH_ADDR_UNASSIGNED) {
		printk("No publishing address associated with the sensor client model - add one with a config app like nrf mesh\n");
		return;
	}

    struct net_buf_simple *msg = model->pub->msg;

	bt_mesh_model_msg_init(msg, BT_MESH_MODEL_OP_SENSOR_GET);

	printk("publishing sensor get message\n");
	err = bt_mesh_model_publish(model);
	if (err) {
		printk("bt_mesh_model_publish err: %d\n", err);
	}
}

#endif //SENSOR_CLI_H