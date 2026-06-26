#!/usr/bin/env bash
# Variant A — operator pushes an update "plan" to a device over the custom
# protocol: presign the S3 object and publish it to dt/<thing>/ota/plan.
# (This stands in for the custom server's reply to should-i-update.)
#
#   scripts/publish-plan.sh <thing-name> <version>
set -euo pipefail
cd "$(dirname "$0")"
. ./_lib.sh
load_stack

THING="${1:?usage: publish-plan.sh <thing-name> <version>}"
VERSION="${2:?need the version uploaded by presign-and-upload.sh}"

KEY="firmware/${VERSION}/firmware.bin"
URL="$(aws s3 presign "s3://$BUCKET/$KEY" --expires-in 3600)" \
  || { echo "error: presign failed — run presign-and-upload.sh first" >&2; exit 1; }
OTA_ID="ota-${VERSION//./-}-$(date +%s)"

PLAN="$(jq -nc --arg url "$URL" --arg oid "$OTA_ID" --arg ver "$VERSION" \
  '{op:"ota", url:$url, ota_id:$oid, target_version:$ver}')"

log "publishing plan ota_id=$OTA_ID -> dt/$THING/ota/plan"
aws iot-data publish \
  --topic "dt/$THING/ota/plan" \
  --qos 1 \
  --cli-binary-format raw-in-base64-out \
  --payload "$PLAN"
log "published. Watch for the device's confirm with:  scripts/watch-confirm.sh $THING"
