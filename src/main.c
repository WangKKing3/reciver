#include <zephyr/kernel.h>
#include "BLE.h"
#include "LED.h"

int main(void)
{
	int err;

	printk("\n\n===========================================\n");
	printk("Joystick Receiver with LED Direction\n");
	printk("micro:bit v2 - Zephyr SDK v2.5.1\n");
	printk("===========================================\n\n");

	/* Initialize LED system */
	err = led_init();
	if (err) {
		printk("LED init failed (err %d)\n", err);
		return -1;
	}

	/* Initialize BLE */
	err = ble_init();
	if (err) {
		printk("BLE init failed (err %d)\n", err);
		return -1;
	}

	/* Start scanning for joystick device */
	err = ble_start_scan();
	if (err) {
		printk("Scan start failed (err %d)\n", err);
		return -1;
	}

	printk("System ready!\n");
	printk("Waiting for joystick connection...\n\n");

	/* Main loop - just keep running */
	while (1) {
		k_sleep(K_SECONDS(1));
	}

	return 0;
}