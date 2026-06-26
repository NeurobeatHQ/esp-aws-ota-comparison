import { Stack, StackProps, CfnOutput, RemovalPolicy } from 'aws-cdk-lib';
import { Construct } from 'constructs';
import * as iot from 'aws-cdk-lib/aws-iot';
import * as s3 from 'aws-cdk-lib/aws-s3';

export interface OtaStackProps extends StackProps {
  thingName: string;
}

/**
 * Variant B (jobs-aws-iot) — AWS IoT Jobs with a CUSTOM job document, no Signer.
 *
 * Lighter than variant C (../aws-iot): the device self-downloads from a presigned
 * S3 URL via esp_https_ota, so there is **no AWS MQTT File Streams** (drop the
 * `…/streams/*` topics) and **no OTA service role / Signer** (a custom-doc
 * `create-job` needs neither). Just: Thing, versioned S3 bucket, and a device
 * policy scoped to `jobs/*` + the app topics.
 */
export class Esp32OtaPocStack extends Stack {
  constructor(scope: Construct, id: string, props: OtaStackProps) {
    super(scope, id, props);

    const thingName = props.thingName;
    const region = this.region;
    const account = this.account;

    const thingGroup = new iot.CfnThingGroup(this, 'ThingGroup', {
      thingGroupName: `${thingName}-group`,
    });
    const thing = new iot.CfnThing(this, 'Thing', { thingName });

    const bucket = new s3.Bucket(this, 'FirmwareBucket', {
      bucketName: `fw-${thingName}-${account}-${region}`.toLowerCase(),
      versioned: true,                       // required for stable presigned URLs
      blockPublicAccess: s3.BlockPublicAccess.BLOCK_ALL,
      encryption: s3.BucketEncryption.S3_MANAGED,
      enforceSSL: true,
      removalPolicy: RemovalPolicy.DESTROY,
      autoDeleteObjects: true,
    });

    // Device policy — jobs + app topics only. NO streams (no File Streams in B).
    const thingVar = '${iot:Connection.Thing.ThingName}';
    const clientArn = `arn:aws:iot:${region}:${account}:client/${thingVar}`;
    const pubTopics = [
      `arn:aws:iot:${region}:${account}:topic/dt/${thingName}/*`,
      `arn:aws:iot:${region}:${account}:topic/$aws/things/${thingVar}/jobs/*`,
    ];
    const subFilters = [
      `arn:aws:iot:${region}:${account}:topicfilter/dt/${thingName}/*`,
      `arn:aws:iot:${region}:${account}:topicfilter/$aws/things/${thingVar}/jobs/*`,
    ];

    const policy = new iot.CfnPolicy(this, 'DevicePolicy', {
      policyName: `${thingName}-policy`,
      policyDocument: {
        Version: '2012-10-17',
        Statement: [
          { Effect: 'Allow', Action: 'iot:Connect', Resource: clientArn },
          { Effect: 'Allow', Action: ['iot:Publish', 'iot:Receive'], Resource: pubTopics },
          { Effect: 'Allow', Action: 'iot:Subscribe', Resource: subFilters },
        ],
      },
    });

    new CfnOutput(this, 'ThingName', { value: thingName });
    new CfnOutput(this, 'ThingGroupName', { value: thingGroup.thingGroupName! });
    new CfnOutput(this, 'PolicyName', { value: policy.policyName! });
    new CfnOutput(this, 'FirmwareBucketName', { value: bucket.bucketName });
  }
}
