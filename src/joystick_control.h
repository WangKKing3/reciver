#ifndef JOYSTICK_CONTROL_H
#define JOYSTICK_CONTROL_H

#include <stdint.h>

/* Tar joystick x/y og styrer motorene */
void joystick_drive(int16_t x_pos, int16_t y_pos);

#endif