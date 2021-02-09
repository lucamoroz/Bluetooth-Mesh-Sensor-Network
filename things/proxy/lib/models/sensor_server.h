/**
 * Model used to forward sensor messages to the proxy client.
 * Messages with a publication address are not received by the proxy: only messages with destionation all nodes (0xFFFF) 
 * are received, therefore sensor messages are sent again with lower TTL in order to be received by the proxy client
 * (e.g. Raspberry Pi)
 */
#ifndef SENSOR_SERVER_H
#define SENSOR_SERVER_H

#include <bluetooth/mesh.h>
#include <stdio.h>

#define BT_MESH_MODEL_OP_SENSOR_STATUS	BT_MESH_MODEL_OP_1(0x52)

BT_MESH_MODEL_PUB_DEFINE(sensor_server_pub, NULL, 2+2+2+2+2+2+2);

static const struct bt_mesh_model_op sens_srv_op[] = {
	BT_MESH_MODEL_OP_END,
};

#define SENSOR_SERVER_MODEL BT_MESH_MODEL(BT_MESH_MODEL_ID_SENSOR_SRV, sens_srv_op, &sensor_server_pub, NULL)

void sensor_srv_pub_to_all(struct net_buf_simple *buf, uint16_t original_dest) {
    printk("sensor_srv_pub_to_all - buf len:%d, original pub addr: 0x%02x\n", buf->len, original_dest);;

    if (!(buf->len == 12 || buf->len == 4)) {
        printk("ignoring message replay: unrecognized len\n");
        return;
    }
    int err;
    
    struct bt_mesh_model mdl = SENSOR_SERVER_MODEL;
    struct net_buf_simple *msg = mdl.pub->msg;
    
    net_buf_simple_init(msg, BT_MESH_MODEL_OP_SENSOR_STATUS);
    
    if (buf->len == 4) {
        net_buf_simple_add_le16(msg, net_buf_simple_pull_le16(buf));
        net_buf_simple_add_le16(msg, net_buf_simple_pull_le16(buf));
    } else if (buf->len == 12) {
        net_buf_simple_add_le16(msg, net_buf_simple_pull_le16(buf));
        net_buf_simple_add_le16(msg, net_buf_simple_pull_le16(buf));
        net_buf_simple_add_le16(msg, net_buf_simple_pull_le16(buf));
        net_buf_simple_add_le16(msg, net_buf_simple_pull_le16(buf));
        net_buf_simple_add_le16(msg, net_buf_simple_pull_le16(buf));
        net_buf_simple_add_le16(msg, net_buf_simple_pull_le16(buf));
    } else {
        return;
    }

    // Append original destination so the proxy client can parse it
    printk("appending original pub addr...\n");
    net_buf_simple_add_le16(msg, original_dest);

    mdl.pub->ttl = 2;
    mdl.pub->addr = BT_MESH_ADDR_ALL_NODES;
    mdl.pub->retransmit = BT_MESH_TRANSMIT(1, 100);
    
    printk("publishing sensor server status...\n");
    err = bt_mesh_model_publish(&mdl);
    if (err) {
        printk("bt_mesh_publish error: %d\n", err);
		return;
    }
}

#endif