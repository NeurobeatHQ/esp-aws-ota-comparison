#!/usr/bin/env bash
# enable-fleet-indexing.sh — turn on AWS IoT Fleet Indexing so the fleet can be queried
# by firmware version + connectivity (see scripts/fleet-query.sh). Account-level, one-time,
# idempotent (safe to re-run).
#
#   REGISTRY_AND_SHADOW          index registry data + the classic Device Shadow
#                                (the device reports swVersion into reported state).
#   thingConnectivityIndexing    STATUS -> connectivity.connected / .timestamp.
#   customFields swVersion/lastSeen as Number -> numeric `<` / `>=` range queries
#                                (a string field would sort "0.10" < "0.3"). lastSeen is
#                                written by the PresenceToShadow rule (cdk), AWS-timestamped.
#
# Notes:
#   * Connect/disconnect lifecycle events ($aws/events/presence/…) are published by the
#     broker by default — no separate enable step.
#   * Fleet indexing is BILLED per indexed device + per update; enable deliberately.
#   * After enabling, the initial index build can take a few minutes before queries return.
set -euo pipefail

aws sts get-caller-identity >/dev/null 2>&1 || {
  echo "ERROR: AWS credentials not usable (aws sts get-caller-identity failed)." >&2; exit 1;
}

aws iot update-indexing-configuration --thing-indexing-configuration '{
    "thingIndexingMode": "REGISTRY_AND_SHADOW",
    "thingConnectivityIndexingMode": "STATUS",
    "customFields": [
      { "name": "shadow.reported.swVersion", "type": "Number" },
      { "name": "shadow.reported.lastSeen",  "type": "Number" }
    ]
  }'

echo ">> fleet indexing enabled: registry + shadow + connectivity; swVersion/lastSeen as Number."
echo ">> initial index build can take a few minutes — then use: aws-iot/scripts/fleet-query.sh"
