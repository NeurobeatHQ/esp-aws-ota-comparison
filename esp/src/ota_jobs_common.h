/*
 * ota_jobs_common.h — the AWS IoT Jobs control plane shared by the three
 * Jobs-based OTA backends (mqtt / https / jobs).
 *
 * These functions are byte-identical duplicates that used to live in each
 * orchestrator: the StartNext request, the Jobs_Update status report, the
 * start-next/accepted subscribe, and the post-commit idempotency guard (which
 * re-affirms SUCCEEDED for the job just committed on a trial boot). The per-backend
 * DATA path (HTTP GET vs File-Streams blocks vs esp_https_ota) stays in each
 * orchestrator. Only the mqtt/https/jobs CMake branches compile this file.
 */
#ifndef OTA_JOBS_COMMON_H
#define OTA_JOBS_COMMON_H

#include <stdbool.h>
#include <stddef.h>

/* Publish a Jobs StartNext to claim the next queued execution. */
void ota_jobs_request_document(void);

/* Build the Jobs Update topic for jobId and publish {"status":"<status>"}.
 * Returns true if the publish was accepted by the transport. */
bool ota_jobs_report_status(const char *jobId, const char *status);

/* Subscribe to the start-next/accepted response topic (deferred-to-boot: the
 * live notify-next push is intentionally NOT subscribed). */
void ota_jobs_subscribe_topics(void);

/* Idempotency guard for the trial-committed job. If jobId equals the job id
 * recorded by ota_jobs_trial_report() on a passing trial boot, log + re-affirm
 * SUCCEEDED and return true (the caller must then NOT re-download it); else
 * return false. */
bool ota_jobs_is_completed(const char *jobId, size_t jobIdLen);

/* Trial-boot report callback for self_test_resolve_trial(): on ok=true reports
 * SUCCEEDED, records the id for the idempotency guard, and clears the NVS id; on
 * ok=false reports FAILED, waits briefly to flush, then clears the NVS id. */
void ota_jobs_trial_report(const char *jobId, bool ok);

#endif /* OTA_JOBS_COMMON_H */
