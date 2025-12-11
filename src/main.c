#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <stdint.h>
#include "pwm.h"
#include "motor_controls.h"
#include "serial.h"
#include "gatekeeper.h"
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include "car_control.h"

int main(void)
{
    // Initialize car controller
    if (car_controller_init() != 0) {
        printk("Car controller initialization failed!\n");
        return -1;
    }

    // motors_start();
    Serial_begin(); 
    car_controller_start();

    while (1) {
        car_controller_check_timeout();
        timeout_control();
        k_msleep(50);
    };
    return 0;
}

