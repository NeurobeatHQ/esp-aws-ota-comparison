#!/usr/bin/env node
import 'source-map-support/register';
import * as cdk from 'aws-cdk-lib';
import { Esp32OtaPocStack } from '../lib/ota-stack';

const app = new cdk.App();

// Thing name can be overridden:  cdk deploy -c thingName=my-thing
const thingName = app.node.tryGetContext('thingName') ?? 'esp32-ota-poc-01';

new Esp32OtaPocStack(app, 'Esp32OtaPocStack', {
  thingName,
  env: {
    account: process.env.CDK_DEFAULT_ACCOUNT,
    region: process.env.CDK_DEFAULT_REGION,
  },
  description: 'ESP32-S3 AWS IoT Core Modular OTA POC — Thing, policy, S3, OTA service role',
});
