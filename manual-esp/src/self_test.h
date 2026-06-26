/*
 * self_test.h — post-OTA trial-boot handling: rollback detection, the self-test
 * watchdog, commit / reject, and the NVS hand-off of the in-flight job id.
 *
 * Commit semantics (see ../README.md): a freshly-activated image boots
 * PENDING_VERIFY; we arm a watchdog, run a self-test (cloud round-trip already
 * proven by the live MQTT connection + a core-function check) and only then
 * commit. A failed self-test rejects -> the bootloader rolls back to the
 * previous slot. Uses IDF-native esp_ota_* rollback APIs.
 */
#ifndef SELF_TEST_H
#define SELF_TEST_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

/* True if the running image is in the PENDING_VERIFY (trial) state, i.e. it was
 * just delivered by OTA and has not been committed yet. */
bool self_test_is_trial_boot(void);

/* Log running partition, app version, and OTA/reset state. */
void self_test_log_boot_info(void);

/* Arm / disarm the Task WDT that bounds the self-test window. On timeout it
 * panics -> reboot -> (uncommitted image) bootloader rolls back. */
void self_test_arm_watchdog(void);
void self_test_feed_watchdog(void);
void self_test_disarm_watchdog(void);

/* The application core-function check. vGOOD returns true; vBAD returns false
 * (built with FW_SELFTEST_SHOULD_PASS=0) to demonstrate rollback. */
bool self_test_core_function_ok(void);

/* Commit the running image (cancel rollback). Call only after the self-test
 * passes. */
void self_test_commit(void);

/* Reject the running image and reboot -> bootloader rolls back. Does not
 * return. */
void self_test_reject_and_rollback(void) __attribute__((noreturn));

/* NVS hand-off of the in-flight OTA job id across the activation reboot. */
esp_err_t ota_nvs_set_job_id(const char *job_id);
esp_err_t ota_nvs_get_job_id(char *out, size_t out_len);   /* ESP_ERR_NVS_NOT_FOUND if none */
void      ota_nvs_clear_job_id(void);

#endif /* SELF_TEST_H */
