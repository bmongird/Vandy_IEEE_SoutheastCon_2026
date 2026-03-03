#include "pid.h"

void pid_init(PIDController *pid, float kp, float ki, float kd) {
    pid->Kp = kp;
    pid->Ki = ki;
    pid->Kd = kd;
    pid->integral = 0;
    pid->last_error = 0;
}

int pid_compute(PIDController *pid, int target, int current) {
    int error = target - current;
    pid->integral += error;
    int derivative = error - pid->last_error;
    pid->last_error = error;

    return (int)(pid->Kp * error + pid->Ki * pid->integral + pid->Kd * derivative);
}
