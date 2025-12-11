#include <zephyr/kernel.h>
#include "car_control.h"

int main(void)
{
    if (car_controller_init() != 0) {
        return -1;
    }

    car_controller_start();

    while (1) {
        car_controller_check_timeout();
        k_msleep(100);
    }

    return 0;
}