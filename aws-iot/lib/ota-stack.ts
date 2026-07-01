import { Stack, StackProps, CfnOutput, RemovalPolicy } from 'aws-cdk-lib';
import { Construct } from 'constructs';
import * as iot from 'aws-cdk-lib/aws-iot';
import * as s3 from 'aws-cdk-lib/aws-s3';
import * as iam from 'aws-cdk-lib/aws-iam';

export type Backend = 'mqtt' | 'https' | 'jobs' | 'manual';

export interface OtaStackProps extends StackProps {
  thingName: string;
  backend: Backend;
}

/**
 * Composite OTA infrastructure — ONE stack, four backends (mirrors the firmware's
 * `pio run -e <backend>`). Reused across all four: the Thing, the thing group, and
 * the versioned firmware S3 bucket. Backend-specific (and simply absent otherwise):
 *
 *   device policy scope :  app topics (all) + jobs/* (mqtt,https,jobs) + streams/* (mqtt)
 *   OTA service role    :  mqtt, https only (create-ota-update assumes it)
 *
 * Deploy:  BACKEND=jobs npx cdk deploy     (or: npx cdk deploy -c backend=jobs)
 */
export class Esp32OtaStack extends Stack {
  constructor(scope: Construct, id: string, props: OtaStackProps) {
    super(scope, id, props);

    const { thingName, backend } = props;
    const region = this.region;
    const account = this.account;

    const usesJobs = backend === 'mqtt' || backend === 'https' || backend === 'jobs';
    const usesStreams = backend === 'mqtt';                       // AWS MQTT File Streams
    const usesSigner = backend === 'mqtt' || backend === 'https'; // create-ota-update + Signer

    // --- shared resources ----------------------------------------------------
    // The Thing + ThingGroup are declared here, but group membership
    // (CfnThingGroupMembership) and cert attach (CfnThingPrincipalAttachment)
    // are INTENTIONALLY not in-stack: the BYO-CA scripts register the device
    // cert (CN = Thing name), attach it, and join the group out-of-band. This is
    // by design, not an omission — see aws-iot/README.md "Fleet".
    const thingGroup = new iot.CfnThingGroup(this, 'ThingGroup', {
      thingGroupName: `${thingName}-group`,
    });
    new iot.CfnThing(this, 'Thing', { thingName });

    const bucket = new s3.Bucket(this, 'FirmwareBucket', {
      bucketName: `fw-${thingName}-${account}-${region}`.toLowerCase(),
      versioned: true,                       // OTA/Signer + presign reference the object version
      blockPublicAccess: s3.BlockPublicAccess.BLOCK_ALL,
      encryption: s3.BucketEncryption.S3_MANAGED,
      enforceSSL: true,
      removalPolicy: RemovalPolicy.DESTROY,
      autoDeleteObjects: true,
    });

    // --- device policy (scope grows with the backend) ------------------------
    const thingVar = '${iot:Connection.Thing.ThingName}';
    const pub: string[] = [`arn:aws:iot:${region}:${account}:topic/dt/${thingVar}/*`];
    const sub: string[] = [`arn:aws:iot:${region}:${account}:topicfilter/dt/${thingVar}/*`];
    if (usesJobs) {
      pub.push(`arn:aws:iot:${region}:${account}:topic/$aws/things/${thingVar}/jobs/*`);
      sub.push(`arn:aws:iot:${region}:${account}:topicfilter/$aws/things/${thingVar}/jobs/*`);
    }
    if (usesStreams) {
      pub.push(`arn:aws:iot:${region}:${account}:topic/$aws/things/${thingVar}/streams/*`);
      sub.push(`arn:aws:iot:${region}:${account}:topicfilter/$aws/things/${thingVar}/streams/*`);
    }
    // Name is backend-INDEPENDENT: swapping the backend updates this policy's
    // *document* in place, so the device cert (attached out-of-band by the
    // BYO-CA provisioning flow) keeps working — no detach/rename churn.
    const policy = new iot.CfnPolicy(this, 'DevicePolicy', {
      policyName: `${thingName}-policy`,
      policyDocument: {
        Version: '2012-10-17',
        Statement: [
          { Effect: 'Allow', Action: 'iot:Connect',
            Resource: `arn:aws:iot:${region}:${account}:client/${thingVar}` },
          { Effect: 'Allow', Action: ['iot:Publish', 'iot:Receive'], Resource: pub },
          { Effect: 'Allow', Action: 'iot:Subscribe', Resource: sub },
        ],
      },
    });

    // --- OTA service role (mqtt/https only) ----------------------------------
    let otaRoleArn: string | undefined;
    if (usesSigner) {
      const otaRole = new iam.Role(this, 'OtaServiceRole', {
        roleName: `${thingName}-ota-role`,
        assumedBy: new iam.ServicePrincipal('iot.amazonaws.com'),
        managedPolicies: [
          iam.ManagedPolicy.fromAwsManagedPolicyName('service-role/AmazonFreeRTOSOTAUpdate'),
        ],
      });
      otaRole.addToPolicy(new iam.PolicyStatement({
        actions: ['iam:GetRole', 'iam:PassRole'], resources: [otaRole.roleArn],
      }));
      otaRole.addToPolicy(new iam.PolicyStatement({
        actions: ['s3:GetObject', 's3:GetObjectVersion', 's3:PutObject'],
        resources: [`${bucket.bucketArn}/*`],
      }));
      otaRoleArn = otaRole.roleArn;
    }

    // --- outputs (consumed by scripts/_lib.sh) -------------------------------
    new CfnOutput(this, 'Backend', { value: backend });
    new CfnOutput(this, 'ThingName', { value: thingName });
    new CfnOutput(this, 'ThingGroupName', { value: thingGroup.thingGroupName! });
    new CfnOutput(this, 'PolicyName', { value: policy.policyName! });
    new CfnOutput(this, 'FirmwareBucketName', { value: bucket.bucketName });
    if (otaRoleArn) {
      new CfnOutput(this, 'OtaServiceRoleArn', { value: otaRoleArn });
    }
  }
}
