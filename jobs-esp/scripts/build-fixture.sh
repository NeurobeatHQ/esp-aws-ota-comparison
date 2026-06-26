#!/usr/bin/env bash
# Build a firmware fixture (vGOOD or vBAD) at a given version.
#
#   scripts/build-fixture.sh good 1.0.0      # initial healthy image to flash
#   scripts/build-fixture.sh good 2.0.0      # happy-path OTA target (commits)
#   scripts/build-fixture.sh bad  2.0.0      # rollback OTA target (fails self-test)
#
# Output: fixtures/firmware-<variant>-v<version>.bin
set -euo pipefail
cd "$(dirname "$0")/.."

variant="${1:?usage: build-fixture.sh good|bad MAJOR.MINOR.BUILD [env]}"
version="${2:?need a version like 2.0.0}"
env="${3:-feather_s3}"

IFS=. read -r MAJ MIN BLD <<< "$version"
: "${MAJ:?}" "${MIN:?}" "${BLD:?}"

case "$variant" in
  good) PASS=1 ;;
  bad)  PASS=0 ;;
  *) echo "variant must be 'good' or 'bad'"; exit 1 ;;
esac

PIO="${PIO:-pio}"
command -v "$PIO" >/dev/null 2>&1 || PIO="$HOME/.platformio/penv/bin/pio"

export PLATFORMIO_BUILD_FLAGS="-DAPP_VERSION_MAJOR=$MAJ -DAPP_VERSION_MINOR=$MIN -DAPP_VERSION_BUILD=$BLD -DFW_SELFTEST_SHOULD_PASS=$PASS"
echo ">> building '$variant' v$version  (env=$env)"
echo ">> $PLATFORMIO_BUILD_FLAGS"

# Force the app sources (which read the version/variant macros) to recompile,
# while keeping the heavy ESP-IDF / esp-aws-iot objects cached.
rm -rf ".pio/build/$env/src"
"$PIO" run -e "$env"

mkdir -p fixtures
out="fixtures/firmware-${variant}-v${version}.bin"
cp ".pio/build/$env/firmware.bin" "$out"
echo ">> wrote $out"
