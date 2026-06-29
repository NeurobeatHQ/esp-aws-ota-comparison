#!/usr/bin/env node
import 'source-map-support/register';
import * as cdk from 'aws-cdk-lib';
import { Esp32OtaStack, Backend } from '../lib/ota-stack';

const app = new cdk.App();

// Backend selection mirrors the firmware's `pio run -e <backend>`:
//   BACKEND=jobs npx cdk deploy      OR      npx cdk deploy -c backend=jobs
const backend = (process.env.BACKEND ?? app.node.tryGetContext('backend') ?? 'mqtt') as Backend;
const valid: Backend[] = ['mqtt', 'https', 'jobs', 'manual'];
if (!valid.includes(backend)) {
  throw new Error(`backend must be one of ${valid.join(', ')} (got '${backend}')`);
}

const thingName = process.env.THING_NAME ?? app.node.tryGetContext('thingName') ?? 'esp32-ota-poc-01';

// ONE stack, swapped in place: `BACKEND=jobs npx cdk deploy` updates the same
// Esp32OtaStack — the Thing, device cert, and S3 bucket persist; only the device
// policy scope and the OTA service role (mqtt/https only) change. (Run backends in
// parallel by overriding THING_NAME, which also makes the stack name distinct.)
const stackName = thingName === 'esp32-ota-poc-01' ? 'Esp32OtaStack' : `Esp32OtaStack-${thingName}`;

new Esp32OtaStack(app, stackName, {
  thingName,
  backend,
  env: { account: process.env.CDK_DEFAULT_ACCOUNT, region: process.env.CDK_DEFAULT_REGION },
  description: `ESP32-S3 OTA cloud — backend '${backend}'`,
});
