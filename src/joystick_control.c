#include <zephyr/sys/printk.h>
#include "joystick_control.h"
#include "motor_controls.h"

#define JOY_CENTER      512
#define JOY_DEADZONE    50
#define JOY_MAX         1023

static int16_t last_y_pos = JOY_CENTER;

static int16_t last_x_pos = JOY_CENTER;

void joystick_drive(int16_t x_pos, int16_t y_pos)
{
    int32_t forward = 0;
    int32_t turn = 0;

    /* Y axis = forward/backward */
    if (y_pos > JOY_CENTER + JOY_DEADZONE) {
        forward = ((y_pos - JOY_CENTER) * 100) / (JOY_MAX - JOY_CENTER);
    } else if (y_pos < JOY_CENTER - JOY_DEADZONE) {
        forward = ((y_pos - JOY_CENTER) * 100) / JOY_CENTER;
    }

    /* X axis = turning */
    if (x_pos > JOY_CENTER + JOY_DEADZONE) {
        turn = ((x_pos - JOY_CENTER) * 100) / (JOY_MAX - JOY_CENTER);
    } else if (x_pos < JOY_CENTER - JOY_DEADZONE) {
        turn = ((x_pos - JOY_CENTER) * 100) / JOY_CENTER;
    }

    /* Ingen input - stopp */
    if (forward == 0 && turn == 0) {

        bool was_forward = last_y_pos > (JOY_CENTER + JOY_DEADZONE);
        bool going_backward = y_pos < (JOY_CENTER - JOY_DEADZONE);

       
        bool was_backward = last_y_pos < (JOY_CENTER - JOY_DEADZONE);
        bool going_forward = y_pos > (JOY_CENTER + JOY_DEADZONE);

        bool was_active = (last_y_pos > JOY_CENTER + JOY_DEADZONE) || 
                          (last_y_pos < JOY_CENTER - JOY_DEADZONE);

        if (was_active)
        {
             Stop_motors(); 
             // Legg inn en liten forsinkelse her hvis problemet vedvarer (ikke ideelt, men kan hjelpe L298N)
             // k_sleep(K_MSEC(1)); 
        }
        
        last_y_pos = y_pos;
        last_x_pos = x_pos;

        return;
    }



    /* Tank drive mixing - forward + turn */
    int32_t left = forward + turn;
    int32_t right = forward - turn;

    /* Clamp til -100 til +100 */
    if (left > 100) left = 100;
    if (left < -100) left = -100;
    if (right > 100) right = 100;
    if (right < -100) right = -100;

    /* Bestem retning og hastighet for venstre side */
    Motor_direction left_dir = Stop;
    uint32_t left_speed = 0;

    if (left > 0) {
        left_dir = Forward;
        left_speed = (uint32_t)left;
    } else if (left < 0) {
        left_dir = Backward;
        left_speed = (uint32_t)(-left);
    }

    /* Bestem retning og hastighet for høyre side */
    Motor_direction right_dir = Stop;
    uint32_t right_speed = 0;

    if (right > 0) {
        right_dir = Forward;
        right_speed = (uint32_t)right;
    } else if (right < 0) {
        right_dir = Backward;
        right_speed = (uint32_t)(-right);
    }

    /* Kjør motorene - bruker nye motor navn */
    Drive_one_motor(Motor_Left_Front, left_dir, left_speed);
    Drive_one_motor(Motor_Left_Back, left_dir, left_speed);
    Drive_one_motor(Motor_Right_Front, right_dir, right_speed);
    Drive_one_motor(Motor_Right_Back, right_dir, right_speed);

    last_y_pos = y_pos;
    last_x_pos = x_pos;
}