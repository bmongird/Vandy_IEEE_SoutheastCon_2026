#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

// Bring in our copied libraries
#include "motor.h"
#include "spi_secondary.h"
#include "pid.h"
#include "encoder.h"

#define TAG "MOTORS_MAIN"

// ── Encoder config ──────────────────────────────────────────────
// Using input-only GPIOs to avoid conflicts with motor PWM pins
#define ENC_PIN_A      GPIO_NUM_34
#define ENC_PIN_B      GPIO_NUM_35
#define ENC_PIN_INDEX  GPIO_NUM_36
#define ENC_PIN_ABS    GPIO_NUM_39

through_bore_encoder_t encoder1;
through_bore_encoder_t encoder2; // If you have a second encoder, configure it similarly

// Mimicking the robot structure from IEEE_Hardware_Competition_2025
typedef struct {
    motor_t frontLeft;
    motor_t frontRight;
    motor_t backRight;
    motor_t backLeft;
    motor_t eliServo1; //idk what u wanna name these
    motor_t eliServo2;
    motor_t eliServo3;
    motor_t lazyTong;

    motor_t *omniMotors;    /* An array of the four maneuver DC motors*/
} robot_t;

robot_t robot_singleton;

void full_motor_init() {
    init_motor_resources();

    robot_singleton.omniMotors = malloc(sizeof(motor_t) * 4);

    motor_t frontLeft =     { .pwm_pin = 17,    .group_id = 0, .timer_id = 0, .oper_id = 0, .type = DC_MOTOR };
    motor_t frontRight =    { .pwm_pin = 21,    .group_id = 0, .timer_id = 0, .oper_id = 0, .type = DC_MOTOR };
    motor_t backLeft =      { .pwm_pin = 25,    .group_id = 0, .timer_id = 1, .oper_id = 1, .type = DC_MOTOR };
    motor_t backRight =     { .pwm_pin = 32,    .group_id = 0, .timer_id = 1, .oper_id = 1, .type = DC_MOTOR };
    motor_t eliServo1 =   { .pwm_pin = 33,    .group_id = 0, .timer_id = 2, .oper_id = 2, .type = SERVO_MOTOR };
    motor_t eliServo2 =  { .pwm_pin = 4,     .group_id = 0, .timer_id = 2, .oper_id = 2, .type = SERVO_MOTOR };
    motor_t eliServo3 =  { .pwm_pin = 27,     .group_id = 0, .timer_id = 2, .oper_id = 2, .type = SERVO_MOTOR };
    motor_t lazyTong =      { .pwm_pin = 3,    .group_id = 1, .timer_id = 0, .oper_id = 0, .type = SERVO_MOTOR };

    motor_control_init(&frontLeft);
    motor_control_init(&frontRight);
    motor_control_init(&backRight);
    motor_control_init(&backLeft);
    motor_control_init(&eliServo1);
    motor_control_init(&eliServo2);
    motor_control_init(&eliServo3);
    motor_control_init(&lazyTong);

    robot_singleton.frontLeft = frontLeft;
    robot_singleton.frontRight = frontRight;
    robot_singleton.backLeft = backLeft;
    robot_singleton.backRight = backRight;
    robot_singleton.eliServo1 = eliServo1;
    robot_singleton.eliServo2 = eliServo2;
    robot_singleton.eliServo3 = eliServo3;
    robot_singleton.lazyTong = lazyTong;
    robot_singleton.omniMotors[0] = frontRight;
    robot_singleton.omniMotors[1] = frontLeft;
    robot_singleton.omniMotors[2] = backRight;
    robot_singleton.omniMotors[3] = backLeft;

    dc_set_speed(&robot_singleton.eliServo1, 0);
    dc_set_speed(&robot_singleton.eliServo2, 0);
    dc_set_speed(&robot_singleton.eliServo3, 0);
    perform_maneuver(robot_singleton.omniMotors, STOP, NULL, 0);
    servo_set_angle(&robot_singleton.lazyTong, 240);
    
    // outtake_reset logic is skipped since we lack the GPIO configuration for it here for now.
}


int setup() {
    /* Motor Initialization Sequence */
    ESP_LOGI(TAG, "Initializing Motors");
    full_motor_init();
    
    // Give it a brief moment
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Encoder Initialization */
    ESP_LOGI(TAG, "Initializing Through Bore Encoder");
    encoder1 = (through_bore_encoder_t){
        .pin_a     = ENC_PIN_A,
        .pin_b     = ENC_PIN_B,
        .pin_index = ENC_PIN_INDEX,
        .pin_abs   = ENC_PIN_ABS,
    };
    if (tbe_init(&encoder1) != ESP_OK) {
        ESP_LOGE(TAG, "Encoder init failed!");
        return -1;
    }
    encoder2 = (through_bore_encoder_t){
        .pin_a     = GPIO_NUM_13, // Example pins for a second encoder
        .pin_b     = GPIO_NUM_14,
        .pin_index = GPIO_NUM_16,
        .pin_abs   = GPIO_NUM_26,
    };
    if (tbe_init(&encoder2) != ESP_OK) {
        ESP_LOGE(TAG, "Second encoder init failed!");
        return -1;
    }

    /* SPI Communication Initialization Sequence */
    ESP_LOGI(TAG, "Initializing SPI Communication");
    spi_secondary_init();
    
    ESP_LOGI(TAG, "Setup Complete");
    return 0;
}

typedef enum {
    IDLE,
    DUCKS,
    ANTENNA1,
    ANTENNA2,
    ANTENNA4,
    END
} state_t;

void antenna1_action() {

    move_distance_encoder(robot_singleton.omniMotors, BACKWARD, 30, 25, &encoder2);
    vTaskDelay(pdMS_TO_TICKS(2000));
    perform_maneuver(robot_singleton.omniMotors, ROTATE_COUNTERCLOCKWISE, NULL, 30);
    vTaskDelay(pdMS_TO_TICKS(2000));
    perform_maneuver(robot_singleton.omniMotors, ROTATE_COUNTERCLOCKWISE, NULL, 30);
    vTaskDelay(pdMS_TO_TICKS(2000));
    move_distance_encoder(robot_singleton.omniMotors, LEFT, 30, 686, &encoder1);
    vTaskDelay(pdMS_TO_TICKS(2000));
    move_distance_encoder(robot_singleton.omniMotors, FORWARD, 30, 661, &encoder2);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    //antenna 1 logic with servos
    servo_set_angle(&robot_singleton.eliServo1, 0);//Need to adjust depending on how long it takes to reach full extension
    vTaskDelay(pdMS_TO_TICKS(3000));

    servo_set_angle(&robot_singleton.eliServo2, 350);
    vTaskDelay(pdMS_TO_TICKS(3000));

    servo_set_angle(&robot_singleton.eliServo1, 0);//Need to adjust depending on how long it takes to reach full extension
    vTaskDelay(pdMS_TO_TICKS(3000));

    servo_set_angle(&robot_singleton.eliServo1, 300);//Same here
    vTaskDelay(pdMS_TO_TICKS(4000));

    servo_set_angle(&robot_singleton.eliServo2, 0);
    vTaskDelay(pdMS_TO_TICKS(3000));
}

void antenna2_action()
{
    move_distance_encoder(robot_singleton.omniMotors, BACKWARD, 30, 25, &encoder2);
    vTaskDelay(pdMS_TO_TICKS(2000));
    perform_maneuver(robot_singleton.omniMotors, ROTATE_CLOCKWISE, NULL, 30);
    vTaskDelay(pdMS_TO_TICKS(2000));
    move_distance_encoder(robot_singleton.omniMotors, FORWARD, 30, 100, &encoder2);
    vTaskDelay(pdMS_TO_TICKS(2000));
    move_distance_encoder(robot_singleton.omniMotors, LEFT, 30, 85, &encoder1);
    vTaskDelay(pdMS_TO_TICKS(2000));
    move_distance_encoder(robot_singleton.omniMotors, FORWARD, 30, 662, &encoder2);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    
    

    servo_set_angle(&robot_singleton.eliServo2, 150);
    vTaskDelay(pdMS_TO_TICKS(2000));

    servo_set_angle(&robot_singleton.eliServo2, 300);
    vTaskDelay(pdMS_TO_TICKS(7000));

    servo_set_angle(&robot_singleton.eliServo2, 150);
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Antenna 2 action executed (placeholder)");
}

void antenna4_action()
{
    move_distance_encoder(robot_singleton.omniMotors, BACKWARD, 30, 110, &encoder2);
    vTaskDelay(pdMS_TO_TICKS(2000));
    move_distance_encoder(robot_singleton.omniMotors, LEFT, 30, 762, &encoder1);
    vTaskDelay(pdMS_TO_TICKS(2000));
    move_distance_encoder(robot_singleton.omniMotors, FORWARD, 30, 25, &encoder2);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    
    
    servo_set_angle(&robot_singleton.eliServo3, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // servo_set_angle(&robot_singleton.eliServo3, 350);
    // vTaskDelay(pdMS_TO_TICKS(7000));

    // servo_set_angle(&robot_singleton.eliServo3, 0);
    // vTaskDelay(pdMS_TO_TICKS(2000));
    ESP_LOGI(TAG, "Antenna 4 action executed (placeholder)");
}

void run_antenna_path()
{
    const int turn_time = 700;
    const int move_time = 1500;

    perform_maneuver(robot_singleton.omniMotors, ROTATE_COUNTERCLOCKWISE, NULL, 40);
    vTaskDelay(pdMS_TO_TICKS(turn_time));
    perform_maneuver(robot_singleton.omniMotors, STOP, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));

    perform_maneuver(robot_singleton.omniMotors, FORWARD, NULL, 10);
    vTaskDelay(pdMS_TO_TICKS(move_time));
    perform_maneuver(robot_singleton.omniMotors, STOP, NULL, 0);
    antenna1_action();
    vTaskDelay(pdMS_TO_TICKS(1000));
    antenna1_action();

    perform_maneuver(robot_singleton.omniMotors, ROTATE_CLOCKWISE, NULL, 50);
    vTaskDelay(pdMS_TO_TICKS(turn_time));
    perform_maneuver(robot_singleton.omniMotors, STOP, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));

    perform_maneuver(robot_singleton.omniMotors, FORWARD, NULL, 10);
    vTaskDelay(pdMS_TO_TICKS(move_time));
    perform_maneuver(robot_singleton.omniMotors, STOP, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));

    perform_maneuver(robot_singleton.omniMotors, ROTATE_COUNTERCLOCKWISE, NULL, 40);
    vTaskDelay(pdMS_TO_TICKS(turn_time));
    perform_maneuver(robot_singleton.omniMotors, STOP, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));

    perform_maneuver(robot_singleton.omniMotors, FORWARD, NULL, 10);
    vTaskDelay(pdMS_TO_TICKS(move_time));
    perform_maneuver(robot_singleton.omniMotors, STOP, NULL, 0);

    perform_maneuver(robot_singleton.omniMotors, ROTATE_CLOCKWISE, NULL, 50);
    vTaskDelay(pdMS_TO_TICKS(turn_time));
    perform_maneuver(robot_singleton.omniMotors, STOP, NULL, 0);

    perform_maneuver(robot_singleton.omniMotors, FORWARD, NULL, 5);
    vTaskDelay(pdMS_TO_TICKS(move_time));
    perform_maneuver(robot_singleton.omniMotors, STOP, NULL, 0);
    //Antenna 2 reached, simulate outtake

    vTaskDelay(pdMS_TO_TICKS(1000));

    perform_maneuver(robot_singleton.omniMotors, FORWARD, NULL, 5);
    vTaskDelay(pdMS_TO_TICKS(move_time));
    perform_maneuver(robot_singleton.omniMotors, STOP, NULL, 0);
    //Antenna 3 reached, simulate outtake

    perform_maneuver(robot_singleton.omniMotors, ROTATE_CLOCKWISE, NULL, 50);
    vTaskDelay(pdMS_TO_TICKS(turn_time));
    perform_maneuver(robot_singleton.omniMotors, STOP, NULL, 0);
}


void app_main(void)
{
    ESP_LOGI(TAG, "Starting application...");
    if (setup() != 0) {
        ESP_LOGE(TAG, "Setup failed. Restarting");
        vTaskDelay(200);
        esp_restart();
    }
    
    state_t currentState = ANTENNA2; // Start with ANTENNA4 for testing, will be set by SPI commands in practice
    uint8_t last_cmd = STATE_IDLE;
    int loop_counter = 0;
    bool state_executed = false; // Prevents re-executing long functions
    bool state_executed1 = false; // For states with multiple steps like ANTENNA1/2/4
    bool state_executed2 = false; // For states with multiple steps like ANTENNA1/2/4
    bool state_executed3 = false; // For states with multiple steps like ANTENNA1/2/4

    while(currentState != END) {
        // Read state command from SPI (binary protocol)
        ESP_LOGI(TAG, "Waiting for SPI command...");
        uint8_t cmd = get_current_command();
        if (cmd != last_cmd) {
            last_cmd = cmd;
            state_executed = false;

            switch (cmd) {
                case STATE_IDLE:
                    currentState = IDLE;
                    ESP_LOGI(TAG, "State transition via SPI: IDLE");
                    break;
                case STATE_DUCKS:
                    currentState = DUCKS;
                    ESP_LOGI(TAG, "State transition via SPI: DUCKS");
                    break;
                case STATE_ANTENNA:
                    currentState = ANTENNA4;
                    ESP_LOGI(TAG, "State transition via SPI: ANTENNA");
                    break;
                default:
                    ESP_LOGW(TAG, "Unknown state code: 0x%02X", cmd);
                    break;
            }
        }

        // ── Print encoder distance every 10 loops ──────────────
        if (loop_counter % 10 == 0) {
            
            int64_t counts = tbe_get_count(&encoder1);
            float distance_mm = (float)counts / (float)TBE_COUNTS_PER_REV * WHEEL_CIRCUMFERENCE_MM;
            float revs = tbe_get_revolutions(&encoder1);
            float abs_deg = tbe_get_abs_angle(&encoder1);

            ESP_LOGI(TAG, "ENC | dist: %.1f mm | counts: %lld | revs: %.2f | abs: %.1f°",
                     distance_mm, (long long)counts, revs, abs_deg);

            int64_t counts2 = tbe_get_count(&encoder2);
            float distance_mm2 = (float)counts2 / (float)TBE_COUNTS_PER_REV * WHEEL_CIRCUMFERENCE_MM;
            float revs2 = tbe_get_revolutions(&encoder2);
            float abs_deg2 = tbe_get_abs_angle(&encoder2);

            ESP_LOGI(TAG, "ENC2| dist: %.1f mm | counts: %lld | revs: %.2f | abs: %.1f°",
                     distance_mm2, (long long)counts2, revs2, abs_deg2);
        }

        switch (currentState) {
            case IDLE:
                if (loop_counter % 10 == 0) {
                    ESP_LOGI(TAG, "Executing state: IDLE");
                }
                perform_maneuver(robot_singleton.omniMotors, STOP, NULL, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
                
            case DUCKS:
                if (!state_executed) {
                    ESP_LOGI(TAG, "Executing state: DUCKS");
                    // TODO: Implement abstract DUCKS state logic
                    // move_to_ducks();
                    
                    // Simulate long running function
                    vTaskDelay(pdMS_TO_TICKS(1000)); 
                    
                    report_state_complete();
                    state_executed = true; // Mark done
                    
                    // Transition to IDLE while waiting for next Pi command
                    currentState = IDLE; 
                }
                break;
                
            case ANTENNA1:
                if (!state_executed2) {
                    ESP_LOGI(TAG, "Executing state: ANTENNA1");
                    vTaskDelay(pdMS_TO_TICKS(4000)); // Simulate processing delay before action
                    // run_antenna_path();
                    antenna1_action();
                    
                    // Simulate long running function
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    
                    report_state_complete();
                    state_executed2 = true; // Mark done
                    
                    // Transition to IDLE while waiting for next Pi command
                    currentState = ANTENNA2; 
                    ESP_LOGI(TAG, "Moving to ANTENNA2");
                }
                break;
            
            case ANTENNA2:
                if (!state_executed3) {
                    ESP_LOGI(TAG, "Executing state: ANTENNA2");
                    // run_antenna_path();
                    antenna2_action();
                    
                    // Simulate long running function
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    
                    report_state_complete();
                    state_executed3 = true; // Mark done
                    state_executed2 = true; // Reset for potential re-entry
                    
                    // Transition to IDLE while waiting for next Pi command
                    currentState = IDLE; 
                }
                break;
            case ANTENNA4:
                if (!state_executed) {
                    ESP_LOGI(TAG, "Executing state: ANTENNA4");
                    // run_antenna_path();
                    // perform_maneuver(robot_singleton.omniMotors, FORWARD, NULL, 10);
                    antenna4_action();
                    
                    // Simulate long running function
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    
                    report_state_complete();
                    state_executed1 = true; // Mark done
                    
                    // Transition to IDLE while waiting for next Pi command
                    currentState = ANTENNA1; 
                    ESP_LOGI(TAG, "Moving to ANTENNA1");
                }
                break;
            
            case END:
                break;
        }
        loop_counter++;
    }
    
    // Stop all motors on exit
    perform_maneuver(robot_singleton.omniMotors, STOP, NULL, 0);
    ESP_LOGI(TAG, "Program complete");
}