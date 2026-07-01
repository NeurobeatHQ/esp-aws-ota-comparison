#!/usr/bin/env bash
# provision-secure-cert.sh — write the device cert + private key into the on-flash
# `esp_secure_cert` partition. The firmware always reads its identity (cert + key +
# Thing name, taken from the cert's CN) from that partition — nothing device-specific
# is embedded in the image, so ONE build serves the whole fleet.
#
# Usage:
#   scripts/provision-secure-cert.sh [CLIENT_CERT CLIENT_KEY] [-p PORT] [--ds]
#     CLIENT_CERT  path to THIS board's device cert (issued CN=<thing>).
#     CLIENT_KEY   path to the matching private key.
#                  Both default to src/certs/client.crt + src/certs/client.key when
#                  omitted. Pass them per board to provision a fleet without copying
#                  each board's files into src/certs/ first.
#     -p PORT      serial port of the board (default: $ESPPORT, else esptool auto-detect)
#     --ds         wrap the private key with the DS peripheral. This BURNS an HMAC eFuse
#                  (IRREVERSIBLE) and requires an RSA key. Default: plaintext key in the
#                  partition (no eFuse, reversible).
#
# Prereqs:
#   * Run a build once (e.g. `pio run -e https`) so the component manager fetches
#     espressif/esp_secure_cert_mgr into managed_components/ (that ships the tool).
#   * A device cert + key from the BYO-CA flow (see aws-iot/README.md "Fleet"); the
#     cert's CN is the Thing name. Pass them as args, or drop them at src/certs/client.*.
#   * The tool's Python deps install automatically into a local .provision-venv on first
#     run (needs python3 + one-time network). Set PYTHON=/path/to/python to use your own.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ESP_DIR="$(dirname "$HERE")"
CERT_DIR="$ESP_DIR/src/certs"
# Read the esp_secure_cert offset straight from partitions.csv so the two can never
# drift apart (a wrong offset writes the cert TLV to the wrong flash region, and the
# board then boots with no identity -> offline). partitions.csv col 4 = Offset.
PART_OFFSET="$(awk -F, '$1 ~ /^[[:space:]]*esp_secure_cert[[:space:]]*$/ {gsub(/[[:space:]]/,"",$4); print $4}' "$ESP_DIR/partitions.csv")"
[ -n "$PART_OFFSET" ] || { echo "ERROR: no esp_secure_cert row in $ESP_DIR/partitions.csv" >&2; exit 1; }
TARGET="esp32s3"
SECTYPE="cust_flash_tlv"        # current (non-legacy) format; read by esp_secure_cert_read
PORT="${ESPPORT:-}"
DS_ARGS=()
CERT=""
KEY=""

while [ $# -gt 0 ]; do
  case "$1" in
    -p|--port) PORT="${2:?-p needs a value}"; shift 2 ;;
    --ds)      DS_ARGS=(--configure_ds --priv_key_algo RSA 2048); shift ;;
    -h|--help) awk 'NR>1 && /^set -euo/{exit} NR>1{sub(/^# ?/,""); print}' "$0"; exit 0 ;;
    -*)        echo "unknown option: $1 (try --help)" >&2; exit 2 ;;
    *)
      if   [ -z "$CERT" ]; then CERT="$1"
      elif [ -z "$KEY"  ]; then KEY="$1"
      else echo "unexpected extra argument: $1 (try --help)" >&2; exit 2
      fi
      shift ;;
  esac
done

# Default to the committed placeholders under src/certs/ when not passed explicitly.
CERT="${CERT:-$CERT_DIR/client.crt}"
KEY="${KEY:-$CERT_DIR/client.key}"

TOOL="$(find "$ESP_DIR/managed_components" -name configure_esp_secure_cert.py 2>/dev/null | head -1 || true)"
if [ -z "$TOOL" ]; then
  echo "ERROR: configure_esp_secure_cert.py not found." >&2
  echo "       Build once first (e.g. 'pio run -e https') so the component manager" >&2
  echo "       fetches espressif/esp_secure_cert_mgr into managed_components/." >&2
  exit 1
fi

[ -f "$CERT" ] || { echo "ERROR: device cert not found: $CERT (mint it via the BYO-CA flow: aws-iot/README 'Fleet')" >&2; exit 1; }
[ -f "$KEY"  ] || { echo "ERROR: private key not found: $KEY" >&2; exit 1; }
[ -f "$CERT_DIR/AmazonRootCA1.pem" ] || { echo "ERROR: missing $CERT_DIR/AmazonRootCA1.pem" >&2; exit 1; }

# The tool needs its own Python deps (cryptography, esptool, esp-idf-nvs-partition-gen,
# construct — see $TOOL's requirements.txt). Keep them in a DEDICATED venv so we touch
# neither PlatformIO's penv nor the system Python (installing them into penv risks version
# conflicts with PlatformIO core). Created + populated once, reused after. Override with
# PYTHON=/path/to/python to use your own interpreter (then those deps are your job).
if [ -n "${PYTHON:-}" ]; then
  PY="$PYTHON"
else
  VENV="$ESP_DIR/.provision-venv"
  PY="$VENV/bin/python"
  REQS="$(dirname "$TOOL")/requirements.txt"
  [ -x "$PY" ] || { echo "Creating provisioning venv at $VENV ..."; python3 -m venv "$VENV"; }
  if ! "$PY" -c 'import esp_idf_nvs_partition_gen, cryptography, esptool, construct' 2>/dev/null; then
    echo "Installing provisioning-tool requirements into the venv (one-time) ..."
    "$PY" -m pip install --quiet --upgrade pip
    # Pin the two deps that break the tool when unpinned: cryptography >=45 removed
    # serialization.Encoding.value (the tool relies on it -> "0 of 3 entries"), and
    # esptool 5.x changed the CLI the tool shells out to. 4.x + cryptography 4x.x match
    # ESP-IDF 5.3.1.
    "$PY" -m pip install --quiet -r "$REQS" 'cryptography<45' 'esptool<5'
  fi
fi

echo "Provisioning esp_secure_cert @ $PART_OFFSET on $TARGET (${DS_ARGS:+DS peripheral}${DS_ARGS:-plaintext key})"
echo "  device cert: $CERT"
echo "  private key: $KEY"

# Build argv incrementally. macOS ships bash 3.2, where "${arr[@]}" on an EMPTY array
# under `set -u` throws "unbound variable" (not fixed until bash 4.4). So append the
# optional groups only when non-empty — never expand a possibly-empty array.
CMD=("$PY" "$TOOL"
  --device-cert "$CERT"
  --private-key "$KEY"
  --ca-cert     "$CERT_DIR/AmazonRootCA1.pem"
  --target_chip "$TARGET"
  --secure_cert_type "$SECTYPE"
  --sec_cert_part_offset "$PART_OFFSET")
[ "${#DS_ARGS[@]}" -gt 0 ] && CMD+=("${DS_ARGS[@]}")
[ -n "$PORT" ] && CMD+=(-p "$PORT")

# The tool shells out to `esptool.py` for the actual flash; put the interpreter's bin
# dir (where pip installed esptool.py) on PATH so that subprocess resolves it.
set -x
PATH="$(dirname "$PY"):$PATH" "${CMD[@]}"
