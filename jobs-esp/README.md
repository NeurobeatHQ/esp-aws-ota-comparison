# `jobs-esp/` — variant B: AWS IoT Jobs (esp-mqtt) + esp_https_ota

The middle of the three OTA variants from *Anti-Bricking OTA & Identity
Re-issuance* (Appendix 1 §B). Same anti-brick boot/commit/rollback as variant C
([`../esp`](../esp/README.md)), but a **lighter, partly-DIY** update path:

| | variant C (`../esp`) | **variant B (here)** |
|---|---|---|
| MQTT client | coreMQTT (+ network_transport) | **esp-mqtt** (IDF native) |
| Orchestration | AWS IoT Jobs lib | AWS IoT Jobs lib (topic helpers only) |
| Firmware download | AWS **MQTT File Streams** | **esp_https_ota** from a presigned S3 URL |
| Job document | AFR_OTA schema | **custom** `{"op":"ota","url","target_version"}` (cJSON) |
| On-device authenticity | AWS Signer **ECDSA-P256** code-sign verify (PAL) | **none** — TLS-to-S3 (+ Secure Boot, off on dev) |
| Cloud | Signer + OTA role + `create-ota-update` | plain `create-job` ([`../jobs-aws-iot`](../jobs-aws-iot/README.md)) |

**Reused verbatim from C:** `self_test.c` (trial-boot gate, watchdog, commit/rollback,
NVS hand-off), `wifi.c`, `partitions.csv`, the rollback/WDT/TLS `sdkconfig`. The
esp-mqtt wrapper (`mqtt_es.c`) and the download helper (`ota_download.c`) are shared
with variant A. The control plane is `jobs_orchestrator.c`.

The Jobs lib + coreJSON are referenced from the shared submodule under
`../esp/components/esp-aws-iot` (run `git submodule update --init --recursive` at the
repo root first). esp-mqtt, esp_https_ota and cJSON are IDF built-ins.

## Build / flash / fixtures

```bash
cp src/secrets.h.example src/secrets.h     # edit Wi-Fi + endpoint + Thing name
pio run -e feather_s3                       # build
pio run -e feather_s3 -t upload -t monitor # flash + watch
```
The `FW_SELFTEST_SHOULD_PASS` flag (vGOOD/vBAD) works exactly as in C — reuse
`../esp/scripts/build-fixture.sh` semantics or pass `PLATFORMIO_BUILD_FLAGS`.

## OTA flow

`notify-next` / `StartNext` → custom job doc (presigned URL) → `reportJobStatus(IN_PROGRESS)`
→ stash job id to NVS → `esp_https_ota` download → reboot → self-test → commit +
`Jobs_Update SUCCEEDED` or reject + `FAILED` → rollback. Push one with
[`../jobs-aws-iot/scripts/create-job.sh`](../jobs-aws-iot/README.md).
