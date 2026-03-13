/** 
 * @file motor.h
 * @brief Motor setup and control functions
 * 
 * This file declares the functions to initialize and control DC motors using the MCPWM peripheral on the ESP32.
 * 
 * @author Solomon Tolson
 * @version 1.1
 * @date 2025-02-11
 * @modified 2025-03-17
 */

 #ifndef MOTOR_H
 #define MOTOR_H
 
 #include "driver/mcpwm_prelude.h"
 #include "driver/gpio.h"
 #include "esp_log.h"
 #include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pid.h"      // Include PID header for PID control
#include "encoder.h"  // Include encoder header for encoder-based movements
 
 /**
  * @brief Enumeration for the different motor types
  */
 typedef enum {
     DC_MOTOR,
     SERVO_MOTOR
 } motor_type_t;
 
 /**
  * @brief Struct to hold motor configuration
  * 
  * Components:
  * - timer: MCPWM timer handle
  * - oper: MCPWM operator handle
  * - gen: MCPWM generator handle
  * - comparator: MCPWM comparator handle
  * - pwm_pin: MCPWM output pin
  * - group_id: MCPWM group ID
  * - type: Motor type (DC or servo)
  */
 typedef struct {
     mcpwm_timer_handle_t timer;      // MCPWM timer handle
     mcpwm_oper_handle_t oper;        // MCPWM operator handle
     mcpwm_gen_handle_t gen;          // MCPWM generator handle
     mcpwm_cmpr_handle_t comparator;  // MCPWM comparator handle
     int pwm_pin;                     // MCPWM output pin
     int group_id;                    // MCPWM group ID
     int timer_id;
     int oper_id;
     motor_type_t type;               // Motor type (DC or servo)
 } motor_t;

void init_motor_resources();

 /**
  * @brief Initialize a motor configuration
  * 
  * @param motors Pointer to the motor_t struct.
  */
 void motor_control_init(motor_t *motor);
 
 /**
  * @brief Set the servo angle.
  * 
  * @param servo Pointer to the motor_t struct.
  * @param angle Angle in degrees (0-180).
  */
 void servo_set_angle(motor_t *servo, float angle);
 
 /**
  * @brief Set the speed of a motor
  * 
  * @param motor Motor to control
  * @param speed Speed to set (-100 to 100)
  */
 void dc_set_speed(motor_t *motor, float speed);
 
 /**
  * @brief Enumeration for the different omnidirectional maneuvers
  */
 typedef enum {
     FORWARD,
     BACKWARD,
     LEFT,
     RIGHT,
     FORWARD_LEFT,
     FORWARD_RIGHT,
     BACKWARD_LEFT,
     BACKWARD_RIGHT,
     ROTATE_CLOCKWISE,
     ROTATE_COUNTERCLOCKWISE,
     STOP,
     CUSTOM
 } maneuver_t;
 
 /**
  * @brief Perform a maneuver with the robot
  * 
  * This function sets the speed of 4 motors to perform a specific omnidirectional maneuver.
  * 
  * @param motors Array of 4 motors
  * @param maneuver The maneuver to perform
  * @param speeds Array of speeds for each motor (only used for CUSTOM maneuver)
  * @param speed_scalar Scalar to scale the speed of the robot (0 to 100)
  */
 void perform_maneuver(motor_t *motors, maneuver_t maneuver, float speeds[4], float speed_scalar);
 
 void outtake_dump(motor_t *outtakeMotor);

 void outtake_reset(motor_t *outtakeMotor);

 void move_distance_hardcode(motor_t *motors, maneuver_t maneuver, float speed_scalar, double feet);

 void rotate_angle_hardcode(motor_t * motors, maneuver_t direction, float speed_scalar, int degrees);

 void move_distance_encoder(motor_t *motors, maneuver_t maneuver, float speed_scalar, double distance_mm, through_bore_encoder_t *enc);

 #endif // MOTOR_H