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
| `upload-firmware.sh` · `_lib.sh` | ✅ | ✅ | ✅ | ✅ |
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
# device identity: BYO-CA cert (CN=<thing>) + provision-secure-cert.sh — see "Fleet" below
# (the same steps serve one board or many; no per-board recompile).
BACKEND=$B scripts/make-codesign-cert.sh # (mqtt/https only; no-ops otherwise)
# build + flash the matching firmware:  (cd ../esp && pio run -e $B -t upload)
BACKEND=$B scripts/upload-firmware.sh ../esp/fixtures/firmware-$B-good-v2.0.0.bin 2.0.0
BACKEND=$B scripts/push-update.sh esp32-ota-poc-01 2.0.0
BACKEND=$B scripts/watch.sh       esp32-ota-poc-01
```

| script | shared? | action |
|--------|:--:|--------|
| `make-codesign-cert.sh` | mqtt/https | ECDSA P-256 cert + ACM import + Signer profile |
| `upload-firmware.sh` | ✅ | upload a build to the versioned S3 bucket |
| `push-update.sh` | dispatch | create-ota-update / create-job / publish-plan |
| `watch.sh` | dispatch | poll the Job execution, or subscribe to the confirm topic |

Teardown: `BACKEND=$B npx cdk destroy` (detach + delete the device cert first; OTA
updates/jobs are CLI-deleted).

## Gotchas

- **Use `npx cdk`, never a bare `cdk`.** `aws-cdk-lib` floats on `^`, so a global
  `cdk` can be older than the library and fail with *"Cloud assembly schema version
  mismatch … upgrade the CLI."* `npx cdk` / `npm run deploy` use the version-matched
  CLI in `node_modules`. (Fix the global too, if you like: `npm i -g aws-cdk@latest`.)
- **Always set `BACKEND=`.** A bare `npx cdk deploy` **defaults to `mqtt`** — so an
  unset `BACKEND` silently deploys the wrong backend (e.g. adds MQTT File Streams
  grants to an https fleet). Set it to match the firmware you flash.

## Fleet — N boards, one OTA push

Each board must be its **own Thing**: the policy pins `clientId ==
${iot:Connection.Thing.ThingName}`, so two boards can't share a clientId. The CDK
already creates the **thing group** (`<thing>-group`) and one **policy** scoped with
the `${iot:Connection.Thing.ThingName}` variable (so it serves *every* Thing). What's
left is per-board identity + populating the group.

**Identity = bring-your-own CA**, so the Thing name rides in the cert (`CN=<thing>`)
and **one firmware image serves the whole fleet** (no per-board recompile — the
firmware reads its Thing name from the cert's CN):

```bash
# once — your device CA (keep deviceCA.key secret; gitignore it)
openssl genrsa -out deviceCA.key 2048
openssl req -x509 -new -nodes -key deviceCA.key -sha256 -days 3650 -subj "/CN=deviceCA" -out deviceCA.crt

# per board — THING = esp32-ota-poc-01 | -02 | -03
openssl genrsa -out "$THING.key" 2048
openssl req -new -key "$THING.key" -subj "/CN=$THING" -out "$THING.csr"
openssl x509 -req -in "$THING.csr" -CA deviceCA.crt -CAkey deviceCA.key \
  -CAcreateserial -days 3650 -sha256 -out "$THING.crt"
CERT_ARN=$(aws iot register-certificate-without-ca \
  --certificate-pem "file://$THING.crt" --status ACTIVE --query certificateArn --output text)
aws iot create-thing             --thing-name "$THING"
aws iot attach-policy            --policy-name "$(aws cloudformation describe-stacks --stack-name Esp32OtaStack --query "Stacks[0].Outputs[?OutputKey=='PolicyName'].OutputValue" --output text)" --target "$CERT_ARN"
aws iot attach-thing-principal   --thing-name "$THING" --principal "$CERT_ARN"
aws iot add-thing-to-thing-group --thing-group-name "$(aws cloudformation describe-stacks --stack-name Esp32OtaStack --query "Stacks[0].Outputs[?OutputKey=='ThingGroupName'].OutputValue" --output text)" --thing-name "$THING"

# provision THIS board's identity into its esp_secure_cert partition (no recompile).
# Pass the board's cert + key as args — no need to copy them into src/certs/ first.
(cd ../esp && scripts/provision-secure-cert.sh "../aws-iot/$THING.crt" "../aws-iot/$THING.key" -p <PORT>)  # + --ds for the DS peripheral
```

Then **build once, flash the same image to every board**, and **push once to the group**:

```bash
(cd ../esp && pio run -e https)   # one image, identity comes from each board's esp_secure_cert
BACKEND=https scripts/upload-firmware.sh ../esp/fixtures/firmware-https-good-v2.0.0.bin 2.0.0
BACKEND=https scripts/push-update.sh -g "$(aws cloudformation describe-stacks --stack-name Esp32OtaStack \
  --query "Stacks[0].Outputs[?OutputKey=='ThingGroupName'].OutputValue" --output text)" 2.0.0
```

Confirm each board upgraded three ways: the **LED blink count** (v1 → v2), the **job
execution** reaching `SUCCEEDED` per Thing, and the **heartbeat** `"fw":"2.0.0"` on
`dt/<thing>/heartbeat`.

> The BYO-CA flow above is the **only** identity path — it serves one board or a whole
> fleet. The device's Thing name is its cert's `CN`, so a cert whose `CN` isn't a real
> Thing makes the board run **offline** (it never impersonates another Thing). There is
> no AWS-issued / embedded-cert shortcut: identity always comes from `esp_secure_cert`.

