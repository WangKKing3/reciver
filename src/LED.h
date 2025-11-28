#ifndef LED_H
#define LED_H

#include <stdint.h>

typedef enum {
    LED_Up,
    LED_Down,
    LED_Left,
    LED_Right
} LED_Name;

typedef enum {
    LED_Off,
    LED_On
} LED_State;

/**
 * @brief Initialize LED system
 * @return 0 on success, negative on error
 */
int led_init(void);

/**
 * @brief Control a single LED
 * @param led LED to control
 * @param state LED_On or LED_Off
 */
void led_set(LED_Name led, LED_State state);

/**
 * @brief Turn off all LEDs
 */
void led_all_off(void);

/**
 * @brief Update LEDs based on joystick position
 * @param x_pos X position from joystick (0-1023)
 * @param y_pos Y position from joystick (0-1023)
 */
void led_update_direction(int16_t x_pos, int16_t y_pos);

/**
 * @brief Start idle animation (rotating pattern)
 */
void led_start_idle_animation(void);

/**
 * @brief Stop idle animation
 */
void led_stop_idle_animation(void);

#endif /* LED_H */