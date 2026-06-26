import { Stack, StackProps, CfnOutput, RemovalPolicy } from 'aws-cdk-lib';
import { Construct } from 'constructs';
import * as iot from 'aws-cdk-lib/aws-iot';
import * as s3 from 'aws-cdk-lib/aws-s3';
import * as iam from 'aws-cdk-lib/aws-iam';

export interface OtaStackProps extends StackProps {
  thingName: string;
}

/**
 * Provisions the *resources* for the ESP32-S3 Modular OTA POC:
 *   - an IoT Thing + thing group
 *   - a versioned S3 bucket for firmware artifacts (afr-ota* name)
 *   - the thing-scoped device IoT policy (app topics + reserved jobs/streams)
 *   - the OTA service role AWS IoT assumes for CreateOTAUpdate
 *
 * The *actions* that need a private key or operate on a specific build live in
 * scripts/ (device cert registration, the Signer profile, sign+publish, the OTA
 * update). See ../README.md.
 */
export class Esp32OtaPocStack extends Stack {
  constructor(scope: Construct, id: string, props: OtaStackProps) {
    super(scope, id, props);

    const thingName = props.thingName;
    const region = this.region;
    const account = this.account;

    // --- Thing + thing group --------------------------------------------------
    const thingGroup = new iot.CfnThingGroup(this, 'ThingGroup', {
      thingGroupName: `${thingName}-group`,
    });
    const thing = new iot.CfnThing(this, 'Thing', { thingName });

    // --- Firmware bucket ------------------------------------------------------
    // The "afr-ota" name prefix is covered by the AmazonFreeRTOSOTAUpdate managed
    // policy's S3 grant. Versioning is REQUIRED — OTA/Signer reference the object
    // version id.
    const bucket = new s3.Bucket(this, 'FirmwareBucket', {
      bucketName: `afr-ota-${thingName}-${account}-${region}`.toLowerCase(),
      versioned: true,
      blockPublicAccess: s3.BlockPublicAccess.BLOCK_ALL,
      encryption: s3.BucketEncryption.S3_MANAGED,
      enforceSSL: true,
      removalPolicy: RemovalPolicy.DESTROY, // POC convenience
      autoDeleteObjects: true,
    });

    // --- Device IoT policy ----------------------------------------------------
    // ${iot:Connection.Thing.ThingName} is an AWS IoT policy variable — keep it a
    // literal string (single quotes => no TS interpolation). It resolves to the
    // connecting thing only because iot:Connect is pinned to clientId == thing
    // name, which is what makes one policy safe for a whole fleet.
    const thingVar = '${iot:Connection.Thing.ThingName}';
    const clientArn = `arn:aws:iot:${region}:${account}:client/${thingVar}`;
    // NOTE: publish/receive use topic/...  ; subscribe uses topicfilter/...
    const pubTopics = [
      `arn:aws:iot:${region}:${account}:topic/dt/${thingName}/*`,
      `arn:aws:iot:${region}:${account}:topic/$aws/things/${thingVar}/jobs/*`,
      `arn:aws:iot:${region}:${account}:topic/$aws/things/${thingVar}/streams/*`,
    ];
    const subFilters = [
      `arn:aws:iot:${region}:${account}:topicfilter/dt/${thingName}/*`,
      `arn:aws:iot:${region}:${account}:topicfilter/$aws/things/${thingVar}/jobs/*`,
      `arn:aws:iot:${region}:${account}:topicfilter/$aws/things/${thingVar}/streams/*`,
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

    // --- OTA service role -----------------------------------------------------
    // Assumed by AWS IoT to create the OTA stream, read S3, and create the job.
    const otaRole = new iam.Role(this, 'OtaServiceRole', {
      roleName: `${thingName}-ota-service-role`,
      assumedBy: new iam.ServicePrincipal('iot.amazonaws.com'),
      managedPolicies: [
        iam.ManagedPolicy.fromAwsManagedPolicyName('service-role/AmazonFreeRTOSOTAUpdate'),
      ],
    });
    // The role must be able to read/pass itself (per the FreeRTOS OTA docs).
    otaRole.addToPolicy(new iam.PolicyStatement({
      actions: ['iam:GetRole', 'iam:PassRole'],
      resources: [otaRole.roleArn],
    }));
    // afr-ota* bucket is already covered by the managed policy, but be explicit.
    otaRole.addToPolicy(new iam.PolicyStatement({
      actions: ['s3:GetObject', 's3:GetObjectVersion', 's3:PutObject'],
      resources: [`${bucket.bucketArn}/*`],
    }));

    // --- Outputs (consumed by scripts/_lib.sh via describe-stacks) ------------
    new CfnOutput(this, 'ThingName', { value: thingName });
    new CfnOutput(this, 'ThingGroupName', { value: thingGroup.thingGroupName! });
    new CfnOutput(this, 'PolicyName', { value: policy.policyName! });
    new CfnOutput(this, 'FirmwareBucketName', { value: bucket.bucketName });
    new CfnOutput(this, 'OtaServiceRoleArn', { value: otaRole.roleArn });
  }
}
