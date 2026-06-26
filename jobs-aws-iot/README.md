# `jobs-aws-iot/` — variant B cloud: AWS IoT Jobs, custom document

Cloud side for [`../jobs-esp`](../jobs-esp/README.md). Uses **AWS IoT Jobs** for
orchestration but a **custom job document** (the device self-downloads from a
presigned S3 URL with esp_https_ota), so — unlike variant C ([`../aws-iot`](../aws-iot/README.md)) —
there is **no AWS Signer, no OTA service role, no `create-ota-update`, no File
Streams topics**.

CDK provisions: Thing + thing group, a versioned S3 bucket, and a device policy
scoped to `$aws/things/<thing>/jobs/*` + `dt/<thing>/*` (no `…/streams/*`).

## Use

```bash
npm install && npx cdk deploy            # Thing, policy (jobs only), S3
scripts/register-device.sh               # cert + AttachThingPrincipal (writes esp certs)
# build the firmware (see ../jobs-esp), then:
scripts/presign-and-upload.sh ../jobs-esp/fixtures/firmware-good-v2.0.0.bin 2.0.0
scripts/create-job.sh  esp32-ota-poc-01 2.0.0     # create-job w/ {"op":"ota","url":<presigned>,...}
scripts/watch-job.sh   esp32-ota-poc-01           # poll to SUCCEEDED / FAILED / TIMED_OUT
```

| script | action |
|--------|--------|
| `register-device.sh` | create + register the device cert, attach policy + Thing |
| `presign-and-upload.sh <bin> <ver>` | upload the build to the versioned S3 bucket |
| `create-job.sh [-g] <target> <ver>` | `aws iot create-job` with a custom `{op,url,target_version}` document (presigns the S3 URL) |
| `watch-job.sh [thing] [jobId]` | poll `describe-job-execution` to a terminal state |

Stack name: `Esp32OtaJobsStack` (distinct from C's `Esp32OtaPocStack`, so both can
coexist). `npx cdk destroy` to tear down.
