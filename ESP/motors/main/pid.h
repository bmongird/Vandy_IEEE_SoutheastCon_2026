#ifndef PID_H
#define PID_H

typedef struct {
    float Kp, Ki, Kd;
    int integral;
    int last_error;
} PIDController;

void pid_init(PIDController *pid, float kp, float ki, float kd);
int pid_compute(PIDController *pid, int target, int current);

#endif // PID_H