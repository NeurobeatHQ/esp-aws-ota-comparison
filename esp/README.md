# `esp/` — ESP32-S3 firmware (PlatformIO · pure ESP-IDF 5.3 · Modular OTA)

Rollback-safe Over-The-Air updates on an **Adafruit Feather ESP32-S3** using the
**AWS-managed stack**: AWS IoT **Jobs** + **MQTT File Streams** ("Modular OTA") +
**coreMQTT** over mutual TLS. Pure ESP-IDF — `app_main()`, FreeRTOS tasks,
`ESP_LOGx`; no Arduino.

> **Why "Modular OTA" and not the classic `OTA_Init` agent?** The classic AWS OTA
> Agent only exists in `esp-aws-iot` ≤ `202210.01-LTS`, which supports ESP-IDF
> ≤ 5.1 and is end-of-life. ESP-IDF 5.3.x is supported only on `202406.05-LTS`,
> which **replaced** the agent with the Jobs + MQTT-File-Streams libraries used
> here (Espressif's current, supported path; reference: `FreeRTOS/iot-reference-esp32`).
> See the top-level [README](../README.md) for the full decision record.

## How it works

```
            ┌─────────────────────── ESP32-S3 ───────────────────────┐
 AWS IoT    │  mqtt_client.c        ota_orchestrator.c    self_test.c │
 Core  <════╪═ coreMQTT (mTLS) ══>  Jobs + FileStreams +   trial-boot  │
 (Jobs +    │  esp-tls transport    OTA PAL (esp_ota_*)    self-test    │
  streams)  │        ▲                     │                  │        │
            │        └── incoming PUBLISH ─┘   commit/rollback ┘        │
            └─────────────────────────────────────────────────────────┘
```

1. `app_main` brings up Wi-Fi, then coreMQTT to AWS IoT Core (mutual TLS, embedded
   device cert/key).
2. `ota_orchestrator` subscribes to `$aws/things/<thing>/jobs/notify-next`,
   pulls a job with `StartNext`, parses the AFR-OTA job document, downloads the
   image block-by-block over the MQTT stream, **verifies the AWS Signer
   ECDSA-P256 signature** (OTA PAL), writes the passive `ota_x` slot, then
   activates + reboots.
3. The new image boots **PENDING_VERIFY**. `self_test` arms a watchdog, confirms
   the cloud round-trip (MQTT connected) + a core-function check, and either
   **commits** (`esp_ota_mark_app_valid_cancel_rollback`, report job
   **SUCCEEDED**) or **rejects** (`esp_ota_mark_app_invalid_rollback_and_reboot`,
   report **FAILED**) → the bootloader rolls back to the previous slot.

The in-flight job id is stashed in NVS before activation so the freshly-booted
trial image can report the job result for it.

### Source map

| file | role |
|------|------|
| `src/app_main.c`         | boot sequence; arms the self-test watchdog up front on a trial boot |
| `src/wifi.c`             | Wi-Fi station bring-up |
| `src/mqtt_client.c`      | coreMQTT over the esp-tls `network_transport`; process-loop task + reconnect |
| `src/ota_orchestrator.c` | Modular OTA state machine (Jobs + MQTT File Streams + OTA PAL) |
| `src/self_test.c`        | trial-boot detection, watchdog, commit/reject, NVS job-id hand-off |
| `src/app_config.h`       | non-secret config + version/variant flags |
| `src/secrets.h`          | Wi-Fi creds, endpoint, Thing name (gitignored — copy from `.example`) |
| `partitions.csv`         | 4 MB, dual `ota_0`/`ota_1` + `otadata` (no factory image) |
| `sdkconfig.defaults`     | rollback, mbedTLS threading, watchdog, flash size |

## Prerequisites

- **PlatformIO Core** (`pip install platformio`, or the IDE). First build downloads
  ESP-IDF 5.3.1 + the Xtensa toolchain (one-time, a few hundred MB).
- The **esp-aws-iot** component, vendored as a git submodule:
  ```bash
  git submodule update --init --recursive        # from the repo root
  ```
- The board wired for flashing (this repo's `esp-flashcycle` setup uses a Waveshare
  RP2350-One bridge; a plain USB connection works too).

## One-time setup

1. **Secrets**
   ```bash
   cp src/secrets.h.example src/secrets.h
   # edit src/secrets.h: WIFI_SSID, WIFI_PASSWORD, AWS_IOT_ENDPOINT, THING_NAME
   ```
   Get the endpoint with:
   ```bash
   aws iot describe-endpoint --endpoint-type iot:Data-ATS --query endpointAddress --output text
   ```
   `THING_NAME` **must** equal the AWS IoT Thing name (clientId == thing name).

2. **Device certificate + key** — overwrite the placeholders (see
   [`../aws-iot/README.md`](../aws-iot/README.md) to create/register them):
   ```
   src/certs/client.crt        # device certificate
   src/certs/client.key        # device private key (plaintext, POC)
   src/certs/aws_codesign.crt   # PUBLIC code-signing cert (must match the Signer profile)
   ```
   `src/certs/AmazonRootCA1.pem` is already the real (public) AWS root CA.

## Build · flash · monitor

```bash
pio run -e feather_s3                 # build
pio run -e feather_s3 -t upload       # flash
pio run -e feather_s3 -t monitor      # serial @ 115200
```
(In this repo you can instead use the **`esp-flashcycle`** skill to flash + watch
logs through the RP2350-One bridge.)

First healthy image:
```bash
scripts/build-fixture.sh good 1.0.0
pio run -e feather_s3 -t upload       # or flash fixtures/firmware-good-v1.0.0.bin
```

## Demonstrating OTA + rollback

Build the two OTA target fixtures:
```bash
scripts/build-fixture.sh good 2.0.0   # -> fixtures/firmware-good-v2.0.0.bin  (commits)
scripts/build-fixture.sh bad  2.0.0   # -> fixtures/firmware-bad-v2.0.0.bin   (rolls back)
```
`good` builds with `FW_SELFTEST_SHOULD_PASS=1`; `bad` with `=0`, so its
post-reboot self-test deterministically fails. Then, from [`../aws-iot/`](../aws-iot/):

> The two flows below are the **expected** behaviour of the documented steps.
> The on-device logic is reviewed and the firmware builds, but the full cloud
> round-trip has not been run on a live account here — do one run to confirm.

**Happy path**
```bash
../aws-iot/scripts/sign-and-publish.sh ../esp/fixtures/firmware-good-v2.0.0.bin 2.0.0
../aws-iot/scripts/push-ota.sh   <thing-or-group> 2.0.0
../aws-iot/scripts/watch-job.sh  <thing>
```
Expected serial log: download → `signature OK -> activating` → reboot → self-test
→ `image COMMITTED` → job **SUCCEEDED**. (The device reports `IN_PROGRESS` when the
download starts, so the execution leaves `QUEUED`.)

**Rollback path**
```bash
../aws-iot/scripts/sign-and-publish.sh ../esp/fixtures/firmware-bad-v2.0.0.bin 2.0.0-bad
../aws-iot/scripts/push-ota.sh   <thing> 2.0.0-bad
```
Expected serial log: download → activate → reboot into v2.0.0(bad) →
`core-function check FAILED (vBAD)` → `image REJECTED -> rolling back` → reboot →
back on the previous image, still alive → job **FAILED/TIMED_OUT**.

## Notes

- **Watchdog / anti-brick.** On a trial boot the Task WDT is armed *before* Wi-Fi
  (180 s) and disarmed on commit; the bootloader RTC WDT (`CONFIG_BOOTLOADER_WDT_ENABLE`)
  covers pre-`app_main` boot. A hang anywhere in bring-up → reset → rollback.
- **Partition table** (`partitions.csv`): two 1.56 MB `ota_0`/`ota_1` slots +
  `otadata`. No factory/recovery image (out of scope).
- **Secrets & Flash Encryption / Secure Boot are OFF** on dev units (a separate
  hardening pass). Where they slot in is noted in `partitions.csv` and the top
  README. The OTA code-signature check still runs and is exercised here.
- **DS peripheral / `esp_secure_cert`**: the transport already carries
  `use_secure_element` + `ds_data` (see `mqtt_client.c`), so moving the key into
  hardware later is a config change, not a protocol change.

## Troubleshooting

| symptom | cause / fix |
|---------|-------------|
| TLS handshake fails | wrong/placeholder `client.crt`/`client.key`, or `AttachThingPrincipal` not done (see aws-iot README) |
| `MQTT_Connect` returns, then disconnects | device IoT policy missing `iot:Connect` for clientId == thing name |
| Job stuck `QUEUED`, no download | policy missing `$aws/things/<thing>/jobs/*` or `…/streams/*` topic perms |
| `signature verification FAILED` | `src/certs/aws_codesign.crt` doesn't match the Signer profile used to sign |
| Healthy image rolls back | self-test couldn't reach the cloud within 180 s — check Wi-Fi/endpoint |
| component manager can't fetch `espressif/cbor` | first build needs network; re-run `pio run` |
