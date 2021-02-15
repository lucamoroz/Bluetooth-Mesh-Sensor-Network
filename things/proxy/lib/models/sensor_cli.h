/**
 * Sensor client model.
 * The model can query the status of generic onoff server models and receive status updates.
 * This model supports THP and gas status messages.
 * Include SENSOR_CLIENT_MODEL in an element.
 * The model publication context can be auto-configured with sensor_cli_autoconf().
 */

#ifndef SENSOR_CLI_H
#define SENSOR_CLI_H

#include <bluetooth/mesh.h>
#include <stdio.h>

/* Callback to handle a THP status message */
typedef void (*thp_data_cb)(float temperature, float humidity, float pressure, uint16_t node_addr);
/* Callback to handle a gas status message */
typedef void (*gas_data_cb)(uint16_t ppm, uint16_t node_addr);

thp_data_cb thp_callback = NULL;
gas_data_cb gas_callback = NULL;

void sensor_cli_set_thp_callback(thp_data_cb cb) {
    thp_callback = cb;
}

void sensor_cli_set_gas_callback(gas_data_cb cb) {
    gas_callback = cb;
}

#define BT_MESH_MODEL_OP_SENSOR_STATUS	BT_MESH_MODEL_OP_1(0x52)
#define BT_MESH_MODEL_OP_SENSOR_GET	BT_MESH_MODEL_OP_2(0x82, 0x31)

/* IDs identifying sensor reading types */
#define ID_TEMP_CELSIUS 0x2A10
#define ID_HUMIDITY		0x2A11
#define ID_PRESSURE		0x2A12
#define ID_GAS 0x2A13

/* Sensor publication context used to send status-get messages */
BT_MESH_MODEL_PUB_DEFINE(sensor_cli_pub, NULL, 0); // Property ID not supported

/* Handle sensor status messages. Messages can either be THP messages (12 bytes) or gas messages (4 bytes) */
static void sensor_cli_status(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx, struct net_buf_simple *buf) {
	printk("sensor_cli_status - buf len:%d\n",  buf->len);

    if (buf->len == 4) {
        uint16_t gas_sensor_id = net_buf_simple_pull_le16(buf);
		if (gas_sensor_id != ID_GAS) {
			printk("Ignoring gas sensor status message: unrecognized gas sensor ID\n");
			return;
		}
        uint16_t ppm = net_buf_simple_pull_le16(buf);

        printf("Sensor ID: 0x%04x\n", gas_sensor_id);
	    printf("Sensor value: %d\n", ppm);

        if (gas_callback != NULL) {
            gas_callback(ppm, ctx->addr);
        } else {
            printk("Please set gas callback\n");
        }
        
    } else if (buf->len == 12) {
        
        // Sensor values are sent as unsigned integers due to zephyr problems when sending 32-bit values
        // on net_buf_simple
        uint16_t temp_sensor_id = net_buf_simple_pull_le16(buf);
		if (temp_sensor_id != ID_TEMP_CELSIUS) {
			printk("Ignoring THP sensor status message: unrecognized temperature sensor ID\n");
			return;
		}
        float temperature = ((float) net_buf_simple_pull_le16(buf)) / 100;

        uint16_t hum_sensor_id = net_buf_simple_pull_le16(buf);
		if (hum_sensor_id != ID_HUMIDITY) {
			printk("Ignoring THP sensor status message: unrecognized humidity sensor ID\n");
			return;
		}
        float humidity = ((float) net_buf_simple_pull_le16(buf)) / 100;

        uint16_t pres_sensor_id = net_buf_simple_pull_le16(buf);
		if (pres_sensor_id != ID_PRESSURE) {
			printk("Ignoring THP sensor status message: unrecognized pressure sensor ID\n");
			return;
		}
        float pressure = ((float) net_buf_simple_pull_le16(buf)) / 100;

        printf("\nSensor ID: 0x%04x", temp_sensor_id);
	    printf("\nSensor value: %.2f", temperature);
        printf("\nSensor ID: 0x%04x", hum_sensor_id);
	    printf("\nSensor value: %.2f", humidity);
        printf("\nSensor ID: 0x%04x", pres_sensor_id);
	    printf("\nSensor value: %.2f\n", pressure);

        if (thp_callback != NULL) {
            thp_callback(temperature, humidity, pressure, ctx->addr);
        } else {
            printk("Please set thp callback\n");
        }
        
    } else {
        printk("ignoring sensor_status message: unrecognized len\n");
        return;
    }
}

/* Opcodes supported by this model */
static const struct bt_mesh_model_op sensor_cli_op[] = {
	{ BT_MESH_MODEL_OP_SENSOR_STATUS, 4, sensor_cli_status },
	BT_MESH_MODEL_OP_END,
};

#define SENSOR_CLIENT_MODEL BT_MESH_MODEL(BT_MESH_MODEL_ID_SENSOR_CLI, sensor_cli_op, &sensor_cli_pub, NULL)

/* Sensor client get message used to requests sensors status */
void sensor_cli_get(struct bt_mesh_model *model) {
    int err;

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

/**
 * Can be used to self-configure the publishing parameters after provisioning.
 * @param root_addr id of the node hosting this model
 * @param elem_addr id of the element in the node hosting this model
 */
int sensor_cli_autoconf(uint16_t root_addr, uint16_t elem_addr) {
	int err;
	
	err = bt_mesh_cfg_mod_app_bind(0, root_addr, elem_addr, 0, BT_MESH_MODEL_ID_SENSOR_CLI, NULL);
	if (err) {
		printk("Error binding default app key to sensor client model\n");
		return err;
	}

	struct bt_mesh_cfg_mod_pub pub_sens_cli = {
		.addr = 0xFFFF,
		.app_idx = 0,
		.ttl = 7,
		.period = 0,
		.transmit = BT_MESH_TRANSMIT(0, 0),
	};

	err = bt_mesh_cfg_mod_pub_set(0, root_addr, elem_addr, BT_MESH_MODEL_ID_SENSOR_CLI, &pub_sens_cli, NULL);
	if (err) {
		printk("Error setting default publish config to sensor client model\n");
		return err;
	}

	printk("Successfully configured sensor client model\n");
	return 0;
}

#endif //SENSOR_CLI_H