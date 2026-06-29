#!/usr/bin/env bash
# Watch an OTA's progress. Dispatches on the backend:
#   mqtt|https|jobs -> poll the IoT Job execution to a terminal state
#   manual          -> subscribe to dt/<thing>/ota/confirm (the device's report)
#
#   BACKEND=<b> scripts/watch.sh [thing] [jobId]
set -euo pipefail
cd "$(dirname "$0")"
. ./_lib.sh
load_stack

THING="${1:-$THING_NAME}"

if backend_uses_jobs; then
  JOB_ID="${2:-}"
  if [ -z "$JOB_ID" ]; then
    JOB_ID="$(aws iot list-job-executions-for-thing --thing-name "$THING" --max-results 1 \
      --query 'executionSummaries[0].jobId' --output text 2>/dev/null || true)"
    [ -n "$JOB_ID" ] && [ "$JOB_ID" != "None" ] || { echo "no job executions for $THING yet" >&2; exit 1; }
  fi
  log "[$BACKEND] watching job '$JOB_ID' on '$THING'  (Ctrl-C to stop)"
  while true; do
    STATUS="$(aws iot describe-job-execution --job-id "$JOB_ID" --thing-name "$THING" \
      --query 'execution.status' --output text 2>/dev/null || echo UNKNOWN)"
    printf '%s  %s\n' "$(date +%H:%M:%S)" "$STATUS"
    case "$STATUS" in
      SUCCEEDED) echo "OTA committed on device ✅"; exit 0 ;;
      FAILED|TIMED_OUT|REJECTED|CANCELED|REMOVED)
        echo "OTA did not commit — device should have rolled back."; exit 2 ;;
    esac
    sleep 5
  done
else
  # manual backend: no IoT Job; the device reports on dt/<thing>/ota/confirm.
  ENDPOINT="$(aws iot describe-endpoint --endpoint-type iot:Data-ATS --query endpointAddress --output text)"
  if ! python3 -c "import awscrt, awsiot" 2>/dev/null; then
    echo "awsiotsdk not installed.  pip install awsiotsdk   (or use the console MQTT test client)"
    echo "Topic to watch:  dt/$THING/ota/confirm"
    exit 1
  fi
  log "[manual] subscribing to dt/$THING/ota/confirm  (SigV4 WebSocket; Ctrl-C to stop)"
  AWS_REGION="$AWS_REGION" ENDPOINT="$ENDPOINT" THING="$THING" python3 - <<'PY'
import os, time
from awscrt import mqtt
from awsiot import mqtt_connection_builder
conn = mqtt_connection_builder.websockets_with_default_aws_signing(
    endpoint=os.environ["ENDPOINT"], region=os.environ["AWS_REGION"],
    client_id=f"watch-confirm-{int(time.time())}")
conn.connect().result()
topic = f"dt/{os.environ['THING']}/ota/confirm"
conn.subscribe(topic=topic, qos=mqtt.QoS.AT_LEAST_ONCE,
               callback=lambda topic, payload, **kw: print(f"\033[1;32m{topic}\033[0m  {payload.decode()}"))[0].result()
print(f"subscribed to {topic} — waiting...")
while True:
    time.sleep(1)
PY
fi
