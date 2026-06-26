# `manual-aws-iot/` — variant A cloud: custom protocol, operator-driven

Cloud side for [`../manual-esp`](../manual-esp/README.md). **No AWS IoT Jobs.** The
device runs a custom MQTT request/response protocol; an **operator** drives updates
with the scripts here. AWS IoT Core is used purely as the MQTT transport.

CDK provisions the minimum: a Thing, a versioned S3 bucket, and a device policy
scoped to the **app topics only** (`dt/<thing>/*`) — no `jobs/*`, no `streams/*`,
no Signer, no OTA role.

## Use

```bash
npm install && npx cdk deploy            # Thing, app-topics-only policy, S3
scripts/register-device.sh               # cert + AttachThingPrincipal (writes esp certs)
# build the firmware (see ../manual-esp), then drive an update by hand:
scripts/presign-and-upload.sh ../manual-esp/fixtures/firmware-good-v2.0.0.bin 2.0.0
scripts/watch-confirm.sh  esp32-ota-poc-01 &    # watch the device's confirm
scripts/publish-plan.sh   esp32-ota-poc-01 2.0.0  # push {op,url,ota_id} to dt/<thing>/ota/plan
```

| script | action |
|--------|--------|
| `register-device.sh` | create + register the device cert, attach policy + Thing |
| `presign-and-upload.sh <bin> <ver>` | upload the build to the versioned S3 bucket |
| `publish-plan.sh <thing> <ver>` | presign + `aws iot-data publish` the update plan to `dt/<thing>/ota/plan` |
| `watch-confirm.sh <thing>` | subscribe to `dt/<thing>/ota/confirm` (awsiotsdk WebSocket/SigV4, or use the console MQTT test client) |

This is the operator-driven stand-in for the §A custom server (the device's
`should-i-update` is answered by `publish-plan.sh` by hand). A reactive long-running
server is out of scope for the POC. Stack name: `Esp32OtaManualStack`.
`npx cdk destroy` to tear down.
