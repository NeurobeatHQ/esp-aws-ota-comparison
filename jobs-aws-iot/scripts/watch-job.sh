#!/usr/bin/env bash
# Poll the OTA job execution status for a Thing until it reaches a terminal
# state (SUCCEEDED / FAILED / TIMED_OUT / REJECTED / CANCELED / REMOVED).
#
#   scripts/watch-job.sh [thing-name] [job-id]
#
# With no args it uses the stack's Thing and its most recent job execution.
set -euo pipefail
cd "$(dirname "$0")"
. ./_lib.sh
load_stack

THING="${1:-$THING_NAME}"
JOB_ID="${2:-}"

if [ -z "$JOB_ID" ]; then
  JOB_ID="$(aws iot list-job-executions-for-thing --thing-name "$THING" \
    --max-results 1 --query 'executionSummaries[0].jobId' --output text 2>/dev/null || true)"
  [ -n "$JOB_ID" ] && [ "$JOB_ID" != "None" ] || { echo "no job executions for $THING yet" >&2; exit 1; }
fi

log "watching job '$JOB_ID' on thing '$THING'  (Ctrl-C to stop)"
while true; do
  STATUS="$(aws iot describe-job-execution --job-id "$JOB_ID" --thing-name "$THING" \
    --query 'execution.status' --output text 2>/dev/null || echo UNKNOWN)"
  printf '%s  %s\n' "$(date +%H:%M:%S)" "$STATUS"
  case "$STATUS" in
    SUCCEEDED) echo "OTA committed on device ✅"; exit 0 ;;
    FAILED|TIMED_OUT|REJECTED|CANCELED|REMOVED)
      echo "OTA did not commit — device should have rolled back to the previous image."
      aws iot describe-job-execution --job-id "$JOB_ID" --thing-name "$THING" \
        --query 'execution.statusDetails' --output json 2>/dev/null || true
      exit 2 ;;
  esac
  sleep 5
done
