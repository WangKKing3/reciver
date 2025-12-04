#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <stdint.h>

#include "pwm.h"
#include "motor_controls.h"

static const struct gpio_dt_spec motor_a_in1 = GPIO_DT_SPEC_GET(DT_NODELABEL(motor_a_in1), gpios);
static const struct gpio_dt_spec motor_a_in2 = GPIO_DT_SPEC_GET(DT_NODELABEL(motor_a_in2), gpios);
static const struct gpio_dt_spec motor_a_in3 = GPIO_DT_SPEC_GET(DT_NODELABEL(motor_a_in3), gpios);
static const struct gpio_dt_spec motor_a_in4 = GPIO_DT_SPEC_GET(DT_NODELABEL(motor_a_in4), gpios);

static const struct gpio_dt_spec motor_b_in1 = GPIO_DT_SPEC_GET(DT_NODELABEL(motor_b_in1), gpios);
static const struct gpio_dt_spec motor_b_in2 = GPIO_DT_SPEC_GET(DT_NODELABEL(motor_b_in2), gpios);
static const struct gpio_dt_spec motor_b_in3 = GPIO_DT_SPEC_GET(DT_NODELABEL(motor_b_in3), gpios);
static const struct gpio_dt_spec motor_b_in4 = GPIO_DT_SPEC_GET(DT_NODELABEL(motor_b_in4), gpios);

typedef struct {
    const struct gpio_dt_spec *in1;
    const struct gpio_dt_spec *in2;
    int (*pwm_write)(uint32_t value);
} motor_channel_t;

static const motor_channel_t motors[] = {
    {&motor_a_in1, &motor_a_in2, pwm_write_front_a},
    {&motor_a_in3, &motor_a_in4, pwm_write_front_b},
    {&motor_b_in1, &motor_b_in2, pwm_write_back_a},
    {&motor_b_in3, &motor_b_in4, pwm_write_back_b}
};

#define IDLE_STEP_DURATION_MS  2000

static struct k_work_delayable idle_work;
static bool idle_running = false;
static uint8_t idle_step = 0;

// Idle sequence steps 
typedef struct {
    Motor_direction direction;
    uint32_t speed;
    const char *description;
} idle_step_t;

// Define idle sequence
static const idle_step_t idle_sequence[] = {
    {Forward,  100, "Forward 100%"},
    {Stop,     0,   "Stop"},
    {Backward, 100, "Backward 100%"},
    {Stop,     0,   "Stop"},
    {Forward,  50,  "Forward 50%"},
    {Stop,     0,   "Stop"},
    {Backward, 50,  "Backward 50%"},
    {Stop,     0,   "Stop"}
};
#define IDLE_STEPS (sizeof(idle_sequence) / sizeof(idle_sequence[0]))

static uint32_t percentage_to_pwm(uint32_t speed_percentage){
    if(speed_percentage > 100){
        speed_percentage = 100;
    }
    return (speed_percentage * pwm_period_ns) / 100;
}

// Idle work handler 
static void idle_handler(struct k_work *work)
{
    if (!idle_running) {
        return;
    }

    const idle_step_t *step = &idle_sequence[idle_step];
    
    printk("Idle: %s\n", step->description);
    
    if (step->direction == Stop) {
        Stop_motors();
    } else {
        Drive_motors(step->direction, step->speed);
    }

    idle_step = (idle_step + 1) % IDLE_STEPS;

    k_work_reschedule(&idle_work, K_MSEC(IDLE_STEP_DURATION_MS));
}

int motors_start(){
    const struct gpio_dt_spec *gpio_pins[] = {
        &motor_a_in1, &motor_a_in2,
        &motor_a_in3, &motor_a_in4,
        &motor_b_in1, &motor_b_in2,
        &motor_b_in3, &motor_b_in4

    };
    int check = 0;

    for(size_t i = 0; i < ARRAY_SIZE(gpio_pins); i++){
        if (!gpio_is_ready_dt(gpio_pins[i])){
            printk("Error: One or more GPIO devices not ready.\n");
            return -1;
        }
        int check = gpio_pin_configure_dt(gpio_pins[i], GPIO_OUTPUT_INACTIVE);
        if(check < 0){
            printk("Error: Failed to configure GPIO pin %d.\n", i);
            return -1;
        }
    }

    if(pwm_begin() != 0){
        return -1;
    }

    /* Initialize idle work */
    k_work_init_delayable(&idle_work, idle_handler);
    
    printk("Motor system initialized\n");
    return 0;
}

/*###################################################################################*/
// Oske motor control functions
void Drive_one_motor(Motor_name motor, Motor_direction direction, uint32_t speed_percentage){
    if(motor >= ARRAY_SIZE(motors)){
        printk("Error: Invalid motor selection.\n");
        return; 
    }
    const motor_channel_t *selected_motor = &motors[motor];
    uint32_t pwm_value = percentage_to_pwm(speed_percentage);

    switch (direction){
        case Forward:
            gpio_pin_set_dt(selected_motor->in1, 1);
            gpio_pin_set_dt(selected_motor->in2, 0);
            break;
        case Backward:
            gpio_pin_set_dt(selected_motor->in1, 0);
            gpio_pin_set_dt(selected_motor->in2, 1);
            break;
        case Stop:
            gpio_pin_set_dt(selected_motor->in1, 0);
            gpio_pin_set_dt(selected_motor->in2, 0);
            pwm_value = 0;
            break;
        default:
            gpio_pin_set_dt(selected_motor->in1, 0);
            gpio_pin_set_dt(selected_motor->in2, 0);
            pwm_value = 0;
            break;
    }
    if(selected_motor->pwm_write(pwm_value) != 0){
    printk("Error: Failed to set PWM for motor %d.\n", motor);
    }
}

void Drive_motors(Motor_direction direction, uint32_t speed_percentage){
    if(direction == Stop){
        Stop_motors();
        return;
    }

    Drive_one_motor(Motor_A_Front, direction, speed_percentage);
    Drive_one_motor(Motor_A_Back, direction, speed_percentage);
    Drive_one_motor(Motor_B_Front, direction, speed_percentage);
    Drive_one_motor(Motor_B_Back, direction, speed_percentage);
}

void Stop_motors(){
    Drive_one_motor(Motor_A_Front, Stop, 0);
    Drive_one_motor(Motor_A_Back, Stop, 0);
    Drive_one_motor(Motor_B_Front, Stop, 0);
    Drive_one_motor(Motor_B_Back, Stop, 0);
}
/*###################################################################################*/

/* ========================================
 * Idle 
 * ======================================== */

void motor_start_idle(void)
{
    if (idle_running) {
        return;  /* Already running */
    }

    printk(">>> Starting idle <<<\n");
    idle_running = true;
    idle_step = 0;

    /* Start idle */
    k_work_reschedule(&idle_work, K_NO_WAIT);
}

void motor_stop_idle(void)
{
    if (!idle_running) {
        return;  // Not running
    }

    printk(">>> Stopping idle <<<\n");
    idle_running = false;

    // Stop work and motors
    k_work_cancel_delayable(&idle_work);
    Stop_motors();
}



// Joystick thresholds
#define JOY_CENTER      512
#define JOY_DEADZONE    50
#define JOY_MAX         1023

void motor_drive_from_joystick(int16_t x_pos, int16_t y_pos)
{
    //konverterer til -100 til +100

    int32_t forward = 0;  // -100 til +100
    int32_t turn = 0;     // -100 til +100
    
    // Calculate forward/backward from Y axis 
    if (y_pos > JOY_CENTER + JOY_DEADZONE) {
        forward = ((y_pos - JOY_CENTER) * 100) / (JOY_MAX - JOY_CENTER);
    } else if (y_pos < JOY_CENTER - JOY_DEADZONE) {
        forward = ((y_pos - JOY_CENTER) * 100) / JOY_CENTER;
    }
    
    // Calculate turn from X axis
    if (x_pos > JOY_CENTER + JOY_DEADZONE) {
        turn = ((x_pos - JOY_CENTER) * 100) / (JOY_MAX - JOY_CENTER);
    } else if (x_pos < JOY_CENTER - JOY_DEADZONE) {
        turn = ((x_pos - JOY_CENTER) * 100) / JOY_CENTER;
    }
    
   
    int32_t left_speed = forward + turn;
    int32_t right_speed = forward - turn;
    
    // Clamp to -100 to +100
    if (left_speed > 100) left_speed = 100;
    if (left_speed < -100) left_speed = -100;
    if (right_speed > 100) right_speed = 100;
    if (right_speed < -100) right_speed = -100;
    
    // Determine direction and absolute speed for each side
    Motor_direction left_dir = Stop;
    Motor_direction right_dir = Stop;
    uint32_t left_pwm = 0;
    uint32_t right_pwm = 0;
    
    if (left_speed > 0) {
        left_dir = Forward;
        left_pwm = (uint32_t)left_speed;
    } else if (left_speed < 0) {
        left_dir = Backward;
        left_pwm = (uint32_t)(-left_speed);
    }
    
    if (right_speed > 0) {
        right_dir = Forward;
        right_pwm = (uint32_t)right_speed;
    } else if (right_speed < 0) {
        right_dir = Backward;
        right_pwm = (uint32_t)(-right_speed);
    }
    
    Drive_one_motor(Motor_A_Front, left_dir, left_pwm);
    Drive_one_motor(Motor_A_Back, left_dir, left_pwm);
    Drive_one_motor(Motor_B_Front, right_dir, right_pwm);
    Drive_one_motor(Motor_B_Back, right_dir, right_pwm);

    if (forward != 0 || turn != 0) {
        printk("Joystick: fwd=%4d/%4d turn=%4d/%4d -> L:%4d%% R:%4d%%\n", 
                 forward, turn, left_speed, right_speed);
    }
}