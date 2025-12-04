#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "BLE.h"
#include "motor_controls.h"

int main(void)
{
	int err;

	printk("\n\n===========================================\n");
	printk("BLE Motor Controller\n");
	printk("micro:bit v2 - Zephyr SDK v2.5.1\n");
	printk("===========================================\n\n");

	// Initialize motor system 
	err = motors_start();
	if (err) {
		printk("Motor init failed (err %d)\n", err);
		return -1;
	}
	printk("Motors initialized!\n");

	// Initialize BLE 
	err = ble_init();
	if (err) {
		printk("BLE init failed (err %d)\n", err);
		return -1;
	}

	err = ble_start_scan();
	if (err) {
		printk("Scan start failed (err %d)\n", err);
		return -1;
	}

	printk("\n===========================================\n");
	printk("System ready!\n");
	printk("Waiting for joystick connection...\n");
	printk("\n");
	printk("Controls:\n");
	printk("  - Button A NOT pressed: Idle demo runs\n");
	printk("  - Button A PRESSED: Joystick controls motors\n");
	printk("===========================================\n\n");

	/* Main loop - just keep running */
	while (1) {
		k_sleep(K_SECONDS(1));
	}

	return 0;
}