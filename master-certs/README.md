# `master-certs/` — device CA + per-board certificates

This directory holds the concrete identity artifacts for the fleet:

- `deviceCA.key` / `deviceCA.crt` — your bring-your-own device CA (keep `deviceCA.key`
  secret; it is `.gitignore`d).
- `esp32-ota-poc-0N.{key,csr,crt}` — per-board private key, CSR, and cert (`CN=<thing>`),
  signed by the device CA.

## How to (re)generate and use them

The full, authoritative procedure is the **[Fleet — N boards, one OTA push](../aws-iot/README.md#fleet--n-boards-one-ota-push)**
section of `aws-iot/README.md`. It is the single source of truth for:

- minting the device CA and each board's key + cert,
- registering the cert / Thing / policy / thing group in AWS (resolving the policy and
  group names from the CloudFormation stack outputs, not hardcoded), and
- provisioning a board with
  `provision-secure-cert.sh <thing>.crt <thing>.key -p <PORT>` (which writes the cert +
  key into the on-flash `esp_secure_cert` partition — no copying into `esp/src/certs/`),
  then building **one** image (`pio run -e https`) that serves every board.

Nothing device-specific is embedded in the firmware: each board's identity (cert, key,
and Thing name = the cert's CN) is read at runtime from its `esp_secure_cert` partition.
