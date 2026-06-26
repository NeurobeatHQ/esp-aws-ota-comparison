#!/usr/bin/env bash
# Variant B — create an AWS IoT Job with a CUSTOM document carrying a presigned
# S3 URL. No AWS Signer, no OTA service role, no create-ota-update: the device
# self-downloads with esp_https_ota.
#
#   scripts/create-job.sh <thing-name> <version>
#   scripts/create-job.sh -g <thing-group-name> <version>
set -euo pipefail
cd "$(dirname "$0")"
. ./_lib.sh
load_stack

TARGET_TYPE="thing"
if [ "${1:-}" = "-g" ]; then TARGET_TYPE="thing-group"; shift; fi
TARGET="${1:?usage: create-job.sh [-g] <thing-or-group> <version>}"
VERSION="${2:?need the version uploaded by presign-and-upload.sh}"

ACCOUNT="$(aws sts get-caller-identity --query Account --output text)"
if [ "$TARGET_TYPE" = "thing-group" ]; then
  TARGET_ARN="arn:aws:iot:${AWS_REGION}:${ACCOUNT}:thinggroup/${TARGET}"
else
  TARGET_ARN="arn:aws:iot:${AWS_REGION}:${ACCOUNT}:thing/${TARGET}"
fi

KEY="firmware/${VERSION}/firmware.bin"
URL="$(aws s3 presign "s3://$BUCKET/$KEY" --expires-in 3600)" \
  || { echo "error: presign failed — run presign-and-upload.sh first" >&2; exit 1; }

JOB_ID="esp32-ota-${VERSION//./-}-$(date +%s)"
DOC="$(jq -nc --arg url "$URL" --arg ver "$VERSION" \
  '{op:"ota", url:$url, target_version:$ver}')"

log "creating IoT Job '$JOB_ID' -> $TARGET_ARN"
aws iot create-job \
  --job-id "$JOB_ID" \
  --targets "$TARGET_ARN" \
  --document "$DOC" \
  --target-selection SNAPSHOT \
  --timeout-config inProgressTimeoutInMinutes=10 \
  --abort-config '{"criteriaList":[{"failureType":"FAILED","action":"CANCEL","thresholdPercentage":50,"minNumberOfExecutedThings":1}]}' \
  --query '{jobId:jobId,status:status}' --output table

echo
log "watch it:  scripts/watch-job.sh $([ "$TARGET_TYPE" = thing ] && echo "$TARGET" || echo "$THING_NAME") $JOB_ID"
