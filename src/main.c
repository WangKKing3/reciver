//main.c - BLE Car Receiver with Disconnect Tolerance
// Fortsetter med siste verdier ved korte disconnect (opptil 1 sekund)

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include "BLE.h"
#include "motor_controls.h"
#include "joystick_control.h"

/* Timeout settings */
#define DATA_TIMEOUT_MS 1000      /* Stopp motorene hvis ingen data på 1 sekund */
#define MOTOR_UPDATE_MS 50        /* Oppdater motorer hver 50ms */

/* Knapper på receiver */
static const struct gpio_dt_spec button_a = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

/* Siste mottatte joystick data */
static struct joystick_data last_data = {
    .x_pos = 512,
    .y_pos = 512,
    .buttons = 0
};

/* Tidspunkt for siste mottatte data */
static int64_t last_data_time = 0;
static bool data_received = false;
static bool motors_running = false;

/* Callback når joystick data mottas fra BLE */
static void joystick_callback(const struct joystick_data *data)
{
    /* Lagre data og tidspunkt */
    last_data = *data;
    last_data_time = k_uptime_get();
    data_received = true;
    
    /* Print mottatt data */
    printk("X:%4d Y:%4d", data->x_pos, data->y_pos);
    
    if (data->buttons & BTN_A_MASK) {
        printk(" [A] DRIVE");
    }
    
    printk("\n");
}

/* Oppdater motorer basert på joystick data */
static void update_motors(const struct joystick_data *data, bool force_stop)
{
    if (force_stop || !(data->buttons & BTN_A_MASK)) {
        /* Stopp motorene */
        if (motors_running) {
            Stop_motors();
            motors_running = false;
        }
    } else {
        /* Kjør motorene med joystick data */
        joystick_drive(data->x_pos, data->y_pos);
        motors_running = true;
    }
}

int main(void)
{
    int err;
    
    printk("\n===========================================\n");
    printk("BLE Car Receiver v2 (Disconnect Tolerant)\n");
    printk("Data timeout: %d ms\n", DATA_TIMEOUT_MS);
    printk("===========================================\n\n");

    /* Initialiser GPIO for knapp */
    if (device_is_ready(button_a.port)) {
        gpio_pin_configure_dt(&button_a, GPIO_INPUT | GPIO_PULL_UP);
        printk("Button A ready\n");
    }

    /* Initialiser motorer */
    err = motors_start();
    if (err) {
        printk("Motor init failed: %d\n", err);
        return -1;
    }
    printk("Motors ready\n");

    /* Initialiser BLE med callback */
    err = ble_init(joystick_callback);
    if (err) {
        printk("BLE init failed: %d\n", err);
        return -1;
    }

    /* Start scanning */
    printk("Starting BLE scan...\n");
    ble_start_scan();

    /* Main loop - motor oppdatering */
    while (1) {
        int64_t now = k_uptime_get();
        int64_t data_age = now - last_data_time;
        
        if (data_received) {
            if (data_age < DATA_TIMEOUT_MS) {
                /* Data er fersk nok - bruk det */
                update_motors(&last_data, false);
            } else {
                /* Data er for gammelt - stopp motorene */
                if (motors_running) {
                    printk("Data timeout (%lld ms) - stopping motors\n", data_age);
                }
                update_motors(&last_data, true);
            }
        }
        
        /* Sjekk lokal knapp A som override */
        if (gpio_pin_get_dt(&button_a) == 0) {
            /* Lokal knapp holdt - aktiver motorer med siste data */
            if (data_received && data_age < DATA_TIMEOUT_MS) {
                /* Tving A-knapp aktiv */
                struct joystick_data override = last_data;
                override.buttons |= BTN_A_MASK;
                update_motors(&override, false);
            }
        }
        
        k_msleep(MOTOR_UPDATE_MS);
    }

    return 0;
}