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

#define TAG "MOTORS_MAIN"

// Mimicking the robot structure from IEEE_Hardware_Competition_2025
typedef struct {
    motor_t frontLeft;
    motor_t frontRight;
    motor_t backRight;
    motor_t backLeft;
    motor_t eliServo1; //idk what u wanna name these
    motor_t eliServo2;
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
    motor_t lazyTong =      { .pwm_pin = 27,    .group_id = 1, .timer_id = 0, .oper_id = 0, .type = SERVO_MOTOR };

    motor_control_init(&frontLeft);
    motor_control_init(&frontRight);
    motor_control_init(&backRight);
    motor_control_init(&backLeft);
    motor_control_init(&eliServo1);
    motor_control_init(&eliServo2);
    motor_control_init(&lazyTong);

    robot_singleton.frontLeft = frontLeft;
    robot_singleton.frontRight = frontRight;
    robot_singleton.backLeft = backLeft;
    robot_singleton.backRight = backRight;
    robot_singleton.eliServo1 = eliServo1;
    robot_singleton.eliServo2 = eliServo2;
    robot_singleton.lazyTong = lazyTong;
    robot_singleton.omniMotors[0] = frontRight;
    robot_singleton.omniMotors[1] = frontLeft;
    robot_singleton.omniMotors[2] = backRight;
    robot_singleton.omniMotors[3] = backLeft;

    dc_set_speed(&robot_singleton.eliServo1, 0);
    dc_set_speed(&robot_singleton.eliServo2, 0);
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
    END
} state_t;

void app_main(void)
{
    ESP_LOGI(TAG, "Starting application...");
    if (setup() != 0) {
        ESP_LOGE(TAG, "Setup failed. Restarting");
        vTaskDelay(200);
        esp_restart();
    }
    
    state_t currentState = IDLE;
    char last_msg[32] = "";
    int loop_counter = 0;
    bool state_executed = false; // Prevents re-executing long functions

    while(currentState != END) {
        // Read SPI message to handle State Updates
        char* msg = get_message();
        if (msg != NULL) {
            if (strcmp(msg, last_msg) != 0) {
                // New message received
                strncpy(last_msg, msg, sizeof(last_msg) - 1);
                
                if (strcmp(msg, "IDLE") == 0) {
                    currentState = IDLE;
                    state_executed = false;
                    ESP_LOGI(TAG, "State transition via SPI: IDLE");
                } else if (strcmp(msg, "DUCKS") == 0) {
                    currentState = DUCKS;
                    state_executed = false;
                    ESP_LOGI(TAG, "State transition via SPI: DUCKS");
                } else if (strcmp(msg, "ANTENNA1") == 0) {
                    currentState = ANTENNA1;
                    state_executed = false;
                    ESP_LOGI(TAG, "State transition via SPI: ANTENNA1");
                } else if (strcmp(msg, "END") == 0) {
                    currentState = END;
                    state_executed = false;
                    ESP_LOGI(TAG, "State transition via SPI: END");
                }
            }
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
                    
                    send_message("DUCKS_DONE");
                    state_executed = true; // Mark done
                    
                    // Transition to IDLE while waiting for next PI command
                    currentState = IDLE; 
                }
                break;
                
            case ANTENNA1:
                if (!state_executed) {
                    ESP_LOGI(TAG, "Executing state: ANTENNA1");
                    // TODO: Implement abstract ANTENNA1 state logic
                    // move_to_antenna1();
                    
                    // Simulate long running function
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    
                    send_message("ANTENNA1_DONE");
                    state_executed = true; // Mark done
                    
                    // Transition to IDLE while waiting for next PI command
                    currentState = IDLE; 
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
