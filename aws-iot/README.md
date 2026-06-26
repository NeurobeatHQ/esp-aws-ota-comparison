# `aws-iot/` — cloud infrastructure (AWS CDK + action scripts)

Provisions the AWS IoT Core side of the OTA POC and gives you small scripts to
register the device, create the code-signing material, and push/watch an OTA.

**CDK provisions the *resources*** (a Thing, thing group, versioned S3 bucket,
the thing-scoped device policy, the OTA service role). **Scripts do the
*actions*** that involve a private key or a specific build (device cert
registration, the AWS Signer profile, `create-ota-update`). That split is
deliberate: a device cert and a `AmazonFreeRTOS-Default` Signer profile can't be
created cleanly in CloudFormation.

## Prerequisites

- AWS CLI v2 configured (`aws configure`) with rights for IoT, S3, IAM, ACM, Signer.
- Node 18+ and `jq`.
- `npx cdk bootstrap` once per account/region if you've never used CDK there.

## 1. Deploy the resources

```bash
cd aws-iot
npm install
npx cdk deploy            # creates the Thing, S3 bucket, policy, OTA role
```
Outputs (Thing name, bucket, policy name, OTA role ARN) are read automatically by
the scripts via CloudFormation — no config file to maintain. Override the Thing
name with `npx cdk deploy -c thingName=my-thing`.

Get your ATS endpoint for `esp/src/secrets.h`:
```bash
aws iot describe-endpoint --endpoint-type iot:Data-ATS --query endpointAddress --output text
```

## 2. Register the device certificate

```bash
scripts/register-device.sh
```
This creates keys + certificate, **attaches the policy to the cert**, **attaches
the cert to the Thing (`AttachThingPrincipal`)**, and copies `client.crt` /
`client.key` into `esp/src/certs/`. The `AttachThingPrincipal` step is the one
people miss — without it the cert passes TLS but is refused at MQTT CONNECT under
the thing-scoped policy.

## 3. Create the code-signing certificate + Signer profile

```bash
scripts/make-codesign-cert.sh
```
Generates an ECDSA P-256 cert, imports it into ACM, creates the Signer profile
`esp32_ota_poc_profile` (platform `AmazonFreeRTOS-Default`), and copies the
**public** cert into `esp/src/certs/aws_codesign.crt`.

> After steps 2–3, **rebuild + flash** the firmware so it embeds the device cert
> and the code-signing cert (see [`../esp/README.md`](../esp/README.md)).

## 4. Push and watch an OTA

```bash
# happy path (commits)
scripts/sign-and-publish.sh ../esp/fixtures/firmware-good-v2.0.0.bin 2.0.0
scripts/push-ota.sh   esp32-ota-poc-01 2.0.0
scripts/watch-job.sh  esp32-ota-poc-01            # polls to SUCCEEDED

# rollback path (device rejects -> rolls back)
scripts/sign-and-publish.sh ../esp/fixtures/firmware-bad-v2.0.0.bin 2.0.0-bad
scripts/push-ota.sh   esp32-ota-poc-01 2.0.0-bad
scripts/watch-job.sh  esp32-ota-poc-01            # FAILED/TIMED_OUT; device stays on old image
```
Target a whole group instead of one Thing with `push-ota.sh -g <group> <version>`.

What the scripts do:

| script | action |
|--------|--------|
| `sign-and-publish.sh <bin> <ver>` | upload the build to the versioned S3 bucket |
| `push-ota.sh [-g] <target> <ver>` | `create-ota-update` (Signer signs via the profile, OTA Manager builds the stream + Job) with rollout / abort / timeout config |
| `watch-job.sh [thing] [jobId]`    | poll `describe-job-execution` to a terminal state |

## 5. Teardown

```bash
# detach + delete the device cert(s) created by register-device.sh
#   aws iot list-thing-principals --thing-name esp32-ota-poc-01
#   aws iot detach-thing-principal / detach-policy / update-certificate --new-status INACTIVE / delete-certificate
npx cdk destroy
```
OTA updates/streams/jobs are deleted via the CLI (`aws iot delete-ota-update
--delete-stream --force-delete-aws-job`), not CloudFormation.

## Notes / gotchas

- **`topic/` vs `topicfilter/`** in the device policy: publish/receive use
  `topic/…`, subscribe uses `topicfilter/…`. The CDK policy gets this right for
  the app topics **and** the reserved `$aws/things/<thing>/jobs/*` and `…/streams/*`.
- **S3 versioning is required** — `create-ota-update`/Signer reference the object
  version id. The bucket is created versioned.
- **`afr-ota` bucket prefix** is covered by the `AmazonFreeRTOSOTAUpdate` managed
  policy on the OTA role (the bucket is named `afr-ota-…`).
- **Signer platform availability is regional** — confirm with
  `aws signer list-signing-platforms | jq '.platforms[].platformId'` if a region
  rejects `AmazonFreeRTOS-Default`.
