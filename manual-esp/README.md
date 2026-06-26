# `manual-esp/` — variant A: fully custom MQTT control protocol + esp_https_ota

The lightest of the three variants (*Anti-Bricking OTA & Identity Re-issuance*,
Appendix 1 §A): **no AWS IoT Jobs at all**. The device runs a hand-rolled
request/response protocol on app topics; a custom server replies with a presigned
URL. Same anti-brick boot/commit/rollback as variants B and C.

| | variant B (`../jobs-esp`) | **variant A (here)** |
|---|---|---|
| Orchestration | AWS IoT Jobs lib (over esp-mqtt) | **custom MQTT request/response** |
| AWS libraries on device | Jobs lib + coreJSON | **none** (IDF-only: esp-mqtt + esp_https_ota + cJSON) |
| Firmware download | esp_https_ota (presigned URL) | esp_https_ota (presigned URL) |
| Cloud | `create-job` | operator scripts ([`../manual-aws-iot`](../manual-aws-iot/README.md)) |

> AWS IoT Core is still the **MQTT transport** (mutual TLS, thing-scoped policy,
> clientId == Thing name). "No AWS" means no AWS *libraries / Jobs* — it is **not**
> broker-less and **not** Greengrass.

**Protocol** (`control_protocol.c`):
```
device -> dt/<thing>/ota/should-i-update   {"thing","version"}
server -> dt/<thing>/ota/plan              {"op":"ota","url","ota_id","target_version"} | {"op":"none"}
device -> dt/<thing>/ota/confirm           {"ota_id","result":"success|failed","version"}
```

**Reused verbatim from C/B:** `self_test.c`, `wifi.c`, and the shared `mqtt_es.c` +
`ota_download.c`. `partitions.csv` / `sdkconfig.defaults` identical to C for an
apples-to-apples flash comparison.

## Build / flash

```bash
cp src/secrets.h.example src/secrets.h     # edit Wi-Fi + endpoint + Thing name
pio run -e feather_s3
pio run -e feather_s3 -t upload -t monitor
```

## OTA flow

normal boot → publish `should-i-update` → server replies `plan{url,ota_id}` → stash
ota_id to NVS → `esp_https_ota` download → reboot → self-test → commit + publish
`confirm{success}` or reject + `confirm{failed}` → rollback. Drive it with the
operator scripts in [`../manual-aws-iot`](../manual-aws-iot/README.md).
