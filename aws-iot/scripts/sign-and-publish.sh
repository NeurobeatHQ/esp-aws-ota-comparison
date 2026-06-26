#!/usr/bin/env bash
# Upload a firmware build to the (versioned) S3 bucket. The actual AWS Signer
# signing happens inside create-ota-update (push-ota.sh) via the signing profile.
#
#   scripts/sign-and-publish.sh <firmware.bin> <version>
#   scripts/sign-and-publish.sh ../esp/fixtures/firmware-good-v2.0.0.bin 2.0.0
set -euo pipefail
cd "$(dirname "$0")"
. ./_lib.sh
load_stack

BIN="${1:?usage: sign-and-publish.sh <firmware.bin> <version>}"
VERSION="${2:?need a version label, e.g. 2.0.0}"
[ -f "$BIN" ] || { echo "error: no such file: $BIN" >&2; exit 1; }

KEY="firmware/${VERSION}/firmware.bin"
log "uploading $BIN -> s3://$BUCKET/$KEY"
aws s3api put-object --bucket "$BUCKET" --key "$KEY" --body "$BIN" >/dev/null
VERSION_ID="$(aws s3api head-object --bucket "$BUCKET" --key "$KEY" --query VersionId --output text)"

log "uploaded. S3 object version: $VERSION_ID"
echo
echo "  next:  scripts/push-ota.sh <thing-or-group> $VERSION"
