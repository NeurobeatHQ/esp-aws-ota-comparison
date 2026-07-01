# Restoring your device

If your device stopped working after an update, or support asked you to "run recovery,"
you can restore it yourself in a few minutes. You'll put the device into **recovery mode**,
where it makes its own WiFi hotspot and shows a simple page to re-install firmware.

You can't damage the device doing this — see **Good to know** at the end.

## What you'll need

- The **device** and its USB-C power cable.
- The **recovery link** support gave you. It looks like
  `http://updates.neurobeat.io/recovery.bin`.
- A **2.4 GHz WiFi network** the device can join — its **name** and **password**.
  *(A normal home WiFi is fine. A 5 GHz-only network won't work.)*
- A **phone or laptop**.

## Step by step

1. **Unplug** the device to power it off.

2. Find the **recovery button** and **press and hold it**. *(If you're not sure which button
   it is, ask support — see the technician note at the bottom.)*

3. **Keep holding the button** and **plug the device back in**. Hold for about **3 seconds**
   after it powers on, then **let go**.
   - The device is now in recovery mode and has created a WiFi hotspot called
     **`nbt-factory-ap`**.

4. On your **phone or laptop**, open **WiFi settings** and **connect to `nbt-factory-ap`**.
   It's open — there's no password.

5. A **"Sign in to network"** page should appear by itself. If it doesn't, open a web browser
   and go to **`http://192.168.4.1`**.

6. Fill in the three boxes:
   - **WiFi SSID** — the name of your 2.4 GHz WiFi network.
   - **WiFi password** — that network's password.
   - **Recovery URL** — the link support gave you (for example
     `http://updates.neurobeat.com/recovery.bin`).

7. Tap **Connect & recover**.

8. The device connects to your WiFi, downloads the firmware, and restarts.
   - **It worked** when the **`nbt-factory-ap` hotspot disappears** and the device starts up
     normally again. Give it **30–60 seconds**.
   - While it's working it may briefly bump your phone off `nbt-factory-ap` — that's normal.

## If it doesn't work

If the page shows an error, or **`nbt-factory-ap` is still there after a minute**, the
recovery didn't finish. Re-connect to `nbt-factory-ap`, reopen **`http://192.168.4.1`**, and
check:

- the **WiFi password** is correct — a wrong password is the most common cause,
- the network is **2.4 GHz** (not a 5 GHz-only network),
- the **Recovery URL** is exactly what support gave you (no typos, no missing characters).

Then tap **Connect & recover** again.

## Good to know

- **You can't brick the device with this.** Recovery installs the new firmware into a spare
  area and only switches to it after the firmware passes its own checks. The recovery tool
  itself is never overwritten, so if anything goes wrong you can always start over.
- **Only genuine Neurobeat firmware will install.** The device checks that the firmware is
  signed by Neurobeat before running it. If a link points to the wrong or a tampered file,
  the device simply refuses it and stays in recovery — it will never run unverified firmware.
  *(This is also why the plain `http://` link is safe: the firmware itself is encrypted and
  signed, so the connection doesn't need to be.)*

---

### Technician note

- **Recovery mode = the bootloader's GPIO factory-reset:** hold the recovery button (wired to
  **GPIO 14**, active-low, button to GND) during power-on. The unit must be built with
  `CONFIG_BOOTLOADER_FACTORY_RESET=y` and `CONFIG_BOOTLOADER_NUM_PIN_FACTORY_RESET=14`.
- The factory image is **also** reached automatically when both OTA slots are unbootable.
- **WiFi:** 2.4 GHz, **WPA2-PSK or open** only — no WPA3, OWE, or enterprise.
- The **Recovery URL** must be reachable over plain **HTTP/1.0** (no chunked
  transfer-encoding) and serve a **Secure-Boot-signed** `.bin`. The host needs no TLS;
  authenticity is the signature, which `esp_ota_end` verifies before switching and the
  bootloader re-verifies at every boot.
