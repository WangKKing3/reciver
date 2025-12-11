cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(reciver)

target_sources(app PRIVATE
    src/main.c
    src/receiver_ble.c
    src/motor_controls.c
    src/pwm.c
    src/car_control.c
)