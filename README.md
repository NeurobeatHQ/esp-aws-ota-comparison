# ESP32-S3 · AWS IoT Core · Rollback-safe OTA (POC)

Rollback-safe OTA on an ESP32-S3 (Adafruit Feather) using the AWS-managed stack —
AWS IoT Jobs + MQTT File Streams + coreMQTT over mutual TLS — on pure ESP-IDF 5.3.x
(PlatformIO `framework = espidf`, no Arduino).

```
   ESP32-S3 ──mutual TLS──> AWS IoT Core ── Jobs (orchestration)
   coreMQTT                                └ MQTT File Streams (image, ECDSA-P256 signed)
      │
      └ download → verify signature → activate → reboot → self-test → commit / ROLLBACK
```

> The walkthrough uses **variant C** (`pio run -e mqtt` + `aws-iot/` with `BACKEND=mqtt`),
> one of [four swappable backends](#one-firmware-four-swappable-ota-backends).

## Status (verified vs. expected)

- ✅ **Firmware builds** (PlatformIO `espidf`, ESP-IDF 5.3.1) with the full Modular
  OTA stack; image ~57% of a 1.56 MB OTA slot.
- ✅ **CDK synthesizes** to valid CloudFormation; device IoT policy and OTA
  role/signing wiring inspected in the emitted template.
- ✅ **On-device logic** (Jobs control plane, file-streams download, ECDSA-P256
  verify in the PAL, self-test → commit/rollback, anti-brick watchdog) reviewed
  against the vendored SDK + ESP-IDF source.
- ⛳ **Not yet run on a live AWS account / flashed board** — needs your credentials
  and hardware. `SUCCEEDED` (happy path) and `FAILED`/`TIMED_OUT` (rollback) are the
  expected results of the documented steps, not yet observed. The Jobs control plane
  subscribes to `jobs/start-next/accepted` and picks up a queued job via its own
  `StartNext` at boot/reconnect — **deferred-to-boot**: it does *not* subscribe to the
  live `notify-next` push, so a job pushed while the device runs waits until the next
  boot. The `StartNext` promotes the job off `QUEUED` and reports
  `IN_PROGRESS → SUCCEEDED/FAILED`, so there's no spurious `TIMED_OUT`.


## Architecture decisions

- **Identity = mutual TLS, always from `esp_secure_cert`.** The device cert + private
  key — and the Thing name, taken from the cert's subject CN — are read at runtime from
  the on-flash `esp_secure_cert` partition, provisioned per board on-host by
  [`esp/scripts/provision-secure-cert.sh`](esp/scripts/provision-secure-cert.sh). Nothing
  device-specific is embedded, so ONE firmware image serves the whole fleet (see the
  Fleet section in [`aws-iot/README.md`](aws-iot/README.md)). The key is a plaintext TLV
  or a DS-peripheral-wrapped key (`--ds`); only the public root CA + code-signing cert
  are baked into the `.bin`.
- **Orchestration = AWS IoT Jobs**, created by the OTA Manager (`aws iot
  create-ota-update` → Signer job + Stream + Job).
- **Authenticity:** the AWS Signer code signature (ECDSA-P256) is verified on-device
  before activation. Secure Boot v2 + Flash Encryption are OFF on dev units (separate
  hardening pass; see `esp/partitions.csv`); the signature check still runs.
- **Commit semantics:** an OTA image boots `PENDING_VERIFY`; a watchdog is armed, a
  self-test confirms a cloud round-trip + core-function check, then the device commits
  (`esp_ota_mark_app_valid_cancel_rollback`) — otherwise the bootloader rolls back.
  `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`.

## One firmware, four swappable OTA backends

One codebase ([`esp/`](esp/)) with four interchangeable OTA backends behind a stable
device API ([`esp/src/device_iot.h`](esp/src/device_iot.h)). The app
(`esp/src/app_main.c`) runs unchanged on any backend; pick one at build time:

```bash
cd esp
pio run -e mqtt     # C: AWS OTA agent · MQTT File Streams · Signer ECDSA verify
pio run -e https    # D: AWS OTA agent · HTTPS data path   · Signer ECDSA verify
pio run -e jobs     # B: AWS IoT Jobs lib · esp_https_ota   · no on-device verify
pio run -e manual   # A: custom MQTT protocol · esp_https_ota · no AWS libraries
```

Two axes — **orchestration** (AWS OTA agent / standalone Jobs lib / hand-rolled) ×
**transfer** (MQTT File Streams / HTTPS presigned URL); C/D/B/A map to the
*Anti-Bricking OTA* doc:

| backend (`-e`) | doc | orchestration · transfer · on-device authenticity | cloud ([`aws-iot/`](aws-iot/)) |
|---|---|---|---|
| `mqtt`   | C | AWS OTA agent (coreMQTT+Jobs) · **MQTT File Streams** · Signer ECDSA verify | `BACKEND=mqtt` |
| `https`  | D | AWS OTA agent (coreMQTT+Jobs) · **HTTPS GET** (presigned URL) · Signer ECDSA verify | `BACKEND=https` |
| `jobs`   | B | esp-mqtt + standalone Jobs lib · `esp_https_ota` · none (TLS + Secure Boot) | `BACKEND=jobs` |
| `manual` | A | esp-mqtt + hand-rolled MQTT protocol · `esp_https_ota` · none | `BACKEND=manual` |

Each backend = one *transport* (`transport.h`) + one *orchestrator* (`ota_backend.h`).
A pre-script (`select_backend.py`) exports the env name so CMake compiles only that
backend's two sources and its component deps — no runtime dispatch, each binary as
lean as a single-purpose build. The app, `self_test.c`, `wifi.c`, and the partition
table are shared verbatim; esp-aws-iot is vendored once under
`esp/components/esp-aws-iot` (mqtt/https/jobs reference it; manual uses no AWS libs).
See [`esp/README.md`](esp/README.md). The cloud stays split per backend.

## Flash benchmark — what drives image size (C vs D vs B vs A)

All four build from the same `esp/` codebase with the identical partition table
(1.56 MB OTA slot), so the numbers are directly comparable (vGOOD, same version):

| backend | doc | flash (`.bin`) | % of 1.56 MiB slot | static RAM |
|---|---|--:|--:|--:|
| `mqtt`   | C — agent, MQTT data path | 941,744 B | 57.5 % | 73,068 B |
| `manual` | A — custom proto, HTTPS | 967,440 B | 59.0 % | 47,828 B |
| `https`  | D — agent + Signer verify, HTTPS | 971,824 B | 59.3 % | 56,212 B |
| `jobs`   | B — Jobs lib, HTTPS | 975,376 B | 59.5 % | 48,980 B |

(The shared `device_iot` facade adds a uniform ~0.5–0.8 KB, so the per-component
comparison below is unaffected.)

**The data path drives flash, not the amount of AWS.** C streams firmware over the
existing MQTT/TLS connection and is alone at the bottom (~940 KB). A, D and B
self-download over HTTPS, pulling in an HTTP client stack (~30 KB), so they cluster
at 967–975 KB within ~8 KB of each other — whether orchestrated by a hand-rolled
protocol (A), the Jobs lib (B), or the full agent with Signer verify (D). Switching
MQTT-streams → HTTPS costs ~30 KB; the amount of AWS machinery barely matters.

**Per-component (flash bytes):**

| archive | C | D | B | A |
|---|--:|--:|--:|--:|
| `coreMQTT` / esp-mqtt (`libmqtt`) | 16,391 | 16,399 | 16,361 | 16,353 |
| `http_parser` | — | 15,947 | 15,951 | 15,951 |
| `esp_http_client` | — | 8,939 | 10,329 | 10,329 |
| `tcp_transport` | — | 3,895 | 8,787 | 8,783 |
| `esp_https_ota` | — | — | 3,173 | 3,173 |
| File Streams + PAL | 6,684 | 4,448 | — | — |
| `espressif__cbor` | 3,873 | — | — | — |
| Jobs lib + coreJSON | 7,465 | 7,436 | 6,226 | — |
| cJSON (`libjson`) | — | — | 2,695 | 2,699 |
| mbedTLS + crypto + x509 (shared floor) | ~150 K | ~150 K | ~150 K | ~150 K |

D drops `espressif__cbor` and the unused MQTTFileDownloader, so despite keeping the
full agent + PAL it lands ~3 KB smaller than B. The shared mbedTLS + WiFi + IDF floor
(~750 KB) dwarfs the ~30 KB OTA-stack delta. RAM ordering — C 73 KB ≫ D 56 KB >
B 49 KB ≈ A 48 KB — tracks each variant's static MQTT/streams buffering; HTTPS drops
C's File-Streams block buffers. **D answers "signed URL with the agent?":** efficient
HTTPS transfer plus on-device Signer verify, at ~B's flash and ~17 KB less RAM than C.

> Reproduce: from `esp/`, build each backend, then
> `python -m esp_idf_size --archives .pio/build/<backend>/<proj>.map` (per-component)
> and `xtensa-esp32s3-elf-nm --print-size --size-sort firmware.elf` (per-function).

## Quickstart (end to end)

```bash
git submodule update --init --recursive          # pull esp-aws-iot + its libs

# 1) Cloud resources  (one dir, backend chosen by BACKEND — here variant C)
cd aws-iot && npm install && BACKEND=https npx cdk deploy && cd ..

# 2) Code-signing key, mqtt/https only (device identity is separate — see aws-iot/README "Fleet")
BACKEND=https aws-iot/scripts/make-codesign-cert.sh    # jobs/manual: no-op

# 3) Firmware: set Wi-Fi + endpoint + Thing name, build the initial image, flash
cp esp/src/secrets.h.example esp/src/secrets.h        # then edit it
esp/scripts/build-fixture.sh https good 1.0.0          # backend = https (D)
cd esp && pio run -e https -t upload -t monitor && cd ..

# 4) Happy-path OTA  (downloads, verifies, self-tests, COMMITS -> job SUCCEEDED)
esp/scripts/build-fixture.sh https good 2.0.0
BACKEND=https aws-iot/scripts/upload-firmware.sh esp/fixtures/firmware-https-good-v2.0.0.bin 2.0.0
BACKEND=https aws-iot/scripts/push-update.sh esp32-ota-poc-01 2.0.0
BACKEND=https aws-iot/scripts/watch.sh       esp32-ota-poc-01

# 5) Rollback OTA  (self-test fails -> device ROLLS BACK, stays alive -> job FAILED)
esp/scripts/build-fixture.sh https bad 2.0.0
BACKEND=https aws-iot/scripts/upload-firmware.sh esp/fixtures/firmware-https-bad-v2.0.0.bin 2.0.0-bad
BACKEND=https aws-iot/scripts/push-update.sh esp32-ota-poc-01 2.0.0-bad
BACKEND=https aws-iot/scripts/watch.sh       esp32-ota-poc-01
```
(Swap `https` for `mqtt`/`jobs`/`manual` in both `-e` and `BACKEND=` for any other
backend — the steps are otherwise identical.)

## Acceptance criteria → where

| # | criterion | status |
|---|-----------|--------|
| 1 | builds in PlatformIO `espidf`, connects to AWS IoT Core | build ✅ verified; connect = logic reviewed, needs a live run |
| 2 | happy path: download → verify Signer sig → activate → self-test commits → **SUCCEEDED** | logic in `orchestrators/ota_filestreams.c` / `self_test.c` ✅; outcome expected, run step 4 to confirm |
| 3 | rollback: `vBAD` → self-test fails → **rollback** → device alive → **FAILED/TIMED_OUT** | `FW_SELFTEST_SHOULD_PASS=0` fixture builds ✅; outcome expected, run step 5 to confirm |
| 4 | a fresh dev reproduces each path from the READMEs | this file + the `esp/` and `aws-iot/` READMEs |

## Out of scope

Secure Boot / Flash Encryption eFuse burns; recovery/factory image + captive portal;
A/B identity re-key; custom server-driven control plane. Managed AWS IoT Core path
only. (`esp_secure_cert` credential provisioning — including the DS peripheral — *is*
wired; see **Identity** above. The irreversible eFuse burn on real units is yours.)
