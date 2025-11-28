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

/* LED mapping structure */
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
#define THRESHOLD_HIGH  700
#define THRESHOLD_LOW   300
#define NEUTRAL_MIN     400
#define NEUTRAL_MAX     600

/* Idle animation system */
#define IDLE_ANIMATION_INTERVAL_MS  150  /* Hastighet på animasjon */

static struct k_work_delayable idle_work;
static bool idle_running = false;
static uint8_t idle_step = 0;

/* Animasjonsmønster: roterer rundt (Up -> Right -> Down -> Left -> ...) */
static const LED_Name animation_pattern[] = {
    LED_Up,
    LED_Right,
    LED_Down,
    LED_Left
};
#define ANIMATION_STEPS (sizeof(animation_pattern) / sizeof(animation_pattern[0]))

/* Idle animation work handler */
static void idle_animation_handler(struct k_work *work)
{
    if (!idle_running) {
        return;
    }

    /* Slå av alle LED-er */
    led_all_off();

    /* Tenn neste LED i mønsteret */
    led_set(animation_pattern[idle_step], LED_On);

    /* Gå til neste steg */
    idle_step = (idle_step + 1) % ANIMATION_STEPS;

    /* Schedule neste steg */
    k_work_reschedule(&idle_work, K_MSEC(IDLE_ANIMATION_INTERVAL_MS));
}

int led_init(void)
{
    int ret;

    printk("Initializing LED system...\n");

    /* Configure test LED first */
    if (!gpio_is_ready_dt(&led_test)) {
        printk("Error: Test LED GPIO device not ready\n");
        return -1;
    }

    ret = gpio_pin_configure_dt(&led_test, GPIO_OUTPUT_ACTIVE);
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

    /* Initialize idle animation work */
    k_work_init_delayable(&idle_work, idle_animation_handler);

    printk("LED system initialized successfully\n");
    printk("  - LED Test:  P%d (ALWAYS ON)\n", led_test.pin);
    printk("  - LED Up:    P%d\n", led_up.pin);
    printk("  - LED Down:  P%d\n", led_down.pin);
    printk("  - LED Left:  P%d\n", led_left.pin);
    printk("  - LED Right: P%d\n", led_right.pin);
    printk("  - Idle animation: %d ms interval\n", IDLE_ANIMATION_INTERVAL_MS);

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

void led_start_idle_animation(void)
{
    if (idle_running) {
        return;  /* Allerede kjører */
    }

    printk(">>> Starting idle animation <<<\n");
    idle_running = true;
    idle_step = 0;

    /* Start animasjonen */
    k_work_reschedule(&idle_work, K_NO_WAIT);
}

void led_stop_idle_animation(void)
{
    if (!idle_running) {
        return;  /* Kjører ikke */
    }

    printk(">>> Stopping idle animation <<<\n");
    idle_running = false;

    /* Stopp work og slå av LED-er */
    k_work_cancel_delayable(&idle_work);
    led_all_off();
}