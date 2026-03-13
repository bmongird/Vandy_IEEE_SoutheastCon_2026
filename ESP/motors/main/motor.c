#include "motor.h"
#include "esp_log.h"

#define SERVO_MIN_PULSEWIDTH_US 500   // Minimum pulse width in microseconds
#define SERVO_MAX_PULSEWIDTH_US 2500  // Maximum pulse width in microseconds
#define SERVO_MAX_ANGLE 300.0         // Maximum angle in degrees
#define TAG "MOTOR"
#define FORWARD_SPEED_CONSTANT  0.66   // Feet per second
#define STRAFE_SPEED_CONSTANT 0.5      // Feet per second
#define ROTATE_SPEED_CONSTANT 42        // degrees per second

mcpwm_timer_handle_t timers[2][3];      // Array of 3 timers in each of 2 groups
mcpwm_oper_handle_t opers[2][3];        // Array of 3 operators in each of 2 groups

void init_motor_resources() {
    for (int group = 0; group < 2; ++group) {
        for (int number = 0; number < 3; ++number) {
            mcpwm_timer_config_t timer_config = {
                .group_id = group,
                .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
                .resolution_hz = 1000000,
                .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
                .period_ticks = 20000
            };
            mcpwm_new_timer(&timer_config, &timers[group][number]);
            mcpwm_timer_enable(timers[group][number]);
            mcpwm_timer_start_stop(timers[group][number], MCPWM_TIMER_START_NO_STOP);

            mcpwm_operator_config_t operator_config = {
                .group_id = group,
            };
            mcpwm_new_operator(&operator_config, &opers[group][number]);
        }
    }
}

void motor_control_init(motor_t *motor) {
    motor->timer = timers[motor->group_id][motor->timer_id];
    motor->oper = opers[motor->group_id][motor->oper_id];
    mcpwm_operator_connect_timer(motor->oper, motor->timer);
 
    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };
    mcpwm_new_comparator(motor->oper, &comparator_config, &motor->comparator);
    if (motor->type == DC_MOTOR) {
        mcpwm_comparator_set_compare_value(motor->comparator, 0);
    } else if (motor->type == SERVO_MOTOR) {
        mcpwm_comparator_set_compare_value(motor->comparator, SERVO_MIN_PULSEWIDTH_US);
    }
    
    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = motor->pwm_pin,
    };
    mcpwm_new_generator(motor->oper, &generator_config, &motor->gen);
    
     
    mcpwm_generator_set_action_on_timer_event(
        motor->gen,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH));
 
    mcpwm_generator_set_action_on_compare_event(motor->gen,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, motor->comparator, MCPWM_GEN_ACTION_LOW));
}
 
void servo_set_angle(motor_t *servo, float angle) {
    if (angle < 0) angle = 0;
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;

    // Map angle to pulse width
    uint32_t pulse_width = SERVO_MIN_PULSEWIDTH_US + 
                            ((SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) * (angle / SERVO_MAX_ANGLE));
 
    // Ensure the generator outputs HIGH at the start of the cycle
    mcpwm_generator_set_action_on_timer_event(
        servo->gen,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)
    );
 
    if (servo->comparator == NULL) {
        mcpwm_comparator_config_t cmp_config = {
            .flags.update_cmp_on_tez = true
        };
        mcpwm_new_comparator(servo->oper, &cmp_config, &servo->comparator);
    }
 
    // Set new pulse width
    mcpwm_comparator_set_compare_value(servo->comparator, pulse_width);
 
    mcpwm_gen_compare_event_action_t cmp_event = {
        .direction = MCPWM_TIMER_DIRECTION_UP,
        .action = MCPWM_GEN_ACTION_LOW,
        .comparator = servo->comparator
    };
 
    mcpwm_generator_set_action_on_compare_event(servo->gen, cmp_event);
 
    // ESP_LOGI("SERVO", "Servo angle set to %.2f degrees (Pulse width: %dus)", angle, (int)pulse_width);
}
 
 
void dc_set_speed(motor_t *motor, float speed) {
    if (speed < -100) {
        speed = -100;
    } else if (speed > 100) {
        speed = 100;
    }
 
    float min_pulse = 1050;
    float max_pulse = 1950;
    float pulse_width = (min_pulse + ((speed + 100) / 200) * (max_pulse - min_pulse));
 
    // Set comparator value dynamically
    mcpwm_comparator_set_compare_value(motor->comparator, (uint32_t)pulse_width);
 
    // ESP_LOGI("MOTOR", "Motor speed set: pin %d, pulse %dus (speed %.2f)",
    //          motor->pwm_pin, (int)pulse_width, speed);
}
 
 
void perform_maneuver(motor_t *motors, maneuver_t maneuver, float speeds[4], float speed_scalar) {
    // Ensure speed_scalar is within the range [0, 100]
    if (speed_scalar < 0) speed_scalar = 0;
    if (speed_scalar > 100) speed_scalar = 100;
 
    // Define motor positions
    motor_t *front_right = &motors[0];
    motor_t *front_left = &motors[1];
    motor_t *back_right = &motors[2];
    motor_t *back_left = &motors[3];
    double FR_ADJUSTMENT = -1 * 0.95;
    double FL_ADJUSTMENT = 1;
    double BR_ADJUSTMENT = -1 * 0.95;
    double BL_ADJUSTMENT = 1;
    switch (maneuver) {
        case FORWARD:
            dc_set_speed(front_right, 1 * speed_scalar * FR_ADJUSTMENT);
            dc_set_speed(front_left, 1 * speed_scalar * FL_ADJUSTMENT);
            dc_set_speed(back_right, 1 * speed_scalar * BR_ADJUSTMENT);
            dc_set_speed(back_left, 1 * speed_scalar * BL_ADJUSTMENT);
            break;
        case BACKWARD:
            dc_set_speed(front_right, -1 * speed_scalar * FR_ADJUSTMENT);
            dc_set_speed(front_left, -1 * speed_scalar * FL_ADJUSTMENT);
            dc_set_speed(back_right, -1 * speed_scalar * BR_ADJUSTMENT);
            dc_set_speed(back_left, -1 * speed_scalar * BL_ADJUSTMENT);
            break;
        case RIGHT:
            dc_set_speed(front_right, -1 * speed_scalar * FR_ADJUSTMENT);
            dc_set_speed(front_left, 1 * speed_scalar * FL_ADJUSTMENT);
            dc_set_speed(back_right, 1 * speed_scalar * BR_ADJUSTMENT);
            dc_set_speed(back_left, -1 * speed_scalar * BL_ADJUSTMENT);
            break;
        case LEFT:
            dc_set_speed(front_right, 1 * speed_scalar * FR_ADJUSTMENT);
            dc_set_speed(front_left, -1 * speed_scalar * FL_ADJUSTMENT);
            dc_set_speed(back_right, -1 * speed_scalar * BR_ADJUSTMENT);
            dc_set_speed(back_left, 1 * speed_scalar * BL_ADJUSTMENT);
            break;
        case FORWARD_LEFT:
            dc_set_speed(front_right, 0);
            dc_set_speed(front_left, 1 * speed_scalar * FL_ADJUSTMENT);
            dc_set_speed(back_right, 1 * speed_scalar * BR_ADJUSTMENT);
            dc_set_speed(back_left, 0);
            break;
        case FORWARD_RIGHT:
            dc_set_speed(front_right, 1 * speed_scalar * FR_ADJUSTMENT);
            dc_set_speed(front_left, 0);
            dc_set_speed(back_right, 0);
            dc_set_speed(back_left, 1 * speed_scalar * BL_ADJUSTMENT);
            break;
        case BACKWARD_LEFT:
            dc_set_speed(front_right, -1 * speed_scalar * FR_ADJUSTMENT);
            dc_set_speed(front_left, 0);
            dc_set_speed(back_right, 0);
            dc_set_speed(back_left, -1 * speed_scalar * BL_ADJUSTMENT);
            break;
        case BACKWARD_RIGHT:
            dc_set_speed(front_right, 0);
            dc_set_speed(front_left, -1 * speed_scalar * FL_ADJUSTMENT);
            dc_set_speed(back_right, -1 * speed_scalar * BR_ADJUSTMENT);
            dc_set_speed(back_left, 0);
            break;
        case ROTATE_COUNTERCLOCKWISE:
            dc_set_speed(front_right, 1 * speed_scalar * FR_ADJUSTMENT);
            dc_set_speed(front_left, -1 * speed_scalar * FL_ADJUSTMENT);
            dc_set_speed(back_right, 1 * speed_scalar * BR_ADJUSTMENT);
            dc_set_speed(back_left, -1 * speed_scalar * BL_ADJUSTMENT);
            break;
        case ROTATE_CLOCKWISE:
            dc_set_speed(front_right, -1 * speed_scalar * FR_ADJUSTMENT);
            dc_set_speed(front_left, 1 * speed_scalar * FL_ADJUSTMENT);
            dc_set_speed(back_right, -1 * speed_scalar * BR_ADJUSTMENT);
            dc_set_speed(back_left, 1 * speed_scalar * BL_ADJUSTMENT);
            break;
        case STOP:
            dc_set_speed(front_right, 0);
            dc_set_speed(front_left, 0);
            dc_set_speed(back_right, 0);
            dc_set_speed(back_left, 0);
            break;
        case CUSTOM:
            dc_set_speed(front_right, speeds[0] * speed_scalar);
            dc_set_speed(front_left, speeds[1] * speed_scalar);
            dc_set_speed(back_right, speeds[2] * speed_scalar);
            dc_set_speed(back_left, speeds[3] * speed_scalar);
            break;
        default:
            // Handle invalid maneuver
            break;
    }
}

void outtake_dump(motor_t *outtakeMotor) {
    int64_t start_time = esp_timer_get_time();
    int64_t elapsed_time = 0;
    int64_t end_time = 3250000;
    dc_set_speed(outtakeMotor, 35);
    while (elapsed_time < (end_time)) {
        elapsed_time = esp_timer_get_time() - start_time;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    dc_set_speed(outtakeMotor, 0);
}

void outtake_reset(motor_t *outtakeMotor) {
    dc_set_speed(outtakeMotor, -25);
    // Wait until the limit switch is triggered (i.e., digital reading goes low).
    while (gpio_get_level(GPIO_NUM_14) != 0) {
        // ESP_LOGI(TAG, "lowering the bucket, %d", gpio_get_level(GPIO_NUM_14));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "lowered, %d", gpio_get_level(GPIO_NUM_14));
    dc_set_speed(outtakeMotor, 0);
}

void move_distance_hardcode(motor_t *motors, maneuver_t maneuver, float speed_scalar, double feet) {
    int64_t start_time = esp_timer_get_time();
    int64_t elapsed_time = 0;
    double multiplier = 0.0;
    if (maneuver == FORWARD || maneuver == BACKWARD) {
        multiplier = FORWARD_SPEED_CONSTANT;
    } else if (maneuver == LEFT || maneuver == RIGHT) {
        multiplier = STRAFE_SPEED_CONSTANT;
    }
    int64_t target_time = 1000000 * feet / ((speed_scalar / 25)* multiplier);
    while (elapsed_time < target_time) {
        perform_maneuver(motors, maneuver, NULL, speed_scalar);
        vTaskDelay(20);
        elapsed_time = esp_timer_get_time() - start_time;
    }
    perform_maneuver(motors, STOP, NULL, 0); 
}

void rotate_angle_hardcode(motor_t *motors, maneuver_t maneuver, float speed_scalar, int degrees) {
    int64_t start_time = esp_timer_get_time();
    int64_t elapsed_time = 0;
    int64_t target_time = 1000000 * degrees / ((speed_scalar / 25) * ROTATE_SPEED_CONSTANT);
    while (elapsed_time < target_time) {
        perform_maneuver(motors, maneuver, NULL, speed_scalar);
        vTaskDelay(20);
        elapsed_time = esp_timer_get_time() - start_time;
    }
    perform_maneuver(motors, STOP, NULL, 0); 
}

void move_distance_encoder(motor_t *motors, maneuver_t maneuver, float speed_scalar, double distance_mm, through_bore_encoder_t *enc) {
    if (maneuver != FORWARD && maneuver != BACKWARD && maneuver != LEFT && maneuver != RIGHT) {
        ESP_LOGE(TAG, "Maneuver not supported by move_distance_encoder");
        return;
    }

    tbe_reset(enc);
    vTaskDelay(pdMS_TO_TICKS(10)); // Allow encoder reset to take effect
    int64_t start_count = tbe_get_count(enc);
    
    // Stop 10mm early to account for momentum
    double effective_target_mm = distance_mm - 10.0;
    if (effective_target_mm < 0) {
        effective_target_mm = 0;
    }

    while (true) {
        int64_t current_count = tbe_get_count(enc);
        int64_t diff = current_count - start_count;
        if (diff < 0) {
            diff = -diff; // absolute distance
        }

        float distance_traveled_mm = (float)diff / (float)TBE_COUNTS_PER_REV * WHEEL_CIRCUMFERENCE_MM;

        if (distance_traveled_mm >= effective_target_mm) {
            break;
        }

        perform_maneuver(motors, maneuver, NULL, speed_scalar);
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to yield to other tasks
    }
    
    perform_maneuver(motors, STOP, NULL, 0);
}
