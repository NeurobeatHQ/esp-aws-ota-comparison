#!/usr/bin/env bash
# Shared helpers for the composite OTA scripts. The backend mirrors the firmware
# env: BACKEND=mqtt|https|jobs|manual (default mqtt). Source this from the others.
set -euo pipefail

BACKEND="${BACKEND:-mqtt}"
case "$BACKEND" in mqtt|https|jobs|manual) ;; *)
  echo "BACKEND must be mqtt|https|jobs|manual (got '$BACKEND')" >&2; exit 1 ;;
esac

# One stack swapped in place by BACKEND (matches bin/app.ts). THING_NAME override
# -> a distinct stack, so parallel backends are possible without colliding.
THING_NAME_HINT="${THING_NAME:-esp32-ota-poc-01}"
if [ "$THING_NAME_HINT" = "esp32-ota-poc-01" ]; then
  STACK_NAME="${STACK_NAME:-Esp32OtaStack}"
else
  STACK_NAME="${STACK_NAME:-Esp32OtaStack-${THING_NAME_HINT}}"
fi
SIGNING_PROFILE_NAME="${SIGNING_PROFILE_NAME:-esp32_ota_${BACKEND}_profile}"
AWS_REGION="${AWS_REGION:-$(aws configure get region 2>/dev/null || echo us-east-1)}"
export AWS_REGION AWS_DEFAULT_REGION="$AWS_REGION"

# Which capabilities this backend uses (kept in sync with lib/ota-stack.ts).
backend_uses_jobs()   { case "$BACKEND" in mqtt|https|jobs) return 0 ;; *) return 1 ;; esac; }
backend_uses_signer() { case "$BACKEND" in mqtt|https)      return 0 ;; *) return 1 ;; esac; }

need() { command -v "$1" >/dev/null 2>&1 || { echo "error: '$1' not found on PATH" >&2; exit 1; }; }
need aws; need jq

# Fail fast — with the REAL reason — when AWS credentials are missing or expired.
# Without this the first AWS call dies inside cfn_output() (whose stderr goes to
# /dev/null), and load_stack() then misreports it as "deploy the stack first".
# On success ACCOUNT holds the 12-digit account id, for callers to reuse.
aws_preflight() {
  ACCOUNT="$(aws sts get-caller-identity --query Account --output text 2>&1)" || true
  if ! printf '%s' "$ACCOUNT" | grep -qE '^[0-9]{12}$'; then
    {
      echo "error: AWS credentials are missing or expired (region '$AWS_REGION')."
      printf '%s\n' "$ACCOUNT" | sed '/^[[:space:]]*$/d; s/^/  aws: /'
      echo "  -> Authenticate, then re-run. The line above names the command for your"
      echo "     setup (commonly: aws login | aws sso login | export AWS_PROFILE=<p>)."
    } >&2
    exit 1
  fi
}

cfn_output() {
  aws cloudformation describe-stacks --stack-name "$STACK_NAME" \
    --query "Stacks[0].Outputs[?OutputKey=='$1'].OutputValue" --output text 2>/dev/null
}

load_stack() {
  aws_preflight   # clear "you're not logged in" message before the silent cfn_output calls
  THING_NAME="$(cfn_output ThingName)"
  THING_GROUP_NAME="$(cfn_output ThingGroupName)"
  POLICY_NAME="$(cfn_output PolicyName)"
  BUCKET="$(cfn_output FirmwareBucketName)"
  OTA_ROLE_ARN="$(cfn_output OtaServiceRoleArn)"   # empty for jobs/manual (no such output)
  if [ -z "${THING_NAME:-}" ] || [ "$THING_NAME" = "None" ]; then
    echo "error: no outputs for stack '$STACK_NAME'." >&2
    echo "       Deploy it first:  BACKEND=$BACKEND npx cdk deploy" >&2
    exit 1
  fi
}

log() { printf '\033[1;34m>>\033[0m %s\n' "$*"; }
