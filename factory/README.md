# nbt factory / recovery image (ESP32-S3, PlatformIO + ESP-IDF)

The `factory` image from design doc §4 — what the bootloader falls back to when **no OTA
slot is bootable**, and what the **GPIO factory-reset** boots. Its whole job: let an
operator put a known-good, **signed** image back onto an OTA slot.

Customer-facing recovery steps are in **[RECOVERY.md](RECOVERY.md)**.

## Behavior (§4)

On boot it **always** brings up an open SoftAP **`nbt-factory-ap`** with a captive portal.
The portal has three fields — **SSID, password, URL** — and on submit it:

1. joins the operator's WiFi (AP+STA: the portal AP stays up),
2. downloads the signed `.bin` at **URL** into the next OTA slot, then
3. reboots into it.

On failure it stays on `nbt-factory-ap` so the operator can retry. A DNS hijack pops the
"sign in to network" sheet automatically. There is **no silent/unattended path and no
stored creds** — every recovery is operator-driven at the portal.

## How it downloads — HTTP, no TLS, no trust store

The image is fetched over **plain HTTP with a raw lwIP socket** (no `esp_http_client`, no
`esp-tls`, no mbedTLS — see *Size*). Authenticity is **not** the connection; it's the
**Secure Boot signature** inside the `.bin`: `esp_ota_end` verifies it before the slot is
made bootable (when Secure Boot is on), and the bootloader re-verifies on every boot. A
tampered or wrong-key image is rejected no matter how it arrived, so there's no trust store
to carry, edit, or let go stale. (On a dev unit without Secure Boot it just flashes — enable
SB for the guarantee.)

It writes to `esp_ota_get_next_update_partition()` — an **OTA slot, never `factory`** — so
the recovery flow can't brick itself.

## Size (~540 KB)

Stripped of all TLS/crypto, down from ~910 KB:

| Step | Result |
|---|---|
| cert bundle off + `-Os` + log/printf trim | ~750 KB |
| raw-socket HTTP OTA (drops `libmbedtls`, the TLS handshake) | ~590 KB |
| disable WPA3/OWE so the supplicant drops `libmbedcrypto` | **~540 KB** |

What remains is the **WiFi stack itself** (`libnet80211`/`libpp`/`libphy` ≈ 180 KB
closed-source + lwIP + supplicant + your app), which is the floor for a WiFi-portal image.
**Tradeoff:** WiFi is now **WPA2/open only** (no WPA3/OWE/enterprise) — fine for recovery,
but the network you recover over must be 2.4 GHz WPA2. Verify it joins a real WPA2 network
on hardware after config changes.

## Layout

```
factory/
├── platformio.ini        # env:factory — framework=espidf, board, partitions
├── partitions.csv        # nvs + otadata + phy + factory + ota_0 + ota_1 (4 MB)
├── sdkconfig.defaults    # size flags + (optional) bootloader factory-reset GPIO
├── CMakeLists.txt
├── README.md             # this file
├── RECOVERY.md           # customer recovery steps
└── src/
    ├── CMakeLists.txt
    └── main.c            # everything: SoftAP + captive portal + DNS hijack + raw-socket OTA
```

## Build & flash

```bash
cd factory
pio run -t upload          # builds + flashes into the `factory` partition
pio device monitor         # 115200
```

With empty OTA slots and no otadata, the bootloader boots `factory` → you land in the
recovery portal: join `nbt-factory-ap`, open `http://192.168.4.1`, enter SSID/password/URL,
submit.

## Entering factory on demand — the recovery button (GPIO 14)

Two ways in:

1. **Automatic** — both OTA slots unbootable → the bootloader boots `factory`.
2. **On demand** — hold the **recovery button** at power-on. This is the bootloader's own
   factory-reset, configured in `sdkconfig.defaults`:
   ```
   CONFIG_BOOTLOADER_FACTORY_RESET=y
   CONFIG_BOOTLOADER_NUM_PIN_FACTORY_RESET=14   # recovery button, active-low
   CONFIG_BOOTLOADER_OTA_DATA_ERASE=y           # clear otadata so it picks factory
   ```
   **GPIO 14** is assumed because it is **not** a boot-strapping pin — avoid GPIO0/45/46,
   which the ROM uses to select USB download mode, or the button won't reach factory.
   Verify the exact symbol names against your IDF version, and do **not** add data
   partitions to the factory-reset erase list. The factory app itself reads no GPIO — it
   always shows the portal.

## Notes

- Pure ESP-IDF (`framework = espidf`), single file, no Arduino.
- **AP+STA single-radio quirk:** when it joins the operator's network, the single radio
  moves the SoftAP to that channel, so the operator's phone may briefly drop off
  `nbt-factory-ap` during the OTA. The "recovering…" page is sent *before* the join, so it
  isn't lost; success = reboot into the new image.
- The DNS responder is a minimal single-question A-record hijack — enough for captive-portal
  detection, not a general resolver.
- The OTA `url` host must serve the signed `.bin` over plain HTTP/1.0 (no chunked
  transfer-encoding); the factory image doesn't parse chunked responses.
