#!/usr/bin/env bash
# Create an OTA update: AWS Signer signs the S3 object (via the signing profile),
# the OTA Manager builds the stream, and an IoT Job is created for the target.
#
#   scripts/push-ota.sh <thing-name> <version>            # target one Thing
#   scripts/push-ota.sh -g <thing-group-name> <version>   # target a Thing group
#
# The on-device OTA agent then downloads over the MQTT stream, verifies the
# ECDSA-P256 signature, activates, self-tests, and commits/rolls back.
set -euo pipefail
cd "$(dirname "$0")"
. ./_lib.sh
load_stack

TARGET_TYPE="thing"
if [ "${1:-}" = "-g" ]; then TARGET_TYPE="thing-group"; shift; fi
TARGET="${1:?usage: push-ota.sh [-g] <thing-or-group> <version>}"
VERSION="${2:?need the version uploaded by sign-and-publish.sh}"

ACCOUNT="$(aws sts get-caller-identity --query Account --output text)"
if [ "$TARGET_TYPE" = "thing-group" ]; then
  TARGET_ARN="arn:aws:iot:${AWS_REGION}:${ACCOUNT}:thinggroup/${TARGET}"
else
  TARGET_ARN="arn:aws:iot:${AWS_REGION}:${ACCOUNT}:thing/${TARGET}"
fi

KEY="firmware/${VERSION}/firmware.bin"
VERSION_ID="$(aws s3api head-object --bucket "$BUCKET" --key "$KEY" --query VersionId --output text)" \
  || { echo "error: $KEY not in s3://$BUCKET — run sign-and-publish.sh first" >&2; exit 1; }

# Unique, PII-free OTA update id.
OTA_ID="esp32-ota-${VERSION//./-}-$(date +%s)"

FILES="$(cat <<JSON
[
  {
    "fileName": "/firmware.bin",
    "fileVersion": "1",
    "fileLocation": {
      "s3Location": { "bucket": "$BUCKET", "key": "$KEY", "version": "$VERSION_ID" }
    },
    "codeSigning": {
      "startSigningJobParameter": {
        "signingProfileName": "$SIGNING_PROFILE_NAME",
        "destination": { "s3Destination": { "bucket": "$BUCKET" } }
      }
    }
  }
]
JSON
)"

log "creating OTA update '$OTA_ID' -> $TARGET_ARN"
aws iot create-ota-update \
  --ota-update-id "$OTA_ID" \
  --description "ESP32 OTA POC v$VERSION" \
  --targets "$TARGET_ARN" \
  --target-selection SNAPSHOT \
  --protocols MQTT \
  --files "$FILES" \
  --role-arn "$OTA_ROLE_ARN" \
  --aws-job-executions-rollout-config '{"maximumPerMinute": 5}' \
  --aws-job-abort-config '{"abortCriteriaList":[{"failureType":"FAILED","action":"CANCEL","thresholdPercentage":50,"minNumberOfExecutedThings":1}]}' \
  --aws-job-timeout-config '{"inProgressTimeoutInMinutes": 10}' \
  --query '{otaUpdateId:otaUpdateId,jobId:awsIotJobId,status:otaUpdateStatus}' --output table

echo
log "watch it with:  scripts/watch-job.sh $([ "$TARGET_TYPE" = thing ] && echo "$TARGET" || echo "$THING_NAME")"
echo "  (the IoT Job id is AFR_OTA-$OTA_ID)"
