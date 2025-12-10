#ifndef BLE_H
#define BLE_H

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <stdbool.h>

struct joystick_data {
	int16_t x_pos;
	int16_t y_pos;
	uint8_t buttons;
} __packed;

#define BTN_A_MASK (1 << 0)
#define BTN_B_MASK (1 << 1)

typedef void (*joystick_data_callback_t)(const struct joystick_data *data);

int ble_init(joystick_data_callback_t cb);
int ble_start_scan(void);
bool ble_is_connected(void);

#endif