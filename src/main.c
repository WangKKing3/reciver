#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "BLE.h"
#include "motor_controls.h"
#include "joystick_control.h"

/* Callback fra BLE når joystick data mottas */
static void joystick_callback(const struct joystick_data *data)
{
	bool btn_a = (data->buttons & BTN_A_MASK) != 0;

	printk("X:%4d Y:%4d %s\n", data->x_pos, data->y_pos, btn_a ? "[A] DRIVE" : "");


	if (btn_a) {
		joystick_drive(data->x_pos, data->y_pos);
	} else {
		Stop_motors();
	}
}

int main(void)
{
	int err;

	printk("=== BLE Car ===\n");

	err = motors_start();
	if (err) {
		printk("Motor init failed\n");
		return -1;
	}

	err = ble_init(joystick_callback);
	if (err) {
		printk("BLE init failed\n");
		return -1;
	}

	err = ble_start_scan();
	if (err) {
		printk("Scan failed\n");
		return -1;
	}

	printk("Hold A to drive\n");

	while (1) {
		k_sleep(K_SECONDS(1));
	}

	return 0;
}