#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <stdint.h>

#include "LED.h"

/* GPIO specs from devicetree */
static const struct gpio_dt_spec led_test = GPIO_DT_SPEC_GET(DT_NODELABEL(led_test), gpios);
static const struct gpio_dt_spec led_up = GPIO_DT_SPEC_GET(DT_NODELABEL(led_up), gpios);
static const struct gpio_dt_spec led_down = GPIO_DT_SPEC_GET(DT_NODELABEL(led_down), gpios);
static const struct gpio_dt_spec led_left = GPIO_DT_SPEC_GET(DT_NODELABEL(led_left), gpios);
static const struct gpio_dt_spec led_right = GPIO_DT_SPEC_GET(DT_NODELABEL(led_right), gpios);

/* LED mapping structure - samme stil som motor_channel_t */
typedef struct {
    const struct gpio_dt_spec *spec;
    const char *label;
} led_channel_t;

static const led_channel_t leds[] = {
    {&led_up,    "LED Up"},
    {&led_down,  "LED Down"},
    {&led_left,  "LED Left"},
    {&led_right, "LED Right"}
};

/* Joystick thresholds */
#define THRESHOLD_HIGH  700   /* Over denne = aktiv retning */
#define THRESHOLD_LOW   300   /* Under denne = aktiv retning */
#define NEUTRAL_MIN     400   /* Nøytral sone start */
#define NEUTRAL_MAX     600   /* Nøytral sone slutt */

int led_init(void)
{
    int ret;

    printk("Initializing LED system...\n");

    /* Configure test LED first */
    if (!gpio_is_ready_dt(&led_test)) {
        printk("Error: Test LED GPIO device not ready\n");
        return -1;
    }

    ret = gpio_pin_configure_dt(&led_test, GPIO_OUTPUT_ACTIVE);  /* Start ON */
    if (ret < 0) {
        printk("Error: Failed to configure test LED (err %d)\n", ret);
        return ret;
    }
    
    printk("Test LED (P1) initialized and turned ON\n");

    /* Configure all direction LED pins as output */
    for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
        if (!gpio_is_ready_dt(leds[i].spec)) {
            printk("Error: LED GPIO device not ready for %s\n", leds[i].label);
            return -1;
        }

        ret = gpio_pin_configure_dt(leds[i].spec, GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            printk("Error: Failed to configure %s (err %d)\n", leds[i].label, ret);
            return ret;
        }
    }

    printk("LED system initialized successfully\n");
    printk("  - LED Test:  P%d (ALWAYS ON)\n", led_test.pin);
    printk("  - LED Up:    P%d\n", led_up.pin);
    printk("  - LED Down:  P%d\n", led_down.pin);
    printk("  - LED Left:  P%d\n", led_left.pin);
    printk("  - LED Right: P%d\n", led_right.pin);

    return 0;
}

void led_set(LED_Name led, LED_State state)
{
    if (led >= ARRAY_SIZE(leds)) {
        printk("Error: Invalid LED selection.\n");
        return;
    }

    const led_channel_t *selected_led = &leds[led];
    int value = (state == LED_On) ? 1 : 0;

    gpio_pin_set_dt(selected_led->spec, value);
}

void led_all_off(void)
{
    for (size_t i = 0; i < ARRAY_SIZE(leds); i++) {
        gpio_pin_set_dt(leds[i].spec, 0);
    }
    /* Test LED forblir på */
}

void led_update_direction(int16_t x_pos, int16_t y_pos)
{
    /* Turn off all LEDs first */
    led_all_off();

    /* Check Y-axis (UP/DOWN) */
    if (y_pos > THRESHOLD_HIGH) {
        led_set(LED_Up, LED_On);
        printk("Direction: UP (Y=%d)\n", y_pos);
    } else if (y_pos < THRESHOLD_LOW) {
        led_set(LED_Down, LED_On);
        printk("Direction: DOWN (Y=%d)\n", y_pos);
    }

    /* Check X-axis (LEFT/RIGHT) */
    if (x_pos > THRESHOLD_HIGH) {
        led_set(LED_Right, LED_On);
        printk("Direction: RIGHT (X=%d)\n", x_pos);
    } else if (x_pos < THRESHOLD_LOW) {
        led_set(LED_Left, LED_On);
        printk("Direction: LEFT (X=%d)\n", x_pos);
    }

    /* If in neutral zone, no LED lights up */
    if (x_pos >= NEUTRAL_MIN && x_pos <= NEUTRAL_MAX &&
        y_pos >= NEUTRAL_MIN && y_pos <= NEUTRAL_MAX) {
        printk("Direction: NEUTRAL\n");
    }
}