#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ultrasonic/ultrasonic.h"

void app_main(void)
{
    ultrasonic_init();

    while (1)
    {
        float distance = ultrasonic_read_cm();
        printf("Distance: %.2f cm\n", distance);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}