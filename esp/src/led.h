/*
 * led.h — onboard-LED version indicator.
 *
 * Blinks the board LED in a pattern that encodes the firmware version, so a
 * device's running version is visible at a glance and a fleet OTA-upgrade (or a
 * rollback) can be watched by eye. Shared verbatim across all four OTA backends.
 */
#ifndef LED_H
#define LED_H

/* Start a background task that blinks the onboard LED: `major` short pulses, then
 * a long gap, repeating. v1.x.y -> one blink per cycle, v2.x.y -> two, and so on.
 * Call once early in app_main(); the pattern is fixed for this image's lifetime,
 * so the moment a board boots a new OTA image its blink count changes. minor/build
 * are accepted for future encodings but only `major` drives the count today. */
void led_start_version_blink(int major, int minor, int build);

#endif /* LED_H */
