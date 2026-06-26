#!/usr/bin/env bash
# Shared helpers for the OTA action scripts. Resolves CDK stack outputs so the
# scripts need no hand-maintained config. Source this from the other scripts.
set -euo pipefail

STACK_NAME="${STACK_NAME:-Esp32OtaJobsStack}"
AWS_REGION="${AWS_REGION:-$(aws configure get region 2>/dev/null || echo us-east-1)}"
export AWS_REGION AWS_DEFAULT_REGION="$AWS_REGION"

need() { command -v "$1" >/dev/null 2>&1 || { echo "error: '$1' not found on PATH" >&2; exit 1; }; }
need aws; need jq

# cfn_output <OutputKey> — read a CloudFormation output from the stack.
cfn_output() {
  aws cloudformation describe-stacks --stack-name "$STACK_NAME" \
    --query "Stacks[0].Outputs[?OutputKey=='$1'].OutputValue" --output text 2>/dev/null
}

# Resolve the common values lazily (callers use these vars).
load_stack() {
  THING_NAME="$(cfn_output ThingName)"
  THING_GROUP_NAME="$(cfn_output ThingGroupName)"
  POLICY_NAME="$(cfn_output PolicyName)"
  BUCKET="$(cfn_output FirmwareBucketName)"
  if [ -z "${THING_NAME:-}" ] || [ "$THING_NAME" = "None" ]; then
    echo "error: could not read outputs from stack '$STACK_NAME'." >&2
    echo "       Deploy it first:  (cd aws-iot && npm install && cdk deploy)" >&2
    exit 1
  fi
}

log() { printf '\033[1;34m>>\033[0m %s\n' "$*"; }
