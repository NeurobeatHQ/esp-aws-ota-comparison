#!/usr/bin/env bash
# Dev-flash the MAIN app to ota_0 on the unified factory+ota table, and make it boot.
#
# Why this exists: `factory` holds the small recovery image; the main app is too big
# for it and lives in ota_0. `pio run -t upload` would write the app to factory (and
# overrun it), and the default otadata boots factory — so a plain upload does NOT run
# the main app. This writes the app to ota_0 and runs IDF's otatool to point otadata
# at ota_0 (correct CRC, no hand-rolled blob), so the bootloader boots the main app.
#
#   esp/scripts/flash-main-app.sh <env> <port>
#   e.g.  esp/scripts/flash-main-app.sh https /dev/cu.usbmodem21201
#
# Build first:  (cd esp && pio run -e <env>)
# Board must be in download mode — on the RP2350 bench, drive bootload/reset with the
# esp-jumpstart skill around this (or run on a direct-USB board, which auto-resets).
set -euo pipefail
ENV="${1:?usage: flash-main-app.sh <env> <port>}"
PORT="${2:?need the serial port, e.g. /dev/cu.usbmodem21201}"
cd "$(dirname "$0")/.."          # -> esp/

BUILD=".pio/build/$ENV"
[ -f "$BUILD/firmware.bin" ] || { echo "error: no build for '$ENV' — run: pio run -e $ENV" >&2; exit 1; }

PY="$HOME/.platformio/penv/bin/python"
ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
OTATOOL="$HOME/.platformio/packages/framework-espidf/components/app_update/otatool.py"

# Offsets come from partitions.csv so this tracks the table, not hard-coded values.
off() { awk -F, -v n="$1" '{gsub(/[ \t]/,"",$1)} $1==n {gsub(/[ \t]/,"",$4); print $4}' partitions.csv; }
OTADATA_OFF="$(off otadata)"
OTA0_OFF="$(off ota_0)"
[ -n "$OTA0_OFF" ] || { echo "error: no ota_0 in partitions.csv (is this the unified table?)" >&2; exit 1; }

echo ">> flash: bootloader@0x0, partitions@0x8000, blank otadata@$OTADATA_OFF, app@ota_0=$OTA0_OFF"
"$PY" "$ESPTOOL" --chip esp32s3 -p "$PORT" write_flash \
  0x0            "$BUILD/bootloader.bin" \
  0x8000         "$BUILD/partitions.bin" \
  "$OTADATA_OFF" "$BUILD/ota_data_initial.bin" \
  "$OTA0_OFF"    "$BUILD/firmware.bin"

echo ">> otatool: point otadata at ota_0 so the bootloader boots the main app"
"$PY" "$OTATOOL" -p "$PORT" --partition-table-file partitions.csv switch_ota_partition --slot 0

echo ">> done — reset the board; it boots the main app from ota_0 (factory stays the recovery image)."
