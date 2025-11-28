#ifndef BLE_H
#define BLE_H

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

/* Joystick data structure */
struct joystick_data {
	int16_t x_pos;
	int16_t y_pos;
} __packed;

/**
 * @brief Initialize BLE system
 * @return 0 on success, negative error code on failure
 */
int ble_init(void);

/**
 * @brief Start scanning for joystick devices
 * @return 0 on success, negative error code on failure
 */
int ble_start_scan(void);

#endif /* BLE_H */