#!/usr/bin/env bash
# Build a firmware fixture (vGOOD or vBAD) for a given backend + version.
#
#   scripts/build-fixture.sh <backend> good 1.0.0   # initial healthy image
#   scripts/build-fixture.sh <backend> good 2.0.0   # happy-path OTA target (commits)
#   scripts/build-fixture.sh <backend> bad  2.0.0   # rollback OTA target (fails self-test)
#
# <backend> = mqtt | https | jobs | manual   (the build env / OTA backend)
# Output: fixtures/firmware-<backend>-<variant>-v<version>.bin
set -euo pipefail
cd "$(dirname "$0")/.."

env="${1:?usage: build-fixture.sh <mqtt|https|jobs|manual> good|bad MAJOR.MINOR.BUILD}"
variant="${2:?need good|bad}"
version="${3:?need a version like 2.0.0}"

case "$env" in mqtt|https|jobs|manual) ;; *) echo "backend must be mqtt|https|jobs|manual"; exit 1 ;; esac
case "$variant" in good) PASS=1 ;; bad) PASS=0 ;; *) echo "variant must be good|bad"; exit 1 ;; esac

IFS=. read -r MAJ MIN BLD <<< "$version"
: "${MAJ:?}" "${MIN:?}" "${BLD:?}"

PIO="${PIO:-pio}"
command -v "$PIO" >/dev/null 2>&1 || PIO="$HOME/.platformio/penv/bin/pio"

export PLATFORMIO_BUILD_FLAGS="-DAPP_VERSION_MAJOR=$MAJ -DAPP_VERSION_MINOR=$MIN -DAPP_VERSION_BUILD=$BLD -DFW_SELFTEST_SHOULD_PASS=$PASS"
echo ">> building '$env/$variant' v$version"
echo ">> $PLATFORMIO_BUILD_FLAGS"

# Recompile the app sources (which read the version/variant macros) while keeping
# the heavy ESP-IDF / esp-aws-iot objects cached.
rm -rf ".pio/build/$env/src"
"$PIO" run -e "$env"

mkdir -p fixtures
out="fixtures/firmware-${env}-${variant}-v${version}.bin"
cp ".pio/build/$env/firmware.bin" "$out"
echo ">> wrote $out"
