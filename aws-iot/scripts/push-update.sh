#!/usr/bin/env bash
# Push an OTA update. Dispatches on the backend (all share the same S3 upload):
#   mqtt   -> aws iot create-ota-update --protocols MQTT   (Signer + Stream + Job)
#   https  -> aws iot create-ota-update --protocols HTTP   (Signer + presigned URL + Job)
#   jobs   -> aws iot create-job        (custom doc carrying a presigned S3 URL)
#   manual -> aws iot-data publish      (plan -> dt/<thing>/ota/plan; no Jobs)
#
#   BACKEND=<b> scripts/push-update.sh <thing-name> <version>
#   BACKEND=jobs scripts/push-update.sh -g <thing-group> <version>   (mqtt|https|jobs only)
set -euo pipefail
cd "$(dirname "$0")"
. ./_lib.sh
load_stack

TARGET_TYPE="thing"
if [ "${1:-}" = "-g" ]; then TARGET_TYPE="thing-group"; shift; fi
TARGET="${1:?usage: push-update.sh [-g] <thing-or-group> <version>}"
VERSION="${2:?need the version uploaded by upload-firmware.sh}"

ACCOUNT="$(aws sts get-caller-identity --query Account --output text)"
KEY="firmware/${VERSION}/firmware.bin"
thing_arn()  { echo "arn:aws:iot:${AWS_REGION}:${ACCOUNT}:thing/${1}"; }
group_arn()  { echo "arn:aws:iot:${AWS_REGION}:${ACCOUNT}:thinggroup/${1}"; }
target_arn() { [ "$TARGET_TYPE" = "thing-group" ] && group_arn "$TARGET" || thing_arn "$TARGET"; }

case "$BACKEND" in
  mqtt|https)
    PROTO=$([ "$BACKEND" = "https" ] && echo HTTP || echo MQTT)
    VERSION_ID="$(aws s3api head-object --bucket "$BUCKET" --key "$KEY" --query VersionId --output text)" \
      || { echo "error: $KEY not in s3://$BUCKET — run upload-firmware.sh first" >&2; exit 1; }
    OTA_ID="esp32-ota-${BACKEND}-${VERSION//./-}-$(date +%s)"
    FILES="$(jq -nc --arg b "$BUCKET" --arg k "$KEY" --arg v "$VERSION_ID" --arg p "$SIGNING_PROFILE_NAME" '
      [{fileName:"/firmware.bin", fileVersion:"1",
        fileLocation:{s3Location:{bucket:$b, key:$k, version:$v}},
        codeSigning:{startSigningJobParameter:{signingProfileName:$p, destination:{s3Destination:{bucket:$b}}}}}]')"
    PRESIGN=""
    [ "$BACKEND" = "https" ] && PRESIGN="--aws-job-presigned-url-config {\"expiresInSec\":3600,\"roleArn\":\"$OTA_ROLE_ARN\"}"
    log "[$BACKEND] create-ota-update '$OTA_ID' (--protocols $PROTO) -> $(target_arn)"
    aws iot create-ota-update --ota-update-id "$OTA_ID" --description "ESP32 OTA $BACKEND v$VERSION" \
      --targets "$(target_arn)" --target-selection SNAPSHOT --protocols "$PROTO" \
      --files "$FILES" --role-arn "$OTA_ROLE_ARN" $PRESIGN \
      --aws-job-abort-config '{"abortCriteriaList":[{"failureType":"FAILED","action":"CANCEL","thresholdPercentage":50,"minNumberOfExecutedThings":1}]}' \
      --aws-job-timeout-config '{"inProgressTimeoutInMinutes":10}' \
      --query '{otaUpdateId:otaUpdateId,jobId:awsIotJobId,status:otaUpdateStatus}' --output table
    echo; log "watch:  BACKEND=$BACKEND scripts/watch.sh ${TARGET}   (Job id: AFR_OTA-$OTA_ID)"
    ;;

  jobs)
    URL="$(aws s3 presign "s3://$BUCKET/$KEY" --expires-in 3600)" \
      || { echo "error: presign failed — run upload-firmware.sh first" >&2; exit 1; }
    JOB_ID="esp32-ota-jobs-${VERSION//./-}-$(date +%s)"
    DOC="$(jq -nc --arg url "$URL" --arg ver "$VERSION" '{op:"ota", url:$url, target_version:$ver}')"
    log "[jobs] create-job '$JOB_ID' (custom doc) -> $(target_arn)"
    aws iot create-job --job-id "$JOB_ID" --targets "$(target_arn)" --document "$DOC" \
      --target-selection SNAPSHOT --timeout-config inProgressTimeoutInMinutes=10 \
      --query '{jobId:jobId,status:status}' --output table
    echo; log "watch:  BACKEND=jobs scripts/watch.sh ${TARGET} $JOB_ID"
    ;;

  manual)
    [ "$TARGET_TYPE" = "thing" ] || { echo "manual backend targets a single thing (no -g)"; exit 1; }
    URL="$(aws s3 presign "s3://$BUCKET/$KEY" --expires-in 3600)" \
      || { echo "error: presign failed — run upload-firmware.sh first" >&2; exit 1; }
    OTA_ID="ota-${VERSION//./-}-$(date +%s)"
    PLAN="$(jq -nc --arg url "$URL" --arg oid "$OTA_ID" --arg ver "$VERSION" \
      '{op:"ota", url:$url, ota_id:$oid, target_version:$ver}')"
    log "[manual] publish plan ota_id=$OTA_ID -> dt/$TARGET/ota/plan"
    aws iot-data publish --topic "dt/$TARGET/ota/plan" --qos 1 \
      --cli-binary-format raw-in-base64-out --payload "$PLAN"
    echo; log "watch:  BACKEND=manual scripts/watch.sh ${TARGET}"
    ;;
esac
