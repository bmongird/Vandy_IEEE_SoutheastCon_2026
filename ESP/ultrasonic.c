#include "ultrasonic.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"


#define TRIG_PIN GPIO_NUM_5
#define ECHO_PIN GPIO_NUM_18

void ultrasonic_init()
{
    gpio_set_direction(TRIG_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(ECHO_PIN, GPIO_MODE_INPUT);

    gpio_set_level(TRIG_PIN, 0);
}

float ultrasonic_read_cm()
{
    // --- Send 10µs trigger pulse ---
    gpio_set_level(TRIG_PIN, 0);
    esp_rom_delay_us(2);


    gpio_set_level(TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_PIN, 0);

    // --- Wait for echo to go HIGH ---
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(ECHO_PIN) == 0)
    {
        if ((esp_timer_get_time() - start) > 30000)
            return -1.0; // timeout (no echo)
    }

    // --- Measure how long echo stays HIGH ---
    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_PIN) == 1)
    {
        if ((esp_timer_get_time() - echo_start) > 30000)
            return -1.0; // timeout
    }

    int64_t echo_end = esp_timer_get_time();

    int64_t duration = echo_end - echo_start; // microseconds

    // --- Convert time to distance ---
    float distance_cm = (float)duration / 58.0f;

    return distance_cm;
}