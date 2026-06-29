# `aws-iot/` — composite OTA cloud infrastructure

One CDK app + one set of scripts for **all four OTA backends**, selected with a
`BACKEND` switch that mirrors the firmware's `pio run -e <backend>`:

```bash
npm install
BACKEND=mqtt   npx cdk deploy     # C: agent, MQTT File Streams, Signer
BACKEND=https  npx cdk deploy     # D: agent, HTTPS data path,   Signer
BACKEND=jobs   npx cdk deploy     # B: AWS IoT Jobs (custom doc), no Signer
BACKEND=manual npx cdk deploy     # A: custom MQTT protocol,      no Jobs
```
(or `npx cdk deploy -c backend=jobs`). It's **one** `Esp32OtaStack` swapped in
place: re-deploying with a different `BACKEND` keeps the Thing, the registered
device cert, and the S3 bucket, and only updates the device-policy scope and the
OTA service role (added for mqtt/https, removed for jobs/manual). So registering a
device once lets you swap backends with just a re-deploy + a re-flash. (To run two
backends in parallel, give each its own `THING_NAME` — that also forks the stack
name.)

## What's shared vs per-backend

| resource / action | mqtt | https | jobs | manual |
|---|:--:|:--:|:--:|:--:|
| Thing + thing group + versioned S3 bucket | ✅ | ✅ | ✅ | ✅ |
| `register-device.sh` · `upload-firmware.sh` · `_lib.sh` | ✅ | ✅ | ✅ | ✅ |
| device policy: app topics `dt/<thing>/*` | ✅ | ✅ | ✅ | ✅ |
| device policy: `$aws/things/<thing>/jobs/*` | ✅ | ✅ | ✅ | — |
| device policy: `…/streams/*` (MQTT File Streams) | ✅ | — | — | — |
| OTA service role + AWS Signer profile | ✅ | ✅ | — | — |
| push action | `create-ota-update --protocols MQTT` | `… --protocols HTTP` | `create-job` (custom doc) | `iot-data publish` (plan) |
| watch action | poll Job execution | poll Job execution | poll Job execution | subscribe `…/ota/confirm` |

The CDK stack ([`lib/ota-stack.ts`](lib/ota-stack.ts)) grows the device policy and
adds the OTA role only for the backends that need them; the scripts dispatch on
`BACKEND`. Unused-in-some-cases is fine and explicit (e.g. `make-codesign-cert.sh`
no-ops for `jobs`/`manual`).

## End-to-end (any backend)

```bash
B=jobs                                   # pick a backend
BACKEND=$B npx cdk deploy
BACKEND=$B scripts/register-device.sh    # cert + AttachThingPrincipal -> esp/src/certs/
BACKEND=$B scripts/make-codesign-cert.sh # (mqtt/https only; no-ops otherwise)
# build + flash the matching firmware:  (cd ../esp && pio run -e $B -t upload)
BACKEND=$B scripts/upload-firmware.sh ../esp/fixtures/firmware-$B-good-v2.0.0.bin 2.0.0
BACKEND=$B scripts/push-update.sh esp32-ota-poc-01 2.0.0
BACKEND=$B scripts/watch.sh       esp32-ota-poc-01
```

| script | shared? | action |
|--------|:--:|--------|
| `register-device.sh` | ✅ | create + register the device cert, attach policy + Thing |
| `make-codesign-cert.sh` | mqtt/https | ECDSA P-256 cert + ACM import + Signer profile |
| `upload-firmware.sh` | ✅ | upload a build to the versioned S3 bucket |
| `push-update.sh` | dispatch | create-ota-update / create-job / publish-plan |
| `watch.sh` | dispatch | poll the Job execution, or subscribe to the confirm topic |

Teardown: `BACKEND=$B npx cdk destroy` (detach + delete the device cert first; OTA
updates/jobs are CLI-deleted).
