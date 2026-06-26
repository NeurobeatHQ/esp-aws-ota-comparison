# ESP32-S3 · AWS IoT Core · Rollback-safe OTA (POC)

A working proof-of-concept for **secure, rollback-safe Over-The-Air updates** on
an **ESP32-S3** (Adafruit Feather), using the **AWS-managed stack**: AWS IoT
**Jobs** + the **MQTT File Streams** OTA libraries + **coreMQTT** over mutual TLS,
on **pure ESP-IDF 5.3.x** (PlatformIO, `framework = espidf` — no Arduino).

```
   ESP32-S3 ──mutual TLS──> AWS IoT Core ── Jobs (orchestration)
   coreMQTT                                └ MQTT File Streams (image, ECDSA-P256 signed)
      │
      └ download → verify signature → activate → reboot → self-test → commit / ROLLBACK
```

> This page describes **variant C** (the maximal-AWS stack). The repo also carries
> two lighter variants — **B** (AWS IoT Jobs + self-download) and **A** (fully
> custom) — see [Three OTA variants](#three-ota-variants) and the
> [flash benchmark](#flash-benchmark--the-cost-of-aws-standardization-c-vs-b-vs-a).

## Status (verified vs. expected — read this)

- ✅ **Firmware builds** in PlatformIO (`framework = espidf`, ESP-IDF 5.3.1) with
  the full Modular OTA stack; the image is ~57% of a 1.56 MB OTA slot.
- ✅ **CDK synthesizes** to valid CloudFormation; the device IoT policy and OTA
  role/signing wiring were inspected in the emitted template.
- ✅ **On-device logic** (Jobs control plane, file-streams download, ECDSA-P256
  signature verify in the PAL, self-test → commit/rollback, the anti-brick
  watchdog) was reviewed against the vendored SDK + ESP-IDF source.
- ⛳ **The end-to-end OTA round-trip + rollback have NOT been run on a live AWS
  account / flashed board** — they can't be without your credentials and
  hardware. The job **SUCCEEDED** (happy path) and **FAILED/TIMED_OUT** (rollback)
  outcomes are the *expected* results of the documented steps, not yet observed
  here. Do one real run to confirm. The Jobs control plane reports
  `IN_PROGRESS` → `SUCCEEDED`/`FAILED` explicitly (subscribes to both
  `jobs/notify-next` and `jobs/start-next/accepted`) so a pushed job is promoted
  off `QUEUED` rather than risking a spurious `TIMED_OUT`.

## Two decisions this POC settled (with evidence)

This started from a broader brief (two connection modes incl. a local Greengrass
core; the classic AWS *OTA Agent*). Two things were checked against primary
sources and changed the shape:

1. **Modular OTA, not the classic `OTA_Init` OTA Agent.** The classic agent
   exists only in `esp-aws-iot` ≤ `release/202210.01-LTS`, which supports
   ESP-IDF ≤ 5.1 and is **end-of-life**. ESP-IDF 5.3.x is supported only on
   `202406.05-LTS`, which **removed** the agent and ships the **AWS IoT Jobs +
   MQTT File Streams** libraries instead (verified by listing both branches).
   The firmware here uses that current, supported path (reference:
   `FreeRTOS/iot-reference-esp32`). The architecture the brief asked for — Jobs
   orchestration, AWS Signer ECDSA-P256 verification, self-test/commit/rollback —
   is all intact; only the on-device API is the modern one.

2. **Greengrass was dropped.** The brief flagged "validate, don't assume" for
   relaying the reserved `$aws/things/<thing>/jobs/*` topics over the Greengrass
   MQTT Bridge. It was validated: **it does not work.** AWS publishes Jobs
   request/response messages *directly to the publishing client*, bypassing the
   broker ("these response messages don't pass through the message broker"), and
   the Bridge is a separate client with the *core's* identity — so it cannot
   carry a client device's Jobs control plane. AWS's own analogue (client-device
   Shadows) bridges reserved topics LocalMqtt↔**Pubsub** (local-only) plus a
   dedicated Shadow Manager; there is no Jobs equivalent. The managed OTA agent
   also can't be pointed at a LAN file server (the download source comes from
   AWS S3/streams). So a fully-local managed OTA isn't achievable, and Greengrass
   adds nothing to the OTA story — it's orthogonal. Scope is therefore a focused
   **AWS IoT Core** OTA POC.

## Architecture decisions (carried forward)

- **Identity = mutual TLS** with a plaintext device cert + key (embedded). The
  transport keeps `use_secure_element` + `ds_data` fields, so moving the key to
  the **DS peripheral / `esp_secure_cert`** later is a config change, not a
  protocol change. DS provisioning is **not** implemented here.
- **Orchestration = AWS IoT Jobs**, created by the OTA Manager
  (`aws iot create-ota-update` → Signer job + Stream + Job).
- **Authenticity:** the OTA agent's **AWS Signer code signature (ECDSA-P256)** is
  verified on-device before activation. **Secure Boot v2 + Flash Encryption are
  OFF** on dev units (a separate hardening pass; where they slot in is noted in
  `esp/partitions.csv`). The code-signature check still runs and is exercised.
- **Commit semantics:** an OTA image boots `PENDING_VERIFY`; a watchdog is armed,
  a self-test confirms a cloud round-trip + a core-function check, and only then
  does the device commit (`esp_ota_mark_app_valid_cancel_rollback`). Otherwise it
  rejects → the bootloader rolls back. `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`.

## Three OTA variants

The repo carries three points on the "how much AWS to adopt" spectrum (from the
*Anti-Bricking OTA & Identity Re-issuance* design doc, Appendix 1) — each a
self-contained **firmware + cloud** pair:

| | firmware | cloud | on-device stack |
|---|---|---|---|
| **C** — AWS OTA stack | [`esp/`](esp/) | [`aws-iot/`](aws-iot/) | coreMQTT + Jobs + MQTT File Streams + OTA PAL (ECDSA verify); Signer + `create-ota-update` |
| **B** — AWS IoT Jobs | [`jobs-esp/`](jobs-esp/) | [`jobs-aws-iot/`](jobs-aws-iot/) | esp-mqtt + Jobs lib + `esp_https_ota` (presigned S3 URL); `create-job` w/ custom doc |
| **A** — fully custom | [`manual-esp/`](manual-esp/) | [`manual-aws-iot/`](manual-aws-iot/) | esp-mqtt + hand-rolled MQTT protocol + `esp_https_ota`; operator scripts |

esp-aws-iot is vendored once under `esp/components/esp-aws-iot` (a git submodule);
`jobs-esp` references the Jobs lib from there, `manual-esp` uses **no** AWS
libraries. `self_test.c` (the anti-brick gate), `wifi.c` and the partition table
are shared **verbatim** across all three. Each directory has its own README.

## Flash benchmark — the cost of AWS standardization (C vs B vs A)

All three build with the **identical** partition table (a 1.56 MB OTA slot), so
the numbers are directly comparable (vGOOD, same version, `feather_s3`).

| variant | flash (`.bin`) | % of 1.56 MiB slot | static RAM |
|---|--:|--:|--:|
| **C** — coreMQTT + File Streams + PAL (`esp/`) | 940,816 B | 57.4 % | 72,852 B |
| **B** — Jobs + esp_https_ota (`jobs-esp/`) | 974,672 B | 59.5 % | 48,764 B |
| **A** — custom + esp_https_ota (`manual-esp/`) | 966,704 B | 59.0 % | 47,612 B |

**Counterintuitive result: the "less-AWS" variants use *more* flash, not less** —
B is **+33.9 KB** and A is **+25.9 KB** vs C — but both use **~24 KB *less* RAM**.

**Why (per-component, flash bytes).** C→B/A *removes* ~27 KB (`coreMQTT` 16.4K,
`aws-iot-core-mqtt-file-streams` 6.7K, `espressif__cbor` 3.9K, `backoffAlgorithm`
0.2K) but *adds* ~57 KB — almost all of it the HTTP self-download stack:

| added in B/A | flash |
|---|--:|
| `libmqtt` (esp-mqtt) | 16,355 |
| **`http_parser`** | **15,951** |
| `esp_http_client` | 10,329 |
| `tcp_transport` | 8,785 |
| `esp_https_ota` | 3,173 |
| `cJSON` | 2,699 |

esp-mqtt (16.4K) ≈ coreMQTT (16.4K) — a wash. The real cost is the **HTTP client
stack** (`http_parser` + `esp_http_client` + `tcp_transport` + `esp_https_ota` ≈
**38 KB**), which exists only to self-download firmware over a *second* HTTPS
connection. Variant C avoids it by streaming the image over the **existing**
MQTT/TLS connection (File Streams, ~6.7 KB). The single largest flash symbol in
B/A is `http_parser_execute` at **12,557 bytes** — absent in C. A is 8 KB smaller
than B because it drops the Jobs lib + coreJSON.

**Shared floor (≈ constant all three):** mbedTLS/crypto/x509 ~150 KB + WiFi/lwIP/IDF
~600 KB dominate; the OTA-stack delta (~30 KB) is small against the ~940 KB whole.

**Takeaway.** The flash "cost of standardization" is **inverted**: the maximal-AWS
variant (C) is the *smallest* image, because the OTA agent reuses one MQTT/TLS
connection for everything while the DIY variants pay ~38 KB to bolt on an HTTP
client. Where B/A win is **RAM** (~24 KB less — no coreMQTT network buffer, no 2×
8 KB File-Streams block buffers, no second mbedTLS context held across tasks). On
this 4 MB-flash / 512 KB-RAM part the trade is "spend a little more flash to save
scarcer RAM"; all three sit at 57–60 % of the OTA slot with headroom either way.

> Reproduce: build each `*-esp` (`pio run -e feather_s3`), then
> `python -m esp_idf_size --archives .pio/build/feather_s3/<proj>.map` (per-component)
> and `xtensa-esp32s3-elf-nm --print-size --size-sort firmware.elf` (per-function).

## Quickstart (end to end)

```bash
git submodule update --init --recursive          # pull esp-aws-iot + its libs

# 1) Cloud resources
cd aws-iot && npm install && npx cdk deploy && cd ..

# 2) Device identity + code-signing material (writes certs into esp/src/certs/)
aws-iot/scripts/register-device.sh
aws-iot/scripts/make-codesign-cert.sh

# 3) Firmware: set Wi-Fi + endpoint + Thing name, build the initial image, flash
cp esp/src/secrets.h.example esp/src/secrets.h     # then edit it
esp/scripts/build-fixture.sh good 1.0.0
cd esp && pio run -e feather_s3 -t upload -t monitor && cd ..

# 4) Happy-path OTA  (downloads, verifies, self-tests, COMMITS -> job SUCCEEDED)
esp/scripts/build-fixture.sh good 2.0.0
aws-iot/scripts/sign-and-publish.sh esp/fixtures/firmware-good-v2.0.0.bin 2.0.0
aws-iot/scripts/push-ota.sh  esp32-ota-poc-01 2.0.0
aws-iot/scripts/watch-job.sh esp32-ota-poc-01

# 5) Rollback OTA  (self-test fails -> device ROLLS BACK, stays alive -> job FAILED)
esp/scripts/build-fixture.sh bad 2.0.0
aws-iot/scripts/sign-and-publish.sh esp/fixtures/firmware-bad-v2.0.0.bin 2.0.0-bad
aws-iot/scripts/push-ota.sh  esp32-ota-poc-01 2.0.0-bad
aws-iot/scripts/watch-job.sh esp32-ota-poc-01
```

## Acceptance criteria → where

| # | criterion | status |
|---|-----------|--------|
| 1 | builds in PlatformIO `espidf`, connects to AWS IoT Core | build ✅ verified; connect = logic reviewed, needs a live run |
| 2 | happy path: download → verify Signer sig → activate → self-test commits → **SUCCEEDED** | logic in `ota_orchestrator.c` / `self_test.c` ✅; outcome expected, run step 4 to confirm |
| 3 | rollback: `vBAD` → self-test fails → **rollback** → device alive → **FAILED/TIMED_OUT** | `FW_SELFTEST_SHOULD_PASS=0` fixture builds ✅; outcome expected, run step 5 to confirm |
| 4 | a fresh dev reproduces each path from the READMEs | this file + the two dir READMEs |

## Out of scope (by design)

Secure Boot / Flash Encryption eFuse burns; DS-peripheral provisioning; the
recovery/factory image + captive portal; A/B identity re-key; any custom
server-driven control plane; **AWS IoT Greengrass** (see decision #2). This POC is
the managed AWS IoT Core path only.
