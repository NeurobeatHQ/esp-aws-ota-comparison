#!/usr/bin/env bash
# Shared helpers for the esp/ flashing + provisioning scripts. Single source of truth
# for (a) the partitions.csv column layout and (b) the PlatformIO tool paths, so the
# offset/size parser and the ~/.platformio locations aren't copy-pasted across
# flash-full.sh / flash-main-app.sh / provision-secure-cert.sh.
#
# Source it, don't run it:  . "$(dirname "$0")/_partlib.sh"
# Callers are expected to be under `set -euo pipefail`.

# Absolute path to esp/partitions.csv, resolved relative to THIS file (not the caller's
# cwd), so it works no matter where the sourcing script cd'd to. Guard BASH_SOURCE for
# bash 3.2 under `set -u`, where an unset array element throws "unbound variable".
_PARTLIB_SELF="${BASH_SOURCE[0]:-$0}"
_PARTLIB_DIR="$(cd "$(dirname "$_PARTLIB_SELF")" && pwd)"
PARTITIONS_CSV="${PARTITIONS_CSV:-$(cd "$_PARTLIB_DIR/.." && pwd)/partitions.csv}"

# partitions.csv columns:  Name(1), Type(2), SubType(3), Offset(4), Size(5), Flags(6)
# part_off NAME  -> the Offset (col 4) of the partition named NAME, e.g. 0xB0000
# part_size NAME -> the Size   (col 5), e.g. 0x190000
# Empty output (and success) when NAME isn't in the table — callers check for that.
part_off()  { awk -F, -v n="$1" '{gsub(/[ \t]/,"",$1)} $1==n {gsub(/[ \t]/,"",$4); print $4}' "$PARTITIONS_CSV"; }
part_size() { awk -F, -v n="$1" '{gsub(/[ \t]/,"",$1)} $1==n {gsub(/[ \t]/,"",$5); print $5}' "$PARTITIONS_CSV"; }

# PlatformIO-installed tools (populated by a `pio run`). Single definition so the two
# flash scripts can't drift. Guard existence here so callers fail with one clear message
# instead of a confusing "python: No such file" mid-flash.
PY="$HOME/.platformio/penv/bin/python"
ESPTOOL="$HOME/.platformio/packages/tool-esptoolpy/esptool.py"
OTATOOL="$HOME/.platformio/packages/framework-espidf/components/app_update/otatool.py"

# require_flash_tools — bail out unless the PlatformIO python + tools are present.
require_flash_tools() {
  [ -x "$PY" ] || { echo "error: PlatformIO python not found at $PY — build first (pio run) or install PlatformIO" >&2; exit 1; }
  [ -f "$ESPTOOL" ] || { echo "error: esptool.py not found at $ESPTOOL — build first (pio run)" >&2; exit 1; }
  [ -f "$OTATOOL" ] || { echo "error: otatool.py not found at $OTATOOL — build first (pio run)" >&2; exit 1; }
}

# switch_to_ota0 <port> — point otadata at ota_0 via IDF's otatool (correct CRC, no
# hand-rolled blob) so the bootloader boots the main app instead of factory.
switch_to_ota0() {
  local port="${1:?switch_to_ota0 needs a serial port}"
  "$PY" "$OTATOOL" -p "$port" --partition-table-file "$PARTITIONS_CSV" switch_ota_partition --slot 0
}
