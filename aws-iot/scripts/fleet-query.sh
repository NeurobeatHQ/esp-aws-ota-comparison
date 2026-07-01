#!/usr/bin/env bash
# fleet-query.sh — query the AWS IoT fleet index (run scripts/enable-fleet-indexing.sh once
# first). Version is the integer swVersion the firmware reports (major*10000+minor*100+build),
# so v2.0.0 = 20000, v3.1.0 = 30100.
#
#   aws-iot/scripts/fleet-query.sh outdated        <int>   # things reporting swVersion < <int>
#   aws-iot/scripts/fleet-query.sh online-outdated <int>   # ... AND currently connected
#   aws-iot/scripts/fleet-query.sh version         <int>   # things at exactly this swVersion
#   aws-iot/scripts/fleet-query.sh seen-since      <epoch_ms>  # lastSeen newer than (AWS-stamped)
#   aws-iot/scripts/fleet-query.sh count           <int>   # how many are below <int>
#   aws-iot/scripts/fleet-query.sh raw '<query>'           # any fleet-index query string
#
# Output is the raw search-index JSON (pipe to `jq` to filter). The device shadow comes
# back as a string field; connectivity.{connected,timestamp} are top-level.
set -euo pipefail

cmd="${1:-}"
case "$cmd" in
  outdated)        Q="shadow.reported.swVersion < ${2:?need a version int, e.g. 20000}" ;;
  online-outdated) Q="connectivity.connected:true AND shadow.reported.swVersion < ${2:?need a version int}" ;;
  version)         Q="shadow.reported.swVersion = ${2:?need a version int}" ;;
  seen-since)      Q="shadow.reported.lastSeen > ${2:?need epoch milliseconds}" ;;
  raw)             Q="${2:?need a query string}" ;;
  count)
    aws iot get-statistics --query-string "shadow.reported.swVersion < ${2:?need a version int}"
    exit 0 ;;
  *)
    sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'
    exit 2 ;;
esac

aws iot search-index --query-string "$Q"
