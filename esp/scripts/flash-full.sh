#!/usr/bin/env bash
# One-shot "program a blank board end to end": bootloader + partition table + the
# factory/recovery image + the main app in ota_0 + otadata, in a single esptool call.
# Use this at manufacturing / first-flash time; use flash-main-app.sh for updating
# just the main app later (it never touches factory).
#
#   esp/scripts/flash-full.sh <factory.bin> <env> <port>
#   e.g.  esp/scripts/flash-full.sh ~/factory/recovery.bin https /dev/cu.usbmodem21201
#
# Build the main app first:  (cd esp && pio run -e <env>   [+ PLATFORMIO_BUILD_FLAGS])
# Board must be in download mode (RP2350 bridge / esp-jumpstart, or a direct-USB board).
#
# The bootloader + partition table come from the MAIN app's build, so the factory
# project MUST share the same partitions.csv + bootloader-affecting sdkconfig — that's
# the whole premise of the unified table.
set -euo pipefail
FACTORY="${1:?usage: flash-full.sh <factory.bin> <env> <port>}"
ENV="${2:?need the main-app env, e.g. https}"
PORT="${3:?need the serial port}"
cd "$(dirname "$0")/.."          # -> esp/

BUILD=".pio/build/$ENV"
[ -f "$BUILD/firmware.bin" ] || { echo "error: build the main app first: pio run -e $ENV" >&2; exit 1; }
[ -f "$FACTORY" ]           || { echo "error: no such factory image: $FACTORY" >&2; exit 1; }

PY="$HOME/.platformio/penv/bin/python"
ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
OTATOOL="$HOME/.platformio/packages/framework-espidf/components/app_update/otatool.py"

off()  { awk -F, -v n="$1" '{gsub(/[ \t]/,"",$1)} $1==n {gsub(/[ \t]/,"",$4); print $4}' partitions.csv; }
size() { awk -F, -v n="$1" '{gsub(/[ \t]/,"",$1)} $1==n {gsub(/[ \t]/,"",$5); print $5}' partitions.csv; }
FACT_OFF="$(off factory)"; OTA0_OFF="$(off ota_0)"; OTADATA_OFF="$(off otadata)"
[ -n "$FACT_OFF" ] && [ -n "$OTA0_OFF" ] || { echo "error: table has no factory/ota_0 (unified table expected)" >&2; exit 1; }

# Sanity: the factory image must fit the factory slot.
FSZ=$(wc -c < "$FACTORY"); FCAP=$(printf '%d' "$(size factory)")
[ "$FSZ" -le "$FCAP" ] || { echo "error: factory image $FSZ B > factory slot $FCAP B" >&2; exit 1; }

echo ">> full flash: bootloader@0x0, table@0x8000, factory@$FACT_OFF, main app@ota_0=$OTA0_OFF, otadata@$OTADATA_OFF"
"$PY" "$ESPTOOL" --chip esp32s3 -p "$PORT" write_flash \
  0x0            "$BUILD/bootloader.bin" \
  0x8000         "$BUILD/partitions.bin" \
  "$FACT_OFF"    "$FACTORY" \
  "$OTADATA_OFF" "$BUILD/ota_data_initial.bin" \
  "$OTA0_OFF"    "$BUILD/firmware.bin"

echo ">> otadata -> ota_0 (boot the MAIN app). Delete the next command to boot FACTORY instead"
echo "   (factory then bootstraps the app over OTA — the production first-boot flow)."
"$PY" "$OTATOOL" -p "$PORT" --partition-table-file partitions.csv switch_ota_partition --slot 0

echo ">> done — reset the board. Boots the main app from ota_0; factory holds the recovery image."
