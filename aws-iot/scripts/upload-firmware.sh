#!/usr/bin/env bash
# Upload a firmware build to the (versioned) S3 bucket. Shared by all backends.
#
#   BACKEND=<b> scripts/upload-firmware.sh <firmware.bin> <version>
#   BACKEND=jobs scripts/upload-firmware.sh ../esp/fixtures/firmware-jobs-good-v2.0.0.bin 2.0.0
set -euo pipefail
cd "$(dirname "$0")"
. ./_lib.sh
load_stack

BIN="${1:?usage: upload-firmware.sh <firmware.bin> <version>}"
VERSION="${2:?need a version label, e.g. 2.0.0}"
[ -f "$BIN" ] || { echo "error: no such file: $BIN" >&2; exit 1; }

KEY="firmware/${VERSION}/firmware.bin"
log "[$BACKEND] uploading $BIN -> s3://$BUCKET/$KEY"
aws s3api put-object --bucket "$BUCKET" --key "$KEY" --body "$BIN" >/dev/null
log "uploaded."
echo
echo "  next:  BACKEND=$BACKEND scripts/push-update.sh $THING_NAME $VERSION"
