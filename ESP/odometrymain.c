#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "encoders.h"
#include "odometry.h"
#include "drive.h"

void app_main(void)
{
    printf("Robot starting...\n");

    encoders_init();

    while (1)
    {
        int32_t left  = encoder_left_ticks();
        int32_t right = encoder_right_ticks();
        int32_t perp  = encoder_perp_ticks();

        printf("L:%ld R:%ld P:%ld\n", left, right, perp);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
