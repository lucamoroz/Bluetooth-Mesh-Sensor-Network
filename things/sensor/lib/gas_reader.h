#ifndef GAS_READER_H
#define GAS_READER_H

#include <kernel.h>

void (*callback)(uint16_t ppm) = NULL;

// TODO remove me after implementing trigger logic
extern void gas_sensor_data_kwork(struct k_work *work);
extern void gas_sensor_timer_handler(struct k_timer *dummy);

K_WORK_DEFINE(gas_sensor_data_work, gas_sensor_data_kwork);
K_TIMER_DEFINE(gas_sensor_pub_timer, gas_sensor_timer_handler, NULL);
// STOP REMOVE

void read_gas(uint16_t *ppm) {
    *ppm = 666;
}

void gas_reader_set_callback(void (*cb)(uint16_t ppm)) {
    callback = cb;

    // TODO remove me - just for testing
    printk("starting fake trigger");
	k_timer_start(&gas_sensor_pub_timer, K_SECONDS(4), K_SECONDS(10));
}

// TODO implement sensor logic
static void ccs811_trigger_handler() {

    uint16_t ppm = 666;

    if (callback != NULL) {
        (*callback)(ppm);
    }
}

// TODO remove everything below - used to simulate trigger logic
void gas_sensor_data_kwork(struct k_work *work) {
    ccs811_trigger_handler();
}

void gas_sensor_timer_handler(struct k_timer *dummy) {
    k_work_submit(&gas_sensor_data_work);
}

#endif //GAS_READER_H