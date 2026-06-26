#!/usr/bin/env bash
# Variant B — upload a firmware build to the (versioned) S3 bucket. No AWS Signer:
# the device verifies authenticity via TLS-to-S3 (+ Secure Boot). create-job.sh
# presigns the GET URL when it creates the job.
#
#   scripts/presign-and-upload.sh <firmware.bin> <version>
#   scripts/presign-and-upload.sh ../jobs-esp/fixtures/firmware-good-v2.0.0.bin 2.0.0
set -euo pipefail
cd "$(dirname "$0")"
. ./_lib.sh
load_stack

BIN="${1:?usage: presign-and-upload.sh <firmware.bin> <version>}"
VERSION="${2:?need a version label, e.g. 2.0.0}"
[ -f "$BIN" ] || { echo "error: no such file: $BIN" >&2; exit 1; }

KEY="firmware/${VERSION}/firmware.bin"
log "uploading $BIN -> s3://$BUCKET/$KEY"
aws s3api put-object --bucket "$BUCKET" --key "$KEY" --body "$BIN" >/dev/null

log "uploaded. (create-job.sh presigns the GET URL at job-creation time.)"
echo
echo "  next:  scripts/create-job.sh <thing-or-group> $VERSION"
