# `esp/` — unified, backend-swappable OTA firmware

One ESP32-S3 firmware codebase with **four interchangeable OTA backends** behind a
single stable device API. The application is written once against
[`src/device_iot.h`](src/device_iot.h) and runs unchanged on any backend — you
pick one at **build time**:

```bash
pio run -e mqtt     # C: AWS OTA agent · MQTT File Streams · Signer ECDSA verify
pio run -e https    # D: AWS OTA agent · HTTPS data path   · Signer ECDSA verify
pio run -e jobs     # B: AWS IoT Jobs lib · esp_https_ota   · no on-device verify
pio run -e manual   # A: custom MQTT protocol · esp_https_ota · no AWS libraries
```

## The normalized API

An application includes only `device_iot.h`:

```c
#include "device_iot.h"

static bool app_healthy(void) { /* your post-OTA health gate */ return true; }
static void on_cmd(const char *topic, const uint8_t *d, size_t n) { /* ... */ }
static void on_conn(bool up) { if (up) device_iot_publish("status", "online", 6, 1); }

void app_main(void) {
    device_iot_set_health_check(app_healthy);
    device_iot_set_connection_cb(on_conn);   // birth on connect; fires on reconnect too

    device_iot_config_t cfg;
    device_iot_default_config(&cfg);         // build-time identity (secrets.h + embedded certs)
    device_iot_init(&cfg);                   // override cfg fields first to provision per device

    device_iot_subscribe("cmd", on_cmd);      // dt/<thing>/cmd
    for (;;) {
        device_iot_publish("heartbeat", json, len, 1);    // dt/<thing>/heartbeat, QoS1
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
```

The facade is the same on every backend:

- **`device_iot_default_config(&cfg)` → `device_iot_init(&cfg)`** — the default loader
  fills `cfg` with the build-time identity (`secrets.h` + embedded certs); override
  fields (endpoint, Thing name, cert/key, or a DS-peripheral handle) before init to
  provision per device — one image, per-device identity. No NULL sentinel; the
  defaults are explicit in the struct.
- **`device_iot_publish(subtopic, …, qos)`** — non-blocking; enqueues into a bounded
  outbox and returns. **QoS1 is delivered at-least-once** (retransmitted until the
  broker acks, surviving reconnects). Under sustained backpressure it returns
  `ESP_ERR_NO_MEM` rather than blocking (coreMQTT) or growing until OOM (esp-mqtt) —
  identical semantics whether coreMQTT or esp-mqtt is linked.
- **`device_iot_publish_topic` / `device_iot_subscribe_topic`** — arbitrary topics
  (Device Shadow `$aws/things/…`, fleet, custom), wildcard (`+`/`#`) delivery.
- **`device_iot_set_connection_cb(cb)`** — fires on first connect and every reconnect
  (the facade has already re-subscribed your subscriptions), and on disconnect.

That `app_main` is **byte-for-byte identical** for all four backends (it's the real
`src/app_main.c`). The facade hides coreMQTT vs esp-mqtt, AWS Jobs vs a custom
protocol, and MQTT-streams vs HTTPS download.

## How the swap works

```
src/
├── app_main.c          # the app — device_iot.h only, identical across backends
├── device_iot.[ch]     # the stable facade + boot orchestration + publish router
├── transport.h         # contract: transport_start/publish/subscribe/is_connected
├── ota_backend.h       # contract: ota_backend_start / ota_backend_on_publish
├── self_test.[ch]      # trial-boot gate, watchdog, commit/rollback (shared)
├── wifi.[ch] · app_config.h · ota_download.[ch] · certs/
├── transports/
│   ├── transport_coremqtt.c   # coreMQTT  (mqtt + https backends)
│   └── transport_espmqtt.c    # esp-mqtt  (jobs + manual backends)
└── orchestrators/
    ├── ota_filestreams.c  ota_http.c  ota_jobs.c  ota_manual.c
```

Each backend = one **transport** (`transport.h`) + one **orchestrator**
(`ota_backend.h`). `select_backend.py` (a PlatformIO pre-script) exports the env
name as `OTA_BACKEND`; [`src/CMakeLists.txt`](src/CMakeLists.txt) then compiles
**only** that backend's two source files and lists **only** its component deps —
so each binary is exactly as lean as the standalone variant was (verified: the
unified `-e mqtt` build is within ~0.5 KB of the old `mqtt-esp`). No runtime
dispatch, no dead stacks linked in.

Adding a fifth backend = drop in `transports/…` and/or `orchestrators/ota_x.c`
that implement the two contracts, add an `elseif` in `src/CMakeLists.txt`, and a
`[env:x]`.

## Build · flash · fixtures

```bash
cp src/secrets.h.example src/secrets.h    # Wi-Fi + endpoint + Thing name
pio run -e https                            # (or https | jobs | manual)
pio run -e https -t upload -t monitor
scripts/build-fixture.sh https good 2.0.0   # vGOOD/vBAD fixture per backend
```

Anti-brick behaviour, partition table, and the `FW_SELFTEST_SHOULD_PASS`
vGOOD/vBAD mechanism are unchanged and shared across all backends. The
backend-by-backend **flash comparison** is in the [top-level README](../README.md).
