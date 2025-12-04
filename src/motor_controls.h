#ifndef MOTOR_CONTROLS_H
#define MOTOR_CONTROLS_H

#include <stdint.h>

typedef enum {
    Forward,
    Backward,
    Stop
}Motor_direction;

typedef enum{
    Motor_A_Front,
    Motor_A_Back,
    Motor_B_Front,
    Motor_B_Back
}Motor_name;

int motors_start();

void Drive_one_motor(Motor_name motor, Motor_direction direction, uint32_t speed_percentage);

void Drive_motors(Motor_direction direction, uint32_t speed_percentage);

void Stop_motors();

/* Idle control */
void motor_start_idle(void);
void motor_stop_idle(void);

/* Joystick control - tank drive */
void motor_drive_from_joystick(int16_t x_pos, int16_t y_pos);

#endif