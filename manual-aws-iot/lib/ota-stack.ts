import { Stack, StackProps, CfnOutput, RemovalPolicy } from 'aws-cdk-lib';
import { Construct } from 'constructs';
import * as iot from 'aws-cdk-lib/aws-iot';
import * as s3 from 'aws-cdk-lib/aws-s3';

export interface OtaStackProps extends StackProps {
  thingName: string;
}

/**
 * Variant A (manual-aws-iot) — fully custom MQTT control protocol, no AWS Jobs.
 *
 * Lightest cloud footprint: just a Thing, a versioned S3 bucket, and a device
 * policy scoped to the APP topics only (`dt/<thing>/*`) — NO reserved
 * `$aws/things/<thing>/jobs/*` or `…/streams/*`, no Signer, no OTA role. AWS IoT
 * Core is used purely as the MQTT transport for the custom protocol; an operator
 * drives updates with the scripts/.
 */
export class Esp32OtaPocStack extends Stack {
  constructor(scope: Construct, id: string, props: OtaStackProps) {
    super(scope, id, props);

    const thingName = props.thingName;
    const region = this.region;
    const account = this.account;

    const thing = new iot.CfnThing(this, 'Thing', { thingName });

    const bucket = new s3.Bucket(this, 'FirmwareBucket', {
      bucketName: `fw-${thingName}-${account}-${region}`.toLowerCase(),
      versioned: true,
      blockPublicAccess: s3.BlockPublicAccess.BLOCK_ALL,
      encryption: s3.BucketEncryption.S3_MANAGED,
      enforceSSL: true,
      removalPolicy: RemovalPolicy.DESTROY,
      autoDeleteObjects: true,
    });

    // Device policy — APP topics only. The custom OTA protocol lives under
    // dt/<thing>/ota/*; the heartbeat under dt/<thing>/heartbeat.
    const thingVar = '${iot:Connection.Thing.ThingName}';
    const clientArn = `arn:aws:iot:${region}:${account}:client/${thingVar}`;
    const appPubTopic = `arn:aws:iot:${region}:${account}:topic/dt/${thingName}/*`;
    const appSubFilter = `arn:aws:iot:${region}:${account}:topicfilter/dt/${thingName}/*`;

    const policy = new iot.CfnPolicy(this, 'DevicePolicy', {
      policyName: `${thingName}-policy`,
      policyDocument: {
        Version: '2012-10-17',
        Statement: [
          { Effect: 'Allow', Action: 'iot:Connect', Resource: clientArn },
          { Effect: 'Allow', Action: ['iot:Publish', 'iot:Receive'], Resource: appPubTopic },
          { Effect: 'Allow', Action: 'iot:Subscribe', Resource: appSubFilter },
        ],
      },
    });

    new CfnOutput(this, 'ThingName', { value: thingName });
    new CfnOutput(this, 'PolicyName', { value: policy.policyName! });
    new CfnOutput(this, 'FirmwareBucketName', { value: bucket.bucketName });
  }
}
