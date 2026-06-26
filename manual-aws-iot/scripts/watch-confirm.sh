#!/usr/bin/env bash
# Variant A — watch for the device's OTA confirmation on dt/<thing>/ota/confirm.
#
#   scripts/watch-confirm.sh <thing-name>
#
# AWS IoT has no plain-CLI MQTT subscribe, so this uses the AWS IoT Device SDK
# over a WebSocket with SigV4 (your IAM creds — no extra cert). Install once:
#   pip install awsiotsdk
# If it isn't available, subscribe to dt/<thing>/ota/confirm in the AWS IoT
# console "MQTT test client" instead.
set -euo pipefail
cd "$(dirname "$0")"
. ./_lib.sh
load_stack

THING="${1:-$THING_NAME}"
ENDPOINT="$(aws iot describe-endpoint --endpoint-type iot:Data-ATS --query endpointAddress --output text)"

if ! python3 -c "import awscrt, awsiot" 2>/dev/null; then
  echo "awsiotsdk not installed.  pip install awsiotsdk   (or use the console MQTT test client)"
  echo "Topic to watch:  dt/$THING/ota/confirm"
  exit 1
fi

log "subscribing to dt/$THING/ota/confirm  (endpoint $ENDPOINT, SigV4 WebSocket; Ctrl-C to stop)"
AWS_REGION="$AWS_REGION" ENDPOINT="$ENDPOINT" THING="$THING" python3 - <<'PY'
import os, time
from awscrt import mqtt
from awsiot import mqtt_connection_builder
conn = mqtt_connection_builder.websockets_with_default_aws_signing(
    endpoint=os.environ["ENDPOINT"], region=os.environ["AWS_REGION"],
    client_id=f"watch-confirm-{int(time.time())}")
conn.connect().result()
topic = f"dt/{os.environ['THING']}/ota/confirm"
def on_msg(topic, payload, **kw):
    print(f"\033[1;32m{topic}\033[0m  {payload.decode()}")
conn.subscribe(topic=topic, qos=mqtt.QoS.AT_LEAST_ONCE, callback=on_msg)[0].result()
print(f"subscribed to {topic} — waiting...")
while True:
    time.sleep(1)
PY
