/*
 * led.c — blink the onboard LED `APP_VERSION_MAJOR` times per cycle.
 *
 * The ESP-IDF translation of the classic Arduino blink: digitalWrite()/delay() in
 * loop() becomes gpio_set_level() + vTaskDelay() in a FreeRTOS task. Instead of a
 * fixed 1 Hz blink it pulses N times (N = major version) then pauses, so the
 * version is countable on the board and an OTA-upgrade is visible the instant the
 * new image runs. Pin + polarity come from app_config.h (LED_GPIO / LED_ACTIVE_HIGH).
 */
#include "led.h"
#include "app_config.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_PULSE_ON_MS    180   /* a single short pulse: on ... */
#define LED_PULSE_OFF_MS   220   /* ... then off, so pulses are countable */
#define LED_GROUP_GAP_MS  1400   /* gap between version-count groups */

static int s_blinks = 1;

static inline void led_write(bool on)
{
    gpio_set_level(LED_GPIO, (LED_ACTIVE_HIGH ? on : !on) ? 1 : 0);
}

static void led_task(void *arg)
{
    (void)arg;
    for (;;) {
        for (int i = 0; i < s_blinks; i++) {
            led_write(true);
            vTaskDelay(pdMS_TO_TICKS(LED_PULSE_ON_MS));
            led_write(false);
            vTaskDelay(pdMS_TO_TICKS(LED_PULSE_OFF_MS));
        }
        vTaskDelay(pdMS_TO_TICKS(LED_GROUP_GAP_MS));   /* pause, then repeat the count */
    }
}

void led_start_version_blink(int major, int minor, int build)
{
    (void)minor;
    (void)build;
    s_blinks = (major > 0) ? major : 1;   /* at least one pulse, even for v0.x */

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    led_write(false);

    xTaskCreate(led_task, "led", 2048, NULL, 3, NULL);
}
