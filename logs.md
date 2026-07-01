`% BACKEND=https npx cdk deploy`

```
✨  Synthesis time: 3.78s

current credentials could not be used to assume 'arn:aws:iam::214280727683:role/cdk-hnb659fds-file-publishing-role-214280727683-us-east-1', but are for the right account. Proceeding anyway.
current credentials could not be used to assume 'arn:aws:iam::214280727683:role/cdk-hnb659fds-file-publishing-role-214280727683-us-east-1', but are for the right account. Proceeding anyway.
current credentials could not be used to assume 'arn:aws:iam::214280727683:role/cdk-hnb659fds-deploy-role-214280727683-us-east-1', but are for the right account. Proceeding anyway.
Esp32OtaStack: start: Building Esp32OtaStack Template
Esp32OtaStack: success: Built Esp32OtaStack Template
Esp32OtaStack: start: Publishing Esp32OtaStack Template (214280727683-us-east-1-03a7508a)
Esp32OtaStack: success: Published Esp32OtaStack Template (214280727683-us-east-1-03a7508a)
current credentials could not be used to assume 'arn:aws:iam::214280727683:role/cdk-hnb659fds-deploy-role-214280727683-us-east-1', but are for the right account. Proceeding anyway.
Esp32OtaStack: creating CloudFormation changeset...
Changeset arn:aws:cloudformation:us-east-1:214280727683:changeSet/cdk-deploy-change-set/210baa72-180f-4adc-b42e-3163cd6fb447 created and waiting in review for manual execution (--no-execute)
current credentials could not be used to assume 'arn:aws:iam::214280727683:role/cdk-hnb659fds-lookup-role-214280727683-us-east-1', but are for the right account. Proceeding anyway.
Lookup role arn:aws:iam::214280727683:role/cdk-hnb659fds-lookup-role-214280727683-us-east-1 was not assumed. Proceeding with default credentials.
Stack Esp32OtaStack
IAM Statement Changes
┌───┬────────────────────────────┬────────┬────────────────────────────┬────────────────────────────┬────────────────────────────┐
│   │ Resource                   │ Effect │ Action                     │ Principal                  │ Condition                  │
├───┼────────────────────────────┼────────┼────────────────────────────┼────────────────────────────┼────────────────────────────┤
│ + │ ${Custom::S3AutoDeleteObje │ Allow  │ sts:AssumeRole             │ Service:lambda.amazonaws.c │                            │
│   │ ctsCustomResourceProvider/ │        │                            │ om                         │                            │
│   │ Role.Arn}                  │        │                            │                            │                            │
├───┼────────────────────────────┼────────┼────────────────────────────┼────────────────────────────┼────────────────────────────┤
│ + │ ${FirmwareBucket.Arn}      │ Deny   │ s3:*                       │ AWS:*                      │ "Bool": {                  │
│   │ ${FirmwareBucket.Arn}/*    │        │                            │                            │   "aws:SecureTransport": " │
│   │                            │        │                            │                            │ false"                     │
│   │                            │        │                            │                            │ }                          │
│ + │ ${FirmwareBucket.Arn}      │ Allow  │ s3:DeleteObject*           │ AWS:${Custom::S3AutoDelete │                            │
│   │ ${FirmwareBucket.Arn}/*    │        │ s3:GetBucket*              │ ObjectsCustomResourceProvi │                            │
│   │                            │        │ s3:List*                   │ der/Role.Arn}              │                            │
│   │                            │        │ s3:PutBucketPolicy         │                            │                            │
├───┼────────────────────────────┼────────┼────────────────────────────┼────────────────────────────┼────────────────────────────┤
│ + │ ${FirmwareBucket.Arn}/*    │ Allow  │ s3:GetObject               │ AWS:${OtaServiceRole}      │                            │
│   │                            │        │ s3:GetObjectVersion        │                            │                            │
│   │                            │        │ s3:PutObject               │                            │                            │
├───┼────────────────────────────┼────────┼────────────────────────────┼────────────────────────────┼────────────────────────────┤
│ + │ ${OtaServiceRole.Arn}      │ Allow  │ sts:AssumeRole             │ Service:iot.amazonaws.com  │                            │
│ + │ ${OtaServiceRole.Arn}      │ Allow  │ iam:GetRole                │ AWS:${OtaServiceRole}      │                            │
│   │                            │        │ iam:PassRole               │                            │                            │
├───┼────────────────────────────┼────────┼────────────────────────────┼────────────────────────────┼────────────────────────────┤
│ + │ arn:aws:iot:us-east-1:2142 │ Allow  │ iot:Connect                │                            │                            │
│   │ 80727683:client/${iot:Conn │        │                            │                            │                            │
│   │ ection.Thing.ThingName}    │        │                            │                            │                            │
├───┼────────────────────────────┼────────┼────────────────────────────┼────────────────────────────┼────────────────────────────┤
│ + │ arn:aws:iot:us-east-1:2142 │ Allow  │ iot:Publish                │                            │                            │
│   │ 80727683:topic/$aws/things │        │ iot:Receive                │                            │                            │
│   │ /${iot:Connection.Thing.Th │        │                            │                            │                            │
│   │ ingName}/jobs/*            │        │                            │                            │                            │
│   │ arn:aws:iot:us-east-1:2142 │        │                            │                            │                            │
│   │ 80727683:topic/dt/esp32-ot │        │                            │                            │                            │
│   │ a-poc-01/*                 │        │                            │                            │                            │
├───┼────────────────────────────┼────────┼────────────────────────────┼────────────────────────────┼────────────────────────────┤
│ + │ arn:aws:iot:us-east-1:2142 │ Allow  │ iot:Subscribe              │                            │                            │
│   │ 80727683:topicfilter/$aws/ │        │                            │                            │                            │
│   │ things/${iot:Connection.Th │        │                            │                            │                            │
│   │ ing.ThingName}/jobs/*      │        │                            │                            │                            │
│   │ arn:aws:iot:us-east-1:2142 │        │                            │                            │                            │
│   │ 80727683:topicfilter/dt/es │        │                            │                            │                            │
│   │ p32-ota-poc-01/*           │        │                            │                            │                            │
└───┴────────────────────────────┴────────┴────────────────────────────┴────────────────────────────┴────────────────────────────┘
IAM Policy Changes
┌───┬─────────────────────────────────────────────────────────────┬──────────────────────────────────────────────────────────────┐
│   │ Resource                                                    │ Managed Policy ARN                                           │
├───┼─────────────────────────────────────────────────────────────┼──────────────────────────────────────────────────────────────┤
│ + │ ${Custom::S3AutoDeleteObjectsCustomResourceProvider/Role}   │ {"Fn::Sub":"arn:${AWS::Partition}:iam::aws:policy/service-ro │
│   │                                                             │ le/AWSLambdaBasicExecutionRole"}                             │
├───┼─────────────────────────────────────────────────────────────┼──────────────────────────────────────────────────────────────┤
│ + │ ${OtaServiceRole}                                           │ arn:${AWS::Partition}:iam::aws:policy/service-role/AmazonFre │
│   │                                                             │ eRTOSOTAUpdate                                               │
└───┴─────────────────────────────────────────────────────────────┴──────────────────────────────────────────────────────────────┘
(NOTE: There may be security-related changes not in this list. See https://github.com/aws/aws-cdk/issues/1299)


"--require-approval" is enabled and stack includes security-sensitive updates: Do you wish to deploy these changes? (y/n) y
Esp32OtaStack: deploying... [1/1]

 ✅  Esp32OtaStack

✨  Deployment time: 41.65s

Outputs:
Esp32OtaStack.Backend = https
Esp32OtaStack.FirmwareBucketName = fw-esp32-ota-poc-01-214280727683-us-east-1
Esp32OtaStack.OtaServiceRoleArn = arn:aws:iam::214280727683:role/esp32-ota-poc-01-ota-role
Esp32OtaStack.PolicyName = esp32-ota-poc-01-policy
Esp32OtaStack.ThingGroupName = esp32-ota-poc-01-group
Esp32OtaStack.ThingName = esp32-ota-poc-01
Stack ARN:
arn:aws:cloudformation:us-east-1:214280727683:stack/Esp32OtaStack/2e07a560-73f9-11f1-91f4-0e6b23b2b0bb

✨  Total time: 79.35s
```

`% cd ..`
`% BACKEND=https aws-iot/scripts/register-device.sh`
```
>> creating device certificate + keys
>> certificate: arn:aws:iot:us-east-1:214280727683:cert/6eeb9c446b78339f6e993ee15549b800dab487064873e422278bec1fcffcd2ec
>> attaching policy 'esp32-ota-poc-01-policy' to the certificate
>> attaching certificate to Thing 'esp32-ota-poc-01'   (AttachThingPrincipal)
>> copied client.crt + client.key -> ../../esp/src/certs
>> set THING_NAME='esp32-ota-poc-01' in esp/src/secrets.h, then REBUILD + flash.

  CERT_ARN=arn:aws:iot:us-east-1:214280727683:cert/6eeb9c446b78339f6e993ee15549b800dab487064873e422278bec1fcffcd2ec
lookfwd@macbookpro esp-aws-greengrass-ota-poc % 
```

`% BACKEND=https aws-iot/scripts/make-codesign-cert.sh`
```
>> generating ECDSA P-256 code-signing certificate
>> importing certificate into ACM
>> ACM cert: arn:aws:acm:us-east-1:214280727683:certificate/c076e14d-03c1-412c-9d59-923ad62f9de9
>> creating Signer profile 'esp32_ota_https_profile' (AmazonFreeRTOS-Default)
>> copied public code-signing cert -> ../../esp/src/certs/aws_codesign.crt
>> REBUILD the firmware so it embeds this cert before flashing.

  SIGNING_PROFILE_NAME=esp32_ota_https_profile
  CODE_SIGNING_CERT_ARN=arn:aws:acm:us-east-1:214280727683:certificate/c076e14d-03c1-412c-9d59-923ad62f9de9
```


`% esp/scripts/build-fixture.sh https good 2.0.0`
```
>> building 'https/good' v2.0.0
>> -DAPP_VERSION_MAJOR=2 -DAPP_VERSION_MINOR=0 -DAPP_VERSION_BUILD=0 -DFW_SELFTEST_SHOULD_PASS=1
Processing https (platform: espressif32@6.9.0; framework: espidf; board: adafruit_feather_esp32s3)
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
Verbose mode can be enabled via `-v, --verbose` option
>> OTA backend = https
CONFIGURATION: https://docs.platformio.org/page/boards/espressif32/adafruit_feather_esp32s3.html
PLATFORM: Espressif 32 (6.9.0) > Adafruit Feather ESP32-S3 2MB PSRAM
HARDWARE: ESP32S3 240MHz, 320KB RAM, 4MB Flash
DEBUG: Current (cmsis-dap) External (cmsis-dap, esp-bridge, esp-builti
...
...
Environment    Status    Duration
-------------  --------  ------------
https          SUCCESS   00:01:00.511
================================================================================= 1 succeeded in 00:01:00.511 =================================================================================
>> wrote fixtures/firmware-https-good-v2.0.0.bin
```


`% BACKEND=https aws-iot/scripts/upload-firmware.sh esp/fixtures/firmware-https-good-v2.0.0.bin 2.0.0`

```
>> [https] uploading /Users/lookfwd/Desktop/quick-test/esp-aws-greengrass-ota-poc/esp/fixtures/firmware-https-good-v2.0.0.bin -> s3://fw-esp32-ota-poc-01-214280727683-us-east-1/firmware/2.0.0/firmware.bin
>> uploaded.

  next:  BACKEND=https scripts/push-update.sh esp32-ota-poc-01 2.0.0
```

`% BACKEND=https aws-iot/scripts/push-update.sh esp32-ota-poc-01 2.0.0`
```
>> [https] create-ota-update 'esp32-ota-https-2-0-0-1782767189' (--protocols HTTP) -> arn:aws:iot:us-east-1:214280727683:thing/esp32-ota-poc-01
-----------------------------------------------------------------
|                        CreateOTAUpdate                        |
+-------+------------------------------------+------------------+
| jobId |            otaUpdateId             |     status       |
+-------+------------------------------------+------------------+
|  None |  esp32-ota-https-2-0-0-1782767189  |  CREATE_PENDING  |
+-------+------------------------------------+------------------+

>> watch:  BACKEND=https scripts/watch.sh esp32-ota-poc-01   (Job id: AFR_OTA-esp32-ota-https-2-0-0-1782767189)
```


`% BACKEND=https aws-iot/scripts/watch.sh       esp32-ota-poc-01`
```
>> [https] watching job 'AFR_OTA-esp32-ota-https-2-0-0-1782767189' on 'esp32-ota-poc-01'  (Ctrl-C to stop)
17:10:15  IN_PROGRESS
17:10:21  IN_PROGRESS
17:10:27  IN_PROGRESS
17:10:32  IN_PROGRESS
```

Final ESP logs proving bad image rejection:

```


         Connected!
19:35:13.791 > I (1775) esp_netif_handlers: sta ip: 192.168.86.46, mask: 255.255.255.0, gw: 192.168.86.1
19:35:13.792 > I (1775) wifi: got IP 192.168.86.46
19:35:13.799 > I (1780) device_iot: up — backend 'https', firmware v1.0.0
19:35:13.805 > I (1783) app: running on 'https' backend, v1.0.0 (vGOOD)
19:35:14.796 > I (2780) ota: requesting job document
19:35:15.211 > I (3195) coreMQTT: MQTT connection established with the broker.
19:35:15.213 > I (3195) mqtt: MQTT connected to AWS IoT Core as 'esp32-ota-poc-01'
19:35:15.223 > I (3206) mqtt: subscribed: dt/esp32-ota-poc-01/cmd
19:35:15.224 > I (3206) ota: (re)subscribing to job topics
19:35:15.233 > I (3216) mqtt: subscribed: $aws/things/esp32-ota-poc-01/jobs/notify-next
19:35:15.241 > I (3225) mqtt: subscribed: $aws/things/esp32-ota-poc-01/jobs/start-next/accepted
19:35:15.244 > I (3225) ota: requesting job document
19:35:15.339 > I (3323) coreMQTT: Ack packet deserialized with result: MQTTSuccess.
19:35:15.342 > I (3324) coreMQTT: State record updated. New state=MQTTPublishDone.
19:35:15.353 > I (3337) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.
19:35:15.358 > I (3338) coreMQTT: State record updated. New state=MQTTPublishDone.
19:35:44.131 > I (32114) coreMQTT: Ack packet deserialized with result: MQTTSuccess.
19:35:44.134 > I (32114) coreMQTT: State record updated. New state=MQTTPublishDone.
19:36:14.031 > I (62011) coreMQTT: Ack packet deserialized with result: MQTTSuccess.
19:36:14.034 > I (62012) coreMQTT: State record updated. New state=MQTTPublishDone.
19:36:43.942 > I (91921) coreMQTT: Ack packet deserialized with result: MQTTSuccess.
19:36:43.944 > I (91921) coreMQTT: State record updated. New state=MQTTPublishDone.
19:37:13.956 > I (121934) coreMQTT: Ack packet deserialized with result: MQTTSuccess.
19:37:13.959 > I (121934) coreMQTT: State record updated. New state=MQTTPublishDone.
19:37:43.912 > I (151888) coreMQTT: Ack packet deserialized with result: MQTTSuccess.
19:37:43.916 > I (151889) coreMQTT: State record updated. New state=MQTTPublishDone.
19:37:49.428 > I (157404) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.
19:37:49.433 > I (157404) coreMQTT: State record updated. New state=MQTTPubAckSend.
19:37:49.460 > I (157435) AWS_OTA: otaPal_SetPlatformImageState, 2
19:37:49.460 > I (157435) AWS_OTA: Set image as valid one!
19:37:49.467 > I (157436) esp_ota_ops: aws_esp_ota_get_boot_flags: 1
19:37:49.472 > I (157441) esp_ota_ops: [0] aflags/seq:0x2/0x1, pflags/seq:0xffffffff/0x0
19:37:49.477 > W (157448) AWS_OTA: Image not in self test mode 2
19:37:49.483 > I (157453) esp_ota_ops: aws_esp_ota_get_boot_flags: 1
19:37:49.490 > I (157459) esp_ota_ops: [0] aflags/seq:0x2/0x1, pflags/seq:0xffffffff/0x0
19:37:49.498 > I (157467) AWS_OTA: Writing to partition subtype 17 at offset 0x1b0000
19:37:51.181 > I (159157) AWS_OTA: esp_ota_begin succeeded
19:37:51.181 > I (159157) ota: OTA job accepted (HTTP data path), 975968 bytes
19:37:51.190 > I (159158) ota: reporting job AFR_OTA-esp32-ota-https-2-0-0-1782776263 -> IN_PROGRESS
19:37:51.350 > I (159326) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.
19:37:51.355 > I (159327) coreMQTT: State record updated. New state=MQTTPublishDone.
19:37:51.685 > I (159660) ota: downloading ld bytes over HTTPS
19:37:52.265 > I (160240) ota: downloaded 98304 / 975968 bytes (10%)
19:37:52.886 > I (160861) ota: downloaded 196608 / 975968 bytes (20%)
19:37:53.421 > I (161396) ota: downloaded 294912 / 975968 bytes (30%)
19:37:54.065 > I (162039) ota: downloaded 393216 / 975968 bytes (40%)
19:37:54.649 > I (162624) ota: downloaded 491520 / 975968 bytes (50%)
19:37:55.141 > I (163117) ota: downloaded 585728 / 975968 bytes (60%)
19:37:55.730 > I (163705) ota: downloaded 684032 / 975968 bytes (70%)
19:37:56.240 > I (164215) ota: downloaded 782336 / 975968 bytes (80%)
19:37:56.802 > I (164778) ota: downloaded 880640 / 975968 bytes (90%)
19:37:57.390 > I (165365) ota: downloaded 975968 / 975968 bytes (100%)
19:37:57.394 > I (165369) ota: download complete (975968 bytes)
19:37:57.395 > I (165369) ota: verifying signature + closing file
19:37:57.701 > I (165674) AWS_OTA: Signature verification succeeded.
19:37:57.701 > I (165675) ota: signature OK -> activating + rebooting
19:37:57.712 > I (165678) esp_image: segment 0: paddr=001b0020 vaddr=3c0b0020 size=327e8h (206824) map
19:37:57.738 > I (165713) esp_image: segment 1: paddr=001e2810 vaddr=3fc99700 size=04edch ( 20188) 
19:37:57.743 > I (165717) esp_image: segment 2: paddr=001e76f4 vaddr=40374000 size=08924h ( 35108) 
19:37:57.752 > I (165724) esp_image: segment 3: paddr=001f0020 vaddr=42000020 size=a16e0h (661216) map
19:37:57.842 > I (165817) esp_image: segment 4: paddr=00291708 vaddr=4037c924 size=0cd28h ( 52520) 
19:37:57.852 > I (165826) esp_image: segment 0: paddr=001b0020 vaddr=3c0b0020 size=327e8h (206824) map
19:37:57.879 > I (165855) esp_image: segment 1: paddr=001e2810 vaddr=3fc99700 size=04edch ( 20188) 
19:37:57.885 > I (165858) esp_image: segment 2: paddr=001e76f4 vaddr=40374000 size=08924h ( 35108) 
19:37:57.893 > I (165865) esp_image: segment 3: paddr=001f0020 vaddr=42000020 size=a16e0h (661216) map
19:37:57.983 > I (165958) esp_image: segment 4: paddr=00291708 vaddr=4037c924 size=0cd28h ( 52520) 
19:37:58.496 > I (166471) wifi:state: run -> init (0x0)
19:37:58.498 > I (166474) wifi:pm stop, total sleep time: lu us / lu us
19:37:58.498 > 
19:37:58.499 > I (166474) wifi:<ba-del>idx:1, tid:0
19:37:58.500 > I (166475) wifi:<ba-del>idx:0, tid:6
19:37:58.508 > I (166476) wifi:new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
19:37:58.517 > E (166492) esp-tls-mbedtls: read error :-0x000zX:
19:37:58.518 > E (166492) network_transport: Error reading: -76
19:37:58.526 > E (166494) coreMQTT: Call to receiveSingleIteration failed. Status=MQTTRecvFailed
19:37:58.537 > E (166502) mqtt: process loop error: MQTTRecvFailed -> reconnecting
19:37:58.537 > W (166511) app: offline
19:37:58.539 > I (166515) wifi:flush txq
19:37:58.541 > I (166515) wifi:stop sw txq
19:37:58.544 > I (166517) wifi:lmac stop hw txq
19:37:58.557 > ESP-ROM:esp32s3-20210327
19:37:58.557 > Build:Mar 27 2021
19:37:58.558 > rst:0xc (RTC_SW_CPU_RST),boot:0x28 (SPI_FAST_FLASH_BOOT)
19:37:58.662 > Saved PC:0x4037acca
19:37:58.662 >   #0  0x4037acca in esp_cpu_wait_for_intr at /Users/lookfwd/.platformio/packages/framework-espidf@3.50301.0/components/esp_hw_support/cpu.c:64
19:37:58.662 > 
19:37:58.671 > SPIWP:0xee
19:37:58.671 > mode:DIO, clock div:1
19:37:58.671 > load:0x3fce2810,len:0x178c
19:37:58.671 > load:0x403c8700,len:0x4
19:37:58.671 > load:0x403c8704,len:0xcb8
19:37:58.671 > load:0x403cb700,len:0x2df4
19:37:58.671 > entry 0x403c890c
19:37:58.671 > I (26) boot: ESP-IDF 5.3.1 2nd stage bootloader
19:37:58.671 > I (26) boot: compile time Jun 29 2026 19:34:44
19:37:58.671 > I (26) boot: Multicore bootloader
19:37:58.671 > I (29) boot: chip revision: v0.2
19:37:58.671 > I (33) boot.esp32s3: Boot SPI Speed : 80MHz
19:37:58.671 > I (38) boot.esp32s3: SPI Mode       : DIO
19:37:58.671 > I (43) boot.esp32s3: SPI Flash Size : 4MB
19:37:58.671 > I (47) boot: Enabling RNG early entropy source...
19:37:58.671 > I (53) boot: Partition Table:
19:37:58.671 > I (56) boot: ## Label            Usage          Type ST Offset   Length
19:37:58.671 > I (64) boot:  0 nvs              WiFi data        01 02 00009000 00006000
19:37:58.671 > I (71) boot:  1 otadata          OTA data         01 00 0000f000 00002000
19:37:58.671 > I (78) boot:  2 phy_init         RF data          01 01 00011000 00001000
19:37:58.671 > I (86) boot:  3 ota_0            OTA app          00 10 00020000 00190000
19:37:58.672 > I (93) boot:  4 ota_1            OTA app          00 11 001b0000 00190000
19:37:58.672 > I (101) boot:  5 storage          WiFi data        01 02 00340000 00010000
19:37:58.672 > I (108) boot:  6 coredump         Unknown data     01 03 00350000 00010000
19:37:58.676 > I (116) boot: End of partition table
19:37:58.693 > I (137) esp_image: segment 0: paddr=001b0020 vaddr=3c0b0020 size=327e8h (206824) map
19:37:58.731 > I (175) esp_image: segment 1: paddr=001e2810 vaddr=3fc99700 size=04edch ( 20188) load
19:37:58.735 > I (180) esp_image: segment 2: paddr=001e76f4 vaddr=40374000 size=08924h ( 35108) load
19:37:58.744 > I (188) esp_image: segment 3: paddr=001f0020 vaddr=42000020 size=a16e0h (661216) map
19:37:58.865 > I (308) esp_image: segment 4: paddr=00291708 vaddr=4037c924 size=0cd28h ( 52520) load
19:37:58.885 > I (329) boot: Loaded app from partition at offset 0x1b0000
19:37:58.885 > I (329) boot: Disabling RNG early entropy source...
19:37:58.898 > I (341) cpu_start: Multicore app
19:37:58.907 > I (351) cpu_start: Pro cpu start user code
19:37:58.907 > I (351) cpu_start: cpu freq: 160000000 Hz
19:37:58.909 > I (351) app_init: Application information:
19:37:58.914 > I (354) app_init: Project name:     esp_ota
19:37:58.919 > I (358) app_init: App version:      1.0.0
19:37:58.924 > I (363) app_init: Compile time:     Jun 29 2026 19:36:50
19:37:58.930 > I (369) app_init: ELF file SHA256:  06d22f318...
19:37:58.935 > I (374) app_init: ESP-IDF:          5.3.1
19:37:58.940 > I (379) efuse_init: Min chip rev:     v0.0
19:37:58.945 > I (384) efuse_init: Max chip rev:     v0.99 
19:37:58.949 > I (389) efuse_init: Chip rev:         v0.2
19:37:58.955 > I (394) heap_init: Initializing. RAM available for dynamic allocation:
19:37:58.962 > I (401) heap_init: At 3FCA7880 len 00041E90 (263 KiB): RAM
19:37:58.968 > I (407) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM
19:37:58.974 > I (413) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
19:37:58.981 > I (419) heap_init: At 600FE100 len 00001EE8 (7 KiB): RTCRAM
19:37:58.986 > I (427) spi_flash: detected chip: generic
19:37:58.990 > I (430) spi_flash: flash io: dio
19:37:58.995 > I (435) sleep: Configure to isolate all GPIO pins in sleep state
19:37:59.002 > I (441) sleep: Enable automatic switching of GPIO sleep configuration
19:37:59.009 > I (448) esp_core_dump_flash: Init core dump to flash
19:37:59.015 > I (454) esp_core_dump_flash: Found partition 'coredump' @ 350000 65536 bytes
19:37:59.022 > D (461) esp_core_dump_flash: Blank core dump partition!
19:37:59.027 > I (467) main_task: Started on CPU0
19:37:59.031 > I (471) main_task: Calling app_main()
19:37:59.050 > I (493) self_test: ----------------------------------------------------------
19:37:59.056 > I (493) self_test:  firmware v2.0.0   variant=vGOOD
19:37:59.059 > I (495) self_test:  running partition: ota_1 @ 0x1b0000  (ota state 1)
19:37:59.066 > I (503) self_test:  reset reason: 3   free heap: 301568
19:37:59.072 > I (509) self_test: ----------------------------------------------------------
19:37:59.081 > W (517) self_test: self-test watchdog armed (180000 ms)
19:37:59.082 > I (524) pp: pp rom version: e7ae62f
19:37:59.088 > I (527) net80211: net80211 rom version: e7ae62f
19:37:59.095 > I (533) wifi:wifi driver task: 3fcb24d8, prio:23, stack:6144, core=0
19:37:59.105 > I (549) wifi:wifi firmware version: ccaebfa
19:37:59.105 > I (549) wifi:wifi certification version: v7.0
19:37:59.105 > I (549) wifi:config NVS flash: enabled
19:37:59.109 > I (549) wifi:config nano formating: enabled
19:37:59.114 > I (553) wifi:Init data frame dynamic rx buffer num: 32
19:37:59.118 > I (558) wifi:Init static rx mgmt buffer num: 5
19:37:59.122 > I (562) wifi:Init management short buffer num: 32
19:37:59.127 > I (567) wifi:Init dynamic tx buffer num: 32
19:37:59.130 > I (571) wifi:Init static tx FG buffer num: 2
19:37:59.134 > I (575) wifi:Init static rx buffer size: 1600
19:37:59.138 > I (579) wifi:Init static rx buffer num: 10
19:37:59.142 > I (582) wifi:Init dynamic rx buffer num: 32
19:37:59.146 > I (587) wifi_init: rx ba win: 6
19:37:59.150 > I (590) wifi_init: accept mbox: 6
19:37:59.154 > I (594) wifi_init: tcpip mbox: 32
19:37:59.158 > I (598) wifi_init: udp mbox: 6
19:37:59.161 > I (602) wifi_init: tcp mbox: 6
19:37:59.166 > I (606) wifi_init: tcp tx win: 5760
19:37:59.170 > I (610) wifi_init: tcp rx win: 5760
19:37:59.174 > I (614) wifi_init: tcp mss: 1440
19:37:59.178 > I (618) wifi_init: WiFi IRAM OP enabled
19:37:59.183 > I (622) wifi_init: WiFi RX IRAM OP enabled
19:37:59.190 > I (629) phy_init: phy_version 680,a6008b2,Jun  4 2024,16:41:10
19:37:59.231 > W (674) phy_init: saving new calibration data because of checksum failure, mode(0)
19:37:59.245 > I (688) wifi:mode : sta (a0:f2:62:f4:5e:a0)
19:37:59.245 > I (689) wifi:enable tsf
19:37:59.247 > I (690) wifi: connecting to SSID 'bananas'...
19:37:59.255 > I (699) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
19:37:59.256 > I (700) wifi:state: init -> auth (0xb0)
19:37:59.265 > I (708) wifi:state: auth -> assoc (0x0)
19:37:59.272 > I (715) wifi:state: assoc -> run (0x10)
19:37:59.286 > I (729) wifi:connected with bananas, aid = 8, channel 6, BW20, bssid = b0:e4:d5:6a:f3:8d
19:37:59.287 > I (730) wifi:security: WPA2-PSK, phy: bgn, rssi: -52
19:37:59.290 > I (734) wifi:pm start, type: 1
19:37:59.290 > 
19:37:59.298 > I (734) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
19:37:59.306 > I (742) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
19:37:59.316 > I (759) wifi:<ba-add>idx:0 (ifx:0, b0:e4:d5:6a:f3:8d), tid:6, ssn:2, winSize:64
19:37:59.354 > I (797) wifi:dp: 2, bi: 102400, li: 4, scale listen interval from 307200 us to 409600 us
19:37:59.356 > I (798) wifi:AP's beacon interval = 102400 us, DTIM period = 2
19:38:00.309 > I (1753) esp_netif_handlers: sta ip: 192.168.86.46, mask: 255.255.255.0, gw: 192.168.86.1
19:38:00.311 > I (1753) wifi: got IP 192.168.86.46
19:38:00.318 > I (1761) wifi:<ba-add>idx:1 (ifx:0, b0:e4:d5:6a:f3:8d), tid:0, ssn:0, winSize:64
19:38:01.716 > I (3159) coreMQTT: MQTT connection established with the broker.
19:38:01.718 > I (3159) mqtt: MQTT connected to AWS IoT Core as 'esp32-ota-poc-01'
19:38:01.861 > I (3304) coreMQTT: Ack packet deserialized with result: MQTTSuccess.
19:38:01.863 > I (3304) coreMQTT: State record updated. New state=MQTTPublishDone.
19:38:01.915 > W (3358) ota: trial boot detected — running self-test
19:38:01.916 > I (3359) self_test: core-function check: free heap = 207188
19:38:01.921 > I (3360) self_test: app health check -> PASS
19:38:01.938 > I (3382) self_test: image COMMITTED (rollback cancelled)
19:38:01.939 > I (3382) self_test: self-test watchdog disarmed
19:38:01.947 > I (3383) ota: reporting job AFR_OTA-esp32-ota-https-2-0-0-1782776263 -> SUCCEEDED
19:38:02.126 > I (3569) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.
19:38:02.130 > I (3569) coreMQTT: State record updated. New state=MQTTPublishDone.
19:38:07.027 > I (8470) mqtt: subscribed: $aws/things/esp32-ota-poc-01/jobs/notify-next
19:38:07.040 > I (8478) mqtt: subscribed: $aws/things/esp32-ota-poc-01/jobs/start-next/accepted
19:38:07.040 > I (8478) device_iot: up — backend 'https', firmware v2.0.0
19:38:07.141 > I (8582) mqtt: subscribed: dt/esp32-ota-poc-01/cmd
19:38:07.141 > I (8584) app: running on 'https' backend, v2.0.0 (vGOOD)
19:38:07.339 > I (8782) coreMQTT: Ack packet deserialized with result: MQTTSuccess.
19:38:07.341 > I (8782) coreMQTT: State record updated. New state=MQTTPublishDone.
19:38:08.035 > I (9478) ota: requesting job document
19:38:08.311 > I (9753) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.
19:38:08.315 > I (9754) coreMQTT: State record updated. New state=MQTTPublishDone.
19:38:21.379 > I (22821) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.
19:38:21.383 > I (22822) coreMQTT: State record updated. New state=MQTTPubAckSend.
19:38:21.410 > I (22853) AWS_OTA: otaPal_SetPlatformImageState, 2
19:38:21.411 > I (22853) AWS_OTA: Set image as valid one!
19:38:21.415 > I (22853) esp_ota_ops: aws_esp_ota_get_boot_flags: 1
19:38:21.422 > I (22858) esp_ota_ops: [1] aflags/seq:0x2/0x2, pflags/seq:0x2/0x1
19:38:21.427 > W (22865) AWS_OTA: Image not in self test mode 2
19:38:21.433 > I (22870) esp_ota_ops: aws_esp_ota_get_boot_flags: 1
19:38:21.440 > I (22876) esp_ota_ops: [1] aflags/seq:0x2/0x2, pflags/seq:0x2/0x1
19:38:21.446 > I (22883) AWS_OTA: Writing to partition subtype 16 at offset 0x20000
19:38:23.151 > I (24593) AWS_OTA: esp_ota_begin succeeded
19:38:23.151 > I (24593) ota: OTA job accepted (HTTP data path), 975984 bytes
19:38:23.160 > I (24594) ota: reporting job AFR_OTA-esp32-ota-https-2-0-0-bad-1782776294 -> IN_PROGRESS
19:38:23.211 > I (24653) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.
19:38:23.215 > I (24653) coreMQTT: State record updated. New state=MQTTPublishDone.
19:38:23.616 > I (25057) ota: downloading ld bytes over HTTPS
19:38:24.240 > I (25682) ota: downloaded 98304 / 975984 bytes (10%)
19:38:24.815 > I (26254) ota: downloaded 196608 / 975984 bytes (20%)
19:38:25.321 > I (26763) ota: downloaded 294912 / 975984 bytes (30%)
19:38:25.909 > I (27351) ota: downloaded 393216 / 975984 bytes (40%)
19:38:26.477 > I (27918) ota: downloaded 491520 / 975984 bytes (50%)
19:38:27.060 > I (28502) ota: downloaded 585728 / 975984 bytes (60%)
19:38:27.609 > I (29051) ota: downloaded 684032 / 975984 bytes (70%)
19:38:28.094 > I (29536) ota: downloaded 782336 / 975984 bytes (80%)
19:38:28.650 > I (30092) ota: downloaded 880640 / 975984 bytes (90%)
19:38:29.154 > I (30595) ota: downloaded 975984 / 975984 bytes (100%)
19:38:29.156 > I (30598) ota: download complete (975984 bytes)
19:38:29.158 > I (30599) ota: verifying signature + closing file
19:38:29.461 > I (30903) AWS_OTA: Signature verification succeeded.
19:38:29.462 > I (30904) ota: signature OK -> activating + rebooting
19:38:29.472 > I (30906) esp_image: segment 0: paddr=00020020 vaddr=3c0b0020 size=32804h (206852) map
19:38:29.499 > I (30941) esp_image: segment 1: paddr=0005282c vaddr=3fc99700 size=04edch ( 20188) 
19:38:29.506 > I (30945) esp_image: segment 2: paddr=00057710 vaddr=40374000 size=08908h ( 35080) 
19:38:29.513 > I (30952) esp_image: segment 3: paddr=00060020 vaddr=42000020 size=a16e0h (661216) map
19:38:29.604 > I (31046) esp_image: segment 4: paddr=00101708 vaddr=4037c908 size=0cd44h ( 52548) 
19:38:29.616 > I (31054) esp_image: segment 0: paddr=00020020 vaddr=3c0b0020 size=32804h (206852) map
19:38:29.641 > I (31082) esp_image: segment 1: paddr=0005282c vaddr=3fc99700 size=04edch ( 20188) 
19:38:29.646 > I (31086) esp_image: segment 2: paddr=00057710 vaddr=40374000 size=08908h ( 35080) 
19:38:29.654 > I (31093) esp_image: segment 3: paddr=00060020 vaddr=42000020 size=a16e0h (661216) map
19:38:29.744 > I (31186) esp_image: segment 4: paddr=00101708 vaddr=4037c908 size=0cd44h ( 52548) 
19:38:30.269 > I (31711) wifi:state: run -> init (0x0)
19:38:30.272 > I (31714) wifi:pm stop, total sleep time: lu us / lu us
19:38:30.272 > 
19:38:30.272 > I (31714) wifi:<ba-del>idx:1, tid:0
19:38:30.273 > I (31715) wifi:<ba-del>idx:0, tid:6
19:38:30.280 > I (31716) wifi:new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
19:38:30.289 > E (31726) network_transport: Error reading the message
19:38:30.298 > E (31729) coreMQTT: Call to receiveSingleIteration failed. Status=MQTTRecvFailed
19:38:30.301 > E (31737) mqtt: process loop error: MQTTRecvFailed -> reconnecting
19:38:30.304 > W (31745) app: offline
19:38:30.311 > E (31748) esp-tls: [sock=54] connect() error: Host is unreachable
19:38:30.315 > I (31754) wifi:flush txq
19:38:30.320 > E (31754) esp-tls: Failed to open new connection
19:38:30.321 > I (31756) wifi:stop sw txq
19:38:30.330 > E (31761) mqtt: TLS connect to a2fm5sbtk65rj6-ats.iot.us-east-1.amazonaws.com:8883 failed
19:38:30.333 > I (31763) wifi:lmac stop hw txq
19:38:30.345 > ESP-ROM:esp32s3-20210327
19:38:30.345 > Build:Mar 27 2021
19:38:30.346 > rst:0xc (RTC_SW_CPU_RST),boot:0x28 (SPI_FAST_FLASH_BOOT)
19:38:30.347 > Saved PC:0x4037acca
19:38:30.420 >   #0  0x4037acca in esp_cpu_wait_for_intr at /Users/lookfwd/.platformio/packages/framework-espidf@3.50301.0/components/esp_hw_support/cpu.c:64
19:38:30.420 > 
19:38:30.431 > SPIWP:0xee
19:38:30.431 > mode:DIO, clock div:1
19:38:30.431 > load:0x3fce2810,len:0x178c
19:38:30.431 > load:0x403c8700,len:0x4
19:38:30.431 > load:0x403c8704,len:0xcb8
19:38:30.431 > load:0x403cb700,len:0x2df4
19:38:30.431 > entry 0x403c890c
19:38:30.431 > I (26) boot: ESP-IDF 5.3.1 2nd stage bootloader
19:38:30.431 > I (26) boot: compile time Jun 29 2026 19:34:44
19:38:30.431 > I (26) boot: Multicore bootloader
19:38:30.431 > I (29) boot: chip revision: v0.2
19:38:30.431 > I (33) boot.esp32s3: Boot SPI Speed : 80MHz
19:38:30.431 > I (38) boot.esp32s3: SPI Mode       : DIO
19:38:30.431 > I (43) boot.esp32s3: SPI Flash Size : 4MB
19:38:30.431 > I (47) boot: Enabling RNG early entropy source...
19:38:30.431 > I (53) boot: Partition Table:
19:38:30.431 > I (56) boot: ## Label            Usage          Type ST Offset   Length
19:38:30.431 > I (64) boot:  0 nvs              WiFi data        01 02 00009000 00006000
19:38:30.431 > I (71) boot:  1 otadata          OTA data         01 00 0000f000 00002000
19:38:30.431 > I (78) boot:  2 phy_init         RF data          01 01 00011000 00001000
19:38:30.436 > I (86) boot:  3 ota_0            OTA app          00 10 00020000 00190000
19:38:30.443 > I (93) boot:  4 ota_1            OTA app          00 11 001b0000 00190000
19:38:30.451 > I (101) boot:  5 storage          WiFi data        01 02 00340000 00010000
19:38:30.458 > I (108) boot:  6 coredump         Unknown data     01 03 00350000 00010000
19:38:30.464 > I (116) boot: End of partition table
19:38:30.480 > I (135) esp_image: segment 0: paddr=00020020 vaddr=3c0b0020 size=32804h (206852) map
19:38:30.518 > I (173) esp_image: segment 1: paddr=0005282c vaddr=3fc99700 size=04edch ( 20188) load
19:38:30.522 > I (178) esp_image: segment 2: paddr=00057710 vaddr=40374000 size=08908h ( 35080) load
19:38:30.531 > I (187) esp_image: segment 3: paddr=00060020 vaddr=42000020 size=a16e0h (661216) map
19:38:30.651 > I (307) esp_image: segment 4: paddr=00101708 vaddr=4037c908 size=0cd44h ( 52548) load
19:38:30.672 > I (328) boot: Loaded app from partition at offset 0x20000
19:38:30.672 > I (328) boot: Disabling RNG early entropy source...
19:38:30.684 > I (340) cpu_start: Multicore app
19:38:30.693 > I (349) cpu_start: Pro cpu start user code
19:38:30.693 > I (349) cpu_start: cpu freq: 160000000 Hz
19:38:30.698 > I (349) app_init: Application information:
19:38:30.701 > I (352) app_init: Project name:     esp_ota
19:38:30.706 > I (357) app_init: App version:      1.0.0
19:38:30.711 > I (361) app_init: Compile time:     Jun 29 2026 19:35:30
19:38:30.717 > I (367) app_init: ELF file SHA256:  b5db7e387...
19:38:30.722 > I (373) app_init: ESP-IDF:          5.3.1
19:38:30.727 > I (377) efuse_init: Min chip rev:     v0.0
19:38:30.731 > I (382) efuse_init: Max chip rev:     v0.99 
19:38:30.736 > I (387) efuse_init: Chip rev:         v0.2
19:38:30.742 > I (392) heap_init: Initializing. RAM available for dynamic allocation:
19:38:30.749 > I (399) heap_init: At 3FCA7880 len 00041E90 (263 KiB): RAM
19:38:30.755 > I (405) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM
19:38:30.762 > I (411) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
19:38:30.767 > I (418) heap_init: At 600FE100 len 00001EE8 (7 KiB): RTCRAM
19:38:30.773 > I (425) spi_flash: detected chip: generic
19:38:30.777 > I (428) spi_flash: flash io: dio
19:38:30.782 > I (433) sleep: Configure to isolate all GPIO pins in sleep state
19:38:30.789 > I (439) sleep: Enable automatic switching of GPIO sleep configuration
19:38:30.796 > I (446) esp_core_dump_flash: Init core dump to flash
19:38:30.802 > I (452) esp_core_dump_flash: Found partition 'coredump' @ 350000 65536 bytes
19:38:30.809 > D (460) esp_core_dump_flash: Blank core dump partition!
19:38:30.814 > I (465) main_task: Started on CPU0
19:38:30.818 > I (469) main_task: Calling app_main()
19:38:30.836 > I (491) self_test: ----------------------------------------------------------
19:38:30.838 > I (491) self_test:  firmware v2.0.0   variant=vBAD
19:38:30.845 > I (493) self_test:  running partition: ota_0 @ 0x020000  (ota state 1)
19:38:30.852 > I (500) self_test:  reset reason: 3   free heap: 301700
19:38:30.859 > I (506) self_test: ----------------------------------------------------------
19:38:30.868 > W (515) self_test: self-test watchdog armed (180000 ms)
19:38:30.869 > I (522) pp: pp rom version: e7ae62f
19:38:30.874 > I (524) net80211: net80211 rom version: e7ae62f
19:38:30.880 > I (531) wifi:wifi driver task: 3fcb2454, prio:23, stack:6144, core=0
19:38:30.891 > I (546) wifi:wifi firmware version: ccaebfa
19:38:30.892 > I (547) wifi:wifi certification version: v7.0
19:38:30.892 > I (547) wifi:config NVS flash: enabled
19:38:30.896 > I (547) wifi:config nano formating: enabled
19:38:30.900 > I (551) wifi:Init data frame dynamic rx buffer num: 32
19:38:30.906 > I (556) wifi:Init static rx mgmt buffer num: 5
19:38:30.909 > I (560) wifi:Init management short buffer num: 32
19:38:30.913 > I (564) wifi:Init dynamic tx buffer num: 32
19:38:30.917 > I (568) wifi:Init static tx FG buffer num: 2
19:38:30.921 > I (572) wifi:Init static rx buffer size: 1600
19:38:30.925 > I (576) wifi:Init static rx buffer num: 10
19:38:30.929 > I (580) wifi:Init dynamic rx buffer num: 32
19:38:30.932 > I (585) wifi_init: rx ba win: 6
19:38:30.936 > I (588) wifi_init: accept mbox: 6
19:38:30.940 > I (592) wifi_init: tcpip mbox: 32
19:38:30.944 > I (596) wifi_init: udp mbox: 6
19:38:30.948 > I (599) wifi_init: tcp mbox: 6
19:38:30.952 > I (603) wifi_init: tcp tx win: 5760
19:38:30.956 > I (607) wifi_init: tcp rx win: 5760
19:38:30.960 > I (611) wifi_init: tcp mss: 1440
19:38:30.965 > I (615) wifi_init: WiFi IRAM OP enabled
19:38:30.973 > I (620) wifi_init: WiFi RX IRAM OP enabled
19:38:30.976 > I (627) phy_init: phy_version 680,a6008b2,Jun  4 2024,16:41:10
19:38:31.015 > W (670) phy_init: saving new calibration data because of checksum failure, mode(0)
19:38:31.048 > I (702) wifi:mode : sta (a0:f2:62:f4:5e:a0)
19:38:31.048 > I (703) wifi:enable tsf
19:38:31.050 > I (704) wifi: connecting to SSID 'bananas'...
19:38:31.089 > I (744) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
19:38:31.090 > I (745) wifi:state: init -> auth (0xb0)
19:38:31.103 > I (758) wifi:state: auth -> assoc (0x0)
19:38:31.112 > I (767) wifi:state: assoc -> run (0x10)
19:38:31.142 > I (796) wifi:connected with bananas, aid = 1, channel 6, BW20, bssid = 24:29:34:c1:29:e5
19:38:31.143 > I (797) wifi:security: WPA2-PSK, phy: bgn, rssi: -83
19:38:31.145 > I (800) wifi:pm start, type: 1
19:38:31.146 > 
19:38:31.154 > I (801) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
19:38:31.162 > I (809) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
19:38:31.170 > I (818) wifi:<ba-add>idx:0 (ifx:0, 24:29:34:c1:29:e5), tid:0, ssn:0, winSize:64
19:38:31.191 > I (846) wifi:<ba-add>idx:1 (ifx:0, 24:29:34:c1:29:e5), tid:6, ssn:2, winSize:64
19:38:31.291 > I (946) wifi:dp: 2, bi: 102400, li: 4, scale listen interval from 307200 us to 409600 us
19:38:31.293 > I (947) wifi:AP's beacon interval = 102400 us, DTIM period = 2
19:38:32.673 > I (2328) esp_netif_handlers: sta ip: 192.168.86.46, mask: 255.255.255.0, gw: 192.168.86.1
19:38:32.675 > I (2328) wifi: got IP 192.168.86.46
19:38:34.061 > I (3716) coreMQTT: MQTT connection established with the broker.
19:38:34.063 > I (3716) mqtt: MQTT connected to AWS IoT Core as 'esp32-ota-poc-01'
19:38:34.078 > W (3733) ota: trial boot detected — running self-test
19:38:34.079 > I (3733) self_test: core-function check: free heap = 206956
19:38:34.087 > I (3734) self_test: app health check -> FAIL
19:38:34.090 > E (3739) ota: self-test failed (cloud=1 core=0) -> rollback
19:38:34.099 > I (3746) ota: reporting job AFR_OTA-esp32-ota-https-2-0-0-bad-1782776294 -> FAILED
19:38:34.232 > I (3886) coreMQTT: Ack packet deserialized with result: MQTTSuccess.
19:38:34.235 > I (3887) coreMQTT: State record updated. New state=MQTTPublishDone.
19:38:34.247 > I (3901) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.
19:38:34.252 > I (3902) coreMQTT: State record updated. New state=MQTTPublishDone.
19:38:35.600 > E (5255) self_test: image REJECTED -> rolling back to previous slot
19:38:35.755 > I (5409) esp_ota_ops: Rollback to previously worked partition. Restart.
19:38:35.756 > I (5410) wifi:state: run -> init (0x0)
19:38:35.759 > I (5413) wifi:pm stop, total sleep time: lu us / lu us
19:38:35.760 > 
19:38:35.766 > I (5414) wifi:<ba-del>idx:0, tid:0
19:38:35.767 > I (5417) wifi:<ba-del>idx:1, tid:6
19:38:35.773 > I (5421) wifi:new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
19:38:35.779 > E (5431) network_transport: Error reading the message
19:38:35.787 > E (5434) coreMQTT: Call to receiveSingleIteration failed. Status=MQTTRecvFailed
19:38:35.794 > E (5442) mqtt: process loop error: MQTTRecvFailed -> reconnecting
19:38:35.796 > W (5450) app: offline
19:38:35.806 > E (5453) esp-tls: [sock=54] connect() error: Host is unreachable
19:38:35.808 > E (5459) esp-tls: Failed to open new connection
19:38:35.817 > E (5463) mqtt: TLS connect to a2fm5sbtk65rj6-ats.iot.us-east-1.amazonaws.com:8883 failed
19:38:35.823 > I (5472) wifi:flush txq
19:38:35.824 > I (5474) wifi:stop sw txq
19:38:35.824 > I (5476) wifi:lmac stop hw txq
19:38:35.837 > ESP-ROM:esp32s3-20210327
19:38:35.837 > Build:Mar 27 2021
19:38:35.838 > rst:0xc (RTC_SW_CPU_RST),boot:0x28 (SPI_FAST_FLASH_BOOT)
19:38:35.838 > Saved PC:0x403769bc
19:38:35.883 >   #0  0x403769bc in esp_restart_noos at /Users/lookfwd/.platformio/packages/framework-espidf@3.50301.0/components/esp_system/port/soc/esp32s3/system_internal.c:158
19:38:35.883 > 
19:38:35.894 > SPIWP:0xee
19:38:35.894 > mode:DIO, clock div:1
19:38:35.894 > load:0x3fce2810,len:0x178c
19:38:35.894 > load:0x403c8700,len:0x4
19:38:35.894 > load:0x403c8704,len:0xcb8
19:38:35.894 > load:0x403cb700,len:0x2df4
19:38:35.894 > entry 0x403c890c
19:38:35.894 > I (26) boot: ESP-IDF 5.3.1 2nd stage bootloader
19:38:35.894 > I (26) boot: compile time Jun 29 2026 19:34:44
19:38:35.894 > I (26) boot: Multicore bootloader
19:38:35.894 > I (29) boot: chip revision: v0.2
19:38:35.894 > I (33) boot.esp32s3: Boot SPI Speed : 80MHz
19:38:35.894 > I (38) boot.esp32s3: SPI Mode       : DIO
19:38:35.894 > I (43) boot.esp32s3: SPI Flash Size : 4MB
19:38:35.894 > I (47) boot: Enabling RNG early entropy source...
19:38:35.894 > I (53) boot: Partition Table:
19:38:35.898 > I (56) boot: ## Label            Usage          Type ST Offset   Length
19:38:35.905 > I (64) boot:  0 nvs              WiFi data        01 02 00009000 00006000
19:38:35.912 > I (71) boot:  1 otadata          OTA data         01 00 0000f000 00002000
19:38:35.920 > I (78) boot:  2 phy_init         RF data          01 01 00011000 00001000
19:38:35.927 > I (86) boot:  3 ota_0            OTA app          00 10 00020000 00190000
19:38:35.935 > I (93) boot:  4 ota_1            OTA app          00 11 001b0000 00190000
19:38:35.942 > I (101) boot:  5 storage          WiFi data        01 02 00340000 00010000
19:38:35.950 > I (108) boot:  6 coredump         Unknown data     01 03 00350000 00010000
19:38:35.956 > I (116) boot: End of partition table
19:38:35.962 > I (120) esp_image: segment 0: paddr=001b0020 vaddr=3c0b0020 size=327e8h (206824) map
19:38:36.002 > I (166) esp_image: segment 1: paddr=001e2810 vaddr=3fc99700 size=04edch ( 20188) load
19:38:36.007 > I (170) esp_image: segment 2: paddr=001e76f4 vaddr=40374000 size=08924h ( 35108) load
19:38:36.016 > I (179) esp_image: segment 3: paddr=001f0020 vaddr=42000020 size=a16e0h (661216) map
19:38:36.135 > I (299) esp_image: segment 4: paddr=00291708 vaddr=4037c924 size=0cd28h ( 52520) load
19:38:36.156 > I (319) boot: Loaded app from partition at offset 0x1b0000
19:38:36.156 > I (320) boot: Disabling RNG early entropy source...
19:38:36.168 > I (332) cpu_start: Multicore app
19:38:36.177 > I (341) cpu_start: Pro cpu start user code
19:38:36.177 > I (341) cpu_start: cpu freq: 160000000 Hz
19:38:36.180 > I (341) app_init: Application information:
19:38:36.185 > I (344) app_init: Project name:     esp_ota
19:38:36.191 > I (349) app_init: App version:      1.0.0
19:38:36.195 > I (353) app_init: Compile time:     Jun 29 2026 19:36:50
19:38:36.201 > I (359) app_init: ELF file SHA256:  06d22f318...
19:38:36.207 > I (365) app_init: ESP-IDF:          5.3.1
19:38:36.210 > I (369) efuse_init: Min chip rev:     v0.0
19:38:36.215 > I (374) efuse_init: Max chip rev:     v0.99 
19:38:36.220 > I (379) efuse_init: Chip rev:         v0.2
19:38:36.225 > I (384) heap_init: Initializing. RAM available for dynamic allocation:
19:38:36.233 > I (391) heap_init: At 3FCA7880 len 00041E90 (263 KiB): RAM
19:38:36.239 > I (397) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM
19:38:36.245 > I (403) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
19:38:36.253 > I (410) heap_init: At 600FE100 len 00001EE8 (7 KiB): RTCRAM
19:38:36.257 > I (417) spi_flash: detected chip: generic
19:38:36.260 > I (420) spi_flash: flash io: dio
19:38:36.266 > I (425) sleep: Configure to isolate all GPIO pins in sleep state
19:38:36.273 > I (431) sleep: Enable automatic switching of GPIO sleep configuration
19:38:36.280 > I (438) esp_core_dump_flash: Init core dump to flash
19:38:36.285 > I (444) esp_core_dump_flash: Found partition 'coredump' @ 350000 65536 bytes
19:38:36.294 > D (452) esp_core_dump_flash: Blank core dump partition!
19:38:36.302 > I (457) main_task: Started on CPU0
19:38:36.302 > I (461) main_task: Calling app_main()
19:38:36.319 > I (482) self_test: ----------------------------------------------------------
19:38:36.321 > I (483) self_test:  firmware v2.0.0   variant=vGOOD
19:38:36.329 > I (485) self_test:  running partition: ota_1 @ 0x1b0000  (ota state 2)
19:38:36.335 > I (492) self_test:  reset reason: 3   free heap: 301568
19:38:36.342 > I (498) self_test: ----------------------------------------------------------
19:38:36.346 > I (508) pp: pp rom version: e7ae62f
19:38:36.352 > I (510) net80211: net80211 rom version: e7ae62f
19:38:36.358 > I (517) wifi:wifi driver task: 3fcb2490, prio:23, stack:6144, core=0
19:38:36.369 > I (532) wifi:wifi firmware version: ccaebfa
19:38:36.369 > I (532) wifi:wifi certification version: v7.0
19:38:36.369 > I (533) wifi:config NVS flash: enabled
19:38:36.373 > I (533) wifi:config nano formating: enabled
19:38:36.378 > I (537) wifi:Init data frame dynamic rx buffer num: 32
19:38:36.382 > I (542) wifi:Init static rx mgmt buffer num: 5
19:38:36.387 > I (546) wifi:Init management short buffer num: 32
19:38:36.390 > I (550) wifi:Init dynamic tx buffer num: 32
19:38:36.394 > I (554) wifi:Init static tx FG buffer num: 2
19:38:36.399 > I (558) wifi:Init static rx buffer size: 1600
19:38:36.402 > I (562) wifi:Init static rx buffer num: 10
19:38:36.406 > I (566) wifi:Init dynamic rx buffer num: 32
19:38:36.410 > I (571) wifi_init: rx ba win: 6
19:38:36.414 > I (574) wifi_init: accept mbox: 6
19:38:36.418 > I (578) wifi_init: tcpip mbox: 32
19:38:36.422 > I (582) wifi_init: udp mbox: 6
19:38:36.426 > I (585) wifi_init: tcp mbox: 6
19:38:36.430 > I (589) wifi_init: tcp tx win: 5760
19:38:36.434 > I (593) wifi_init: tcp rx win: 5760
19:38:36.438 > I (597) wifi_init: tcp mss: 1440
19:38:36.442 > I (601) wifi_init: WiFi IRAM OP enabled
19:38:36.447 > I (606) wifi_init: WiFi RX IRAM OP enabled
19:38:36.454 > I (612) phy_init: phy_version 680,a6008b2,Jun  4 2024,16:41:10
19:38:36.495 > W (658) phy_init: saving new calibration data because of checksum failure, mode(0)
19:38:36.509 > I (672) wifi:mode : sta (a0:f2:62:f4:5e:a0)
19:38:36.509 > I (673) wifi:enable tsf
19:38:36.513 > I (674) wifi: connecting to SSID 'bananas'...
19:38:36.520 > I (683) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
19:38:36.521 > I (684) wifi:state: init -> auth (0xb0)
19:38:36.542 > I (705) wifi:state: auth -> assoc (0x0)
19:38:36.562 > I (725) wifi:state: assoc -> run (0x10)
19:38:36.586 > I (749) wifi:connected with bananas, aid = 1, channel 6, BW20, bssid = 24:29:34:c1:29:e5
19:38:36.587 > I (749) wifi:security: WPA2-PSK, phy: bgn, rssi: -83
19:38:36.590 > I (753) wifi:pm start, type: 1
19:38:36.590 > 
19:38:36.598 > I (753) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
19:38:36.606 > I (762) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
19:38:36.613 > I (773) wifi:<ba-add>idx:0 (ifx:0, 24:29:34:c1:29:e5), tid:0, ssn:0, winSize:64
19:38:36.621 > I (780) wifi:dp: 2, bi: 102400, li: 4, scale listen interval from 307200 us to 409600 us
19:38:36.626 > I (784) wifi:AP's beacon interval = 102400 us, DTIM period = 2
19:38:36.633 > I (792) wifi:<ba-add>idx:1 (ifx:0, 24:29:34:c1:29:e5), tid:6, ssn:2, winSize:64
19:38:36.663 > I (826) wifi:<ba-del>idx:1, tid:6
19:38:36.664 > I (827) wifi:<ba-add>idx:1 (ifx:0, 24:29:34:c1:29:e5), tid:6, ssn:3, winSize:64
19:38:37.610 > I (1772) esp_netif_handlers: sta ip: 192.168.86.46, mask: 255.255.255.0, gw: 192.168.86.1
19:38:37.611 > I (1773) wifi: got IP 192.168.86.46
19:38:37.618 > I (1778) device_iot: up — backend 'https', firmware v2.0.0
19:38:37.624 > I (1781) app: running on 'https' backend, v2.0.0 (vGOOD)
19:38:37.652 > I (1815) wifi:[ADDBA]RX DELBA, reason:37, delete tid:0, initiator:0(recipient)
19:38:38.615 > I (2778) ota: requesting job document
19:38:39.080 > I (3242) coreMQTT: MQTT connection established with the broker.
19:38:39.082 > I (3243) mqtt: MQTT connected to AWS IoT Core as 'esp32-ota-poc-01'
19:38:39.091 > I (3254) mqtt: subscribed: dt/esp32-ota-poc-01/cmd
19:38:39.092 > I (3254) ota: (re)subscribing to job topics
19:38:39.099 > I (3261) mqtt: subscribed: $aws/things/esp32-ota-poc-01/jobs/notify-next
19:38:39.107 > I (3269) mqtt: subscribed: $aws/things/esp32-ota-poc-01/jobs/start-next/accepted
19:38:39.112 > I (3271) ota: requesting job document
19:38:39.317 > I (3480) coreMQTT: Ack packet deserialized with result: MQTTSuccess.
19:38:39.320 > I (3481) coreMQTT: State record updated. New state=MQTTPublishDone.
19:38:39.331 > I (3494) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.
19:38:39.336 > I (3494) coreMQTT: State record updated. New state=MQTTPublishDone.
```


## Demo Logs That Show Fleet Working

```
lookfwd@macbookpro ~ % pio device monitor -p /dev/cu.usbmodem211301
--- Terminal on /dev/cu.usbmodem211301 | 9600 8-N-1
--- Available filters and text transformations: debug, default, direct, hexlify, log2file, nocontrol, printable, send_on_enter, time
--- More details at https://bit.ly/pio-monitor-filters
--- Quit: Ctrl+C | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H
Disconnected (read failed: [Errno 6] Device not configured)
Reconnecting to /dev/cu.usbmodem211301 .	 Connected!
W (55186) wifi:Password length matches WPA2 standards, authmode threshold changes from OPEN to WPA2
␛[0;33mW (66336) httpd_txrx: httpd_sock_err: error in recv : 113␛[0m
␛[0;33mW (66346) httpd_txrx: httpd_sock_err: error in recv : 113␛[0m
␛[0;33mW (66346) httpd_txrx: httpd_sock_err: error in recv : 113␛[0m
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0xc (RTC_SW_CPU_RST),boot:0x28 (SPI_FAST_FLASH_BOOT)
Saved PC:0x40375ffd
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce2810,len:0xfb4
load:0x403c8700,len:0x4
load:0x403c8704,len:0xad8
load:0x403cb700,len:0x2ce0
entry 0x403c8888
␛[0;32mI (238) cpu_start: Multicore app␛[0m
␛[0;32mI (247) cpu_start: Pro cpu start user code␛[0m
␛[0;32mI (247) cpu_start: cpu freq: 160000000 Hz␛[0m
␛[0;32mI (247) app_init: Application information:␛[0m
␛[0;32mI (250) app_init: Project name:     esp_ota␛[0m
␛[0;32mI (255) app_init: App version:      1.0.0␛[0m
␛[0;32mI (260) app_init: Compile time:     Jul  1 2026 01:55:32␛[0m
␛[0;32mI (266) app_init: ELF file SHA256:  b204a481c...␛[0m
␛[0;32mI (271) app_init: ESP-IDF:          5.3.1␛[0m
␛[0;32mI (276) efuse_init: Min chip rev:     v0.0␛[0m
␛[0;32mI (280) efuse_init: Max chip rev:     v0.99 ␛[0m
␛[0;32mI (285) efuse_init: Chip rev:         v0.2␛[0m
␛[0;32mI (290) heap_init: Initializing. RAM available for dynamic allocation:␛[0m
␛[0;32mI (298) heap_init: At 3FCA7A00 len 00041D10 (263 KiB): RAM␛[0m
␛[0;32mI (304) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM␛[0m
␛[0;32mI (310) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM␛[0m
␛[0;32mI (316) heap_init: At 600FE100 len 00001EE8 (7 KiB): RTCRAM␛[0m
␛[0;32mI (323) spi_flash: detected chip: generic␛[0m
␛[0;32mI (327) spi_flash: flash io: dio␛[0m
␛[0;32mI (331) sleep: Configure to isolate all GPIO pins in sleep state␛[0m
␛[0;32mI (338) sleep: Enable automatic switching of GPIO sleep configuration␛[0m
␛[0;32mI (345) esp_core_dump_flash: Init core dump to flash␛[0m
␛[0;32mI (351) esp_core_dump_flash: Found partition 'coredump' @ 3e2000 65536 bytes␛[0m
D (358) esp_core_dump_flash: Blank core dump partition!␛[0m
␛[0;32mI (364) main_task: Started on CPU0␛[0m
␛[0;32mI (368) main_task: Calling app_main()␛[0m
␛[0;32mI (372) gpio: GPIO[48]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 ␛[0m
␛[0;32mI (385) device_iot: esp_secure_cert: Thing name from cert CN = 'esp32-ota-poc-02'␛[0m
␛[0;32mI (390) device_iot: esp_secure_cert: device key plaintext in partition␛[0m
␛[0;32mI (419) self_test: ----------------------------------------------------------␛[0m
␛[0;32mI (420) self_test:  firmware v1.0.0   variant=vGOOD␛[0m
␛[0;32mI (422) self_test:  running partition: ota_1 @ 0x240000  (ota state 1)␛[0m
␛[0;32mI (429) self_test:  reset reason: 3   free heap: 299024␛[0m
␛[0;32mI (435) self_test: ----------------------------------------------------------␛[0m
␛[0;33mW (443) self_test: self-test watchdog armed (180000 ms)␛[0m
␛[0;32mI (450) pp: pp rom version: e7ae62f␛[0m
␛[0;32mI (453) net80211: net80211 rom version: e7ae62f␛[0m
I (459) wifi:wifi driver task: 3fcb2ec8, prio:23, stack:6144, core=0
I (477) wifi:wifi firmware version: ccaebfa
I (478) wifi:wifi certification version: v7.0
I (478) wifi:config NVS flash: enabled
I (478) wifi:config nano formating: enabled
I (482) wifi:Init data frame dynamic rx buffer num: 32
I (487) wifi:Init static rx mgmt buffer num: 5
I (491) wifi:Init management short buffer num: 32
I (495) wifi:Init dynamic tx buffer num: 32
I (499) wifi:Init static tx FG buffer num: 2
I (503) wifi:Init static rx buffer size: 1600
I (508) wifi:Init static rx buffer num: 10
I (511) wifi:Init dynamic rx buffer num: 32
␛[0;32mI (516) wifi_init: rx ba win: 6␛[0m
␛[0;32mI (519) wifi_init: accept mbox: 6␛[0m
␛[0;32mI (523) wifi_init: tcpip mbox: 32␛[0m
␛[0;32mI (527) wifi_init: udp mbox: 6␛[0m
␛[0;32mI (531) wifi_init: tcp mbox: 6␛[0m
␛[0;32mI (534) wifi_init: tcp tx win: 5760␛[0m
␛[0;32mI (539) wifi_init: tcp rx win: 5760␛[0m
␛[0;32mI (543) wifi_init: tcp mss: 1440␛[0m
␛[0;32mI (547) wifi_init: WiFi IRAM OP enabled␛[0m
␛[0;32mI (551) wifi_init: WiFi RX IRAM OP enabled␛[0m
␛[0;32mI (581) phy_init: phy_version 680,a6008b2,Jun  4 2024,16:41:10␛[0m
␛[0;33mW (619) phy_init: saving new calibration data because of checksum failure, mode(0)␛[0m
I (663) wifi:mode : sta (a0:f2:62:f4:56:84)
I (663) wifi:enable tsf
␛[0;32mI (665) wifi: connecting to SSID 'bananas'...␛[0m
I (3564) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
I (3566) wifi:state: init -> auth (0xb0)
I (3574) wifi:state: auth -> assoc (0x0)
I (3583) wifi:state: assoc -> run (0x10)
I (3597) wifi:connected with bananas, aid = 10, channel 6, BW20, bssid = b0:e4:d5:6a:f3:8d
I (3598) wifi:security: WPA2-PSK, phy: bgn, rssi: -53
I (3637) wifi:pm start, type: 1

I (3638) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (3638) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
I (3656) wifi:<ba-add>idx:0 (ifx:0, b0:e4:d5:6a:f3:8d), tid:6, ssn:2, winSize:64
I (3668) wifi:dp: 2, bi: 102400, li: 4, scale listen interval from 307200 us to 409600 us
I (3668) wifi:AP's beacon interval = 102400 us, DTIM period = 2
I (3977) wifi:<ba-add>idx:1 (ifx:0, b0:e4:d5:6a:f3:8d), tid:0, ssn:0, winSize:64
␛[0;32mI (4647) esp_netif_handlers: sta ip: 192.168.86.56, mask: 255.255.255.0, gw: 192.168.86.1␛[0m
␛[0;32mI (4648) wifi: got IP 192.168.86.56␛[0m
␛[0;32mI (6037) coreMQTT: MQTT connection established with the broker.␛[0m
␛[0;32mI (6037) mqtt: MQTT connected to AWS IoT Core as 'esp32-ota-poc-02'␛[0m
␛[0;33mW (6129) ota: trial boot detected — running self-test␛[0m
␛[0;32mI (6129) self_test: core-function check: free heap = 204372␛[0m
␛[0;32mI (6130) self_test: app health check -> PASS␛[0m
␛[0;32mI (6158) self_test: ␛[1;32mimage COMMITTED (rollback cancelled)␛[0m␛[0m
␛[0;32mI (6159) self_test: self-test watchdog disarmed␛[0m
␛[0;32mI (6163) ota: reporting job AFR_OTA-esp32-ota-https-3-0-0-1782885637 -> SUCCEEDED␛[0m
␛[0;32mI (6200) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (6200) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (6307) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.␛[0m
␛[0;32mI (6307) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (11168) ota: (re)subscribing to job topics␛[0m
␛[0;32mI (11205) mqtt: subscribed: $aws/things/esp32-ota-poc-02/jobs/start-next/accepted␛[0m
␛[0;32mI (11207) ota: requesting job document␛[0m
␛[0;32mI (11208) device_iot: up — backend 'https', firmware v1.0.0␛[0m
␛[0;32mI (11318) mqtt: subscribed: dt/esp32-ota-poc-02/cmd␛[0m
␛[0;32mI (11318) app: running on 'https' backend, v1.0.0 (vGOOD)␛[0m
␛[0;32mI (11372) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.␛[0m
␛[0;32mI (11373) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (11424) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (11424) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (41449) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (41450) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (71469) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (71470) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
Disconnected (read failed: [Errno 6] Device not configured)
Reconnecting to /dev/cu.usbmodem211301 .	 Connected!
I (3060) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
I (3061) wifi:state: init -> auth (0xb0)
I (3075) wifi:state: auth -> assoc (0x0)
I (3089) wifi:state: assoc -> run (0x10)
I (3104) wifi:connected with bananas, aid = 10, channel 6, BW20, bssid = b0:e4:d5:6a:f3:8d
I (3105) wifi:security: WPA2-PSK, phy: bgn, rssi: -50
I (3110) wifi:pm start, type: 1

I (3111) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (3117) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
I (3126) wifi:dp: 2, bi: 102400, li: 4, scale listen interval from 307200 us to 409600 us
I (3133) wifi:AP's beacon interval = 102400 us, DTIM period = 2
I (3145) wifi:<ba-add>idx:0 (ifx:0, b0:e4:d5:6a:f3:8d), tid:6, ssn:2, winSize:64
␛[0;32mI (4142) esp_netif_handlers: sta ip: 192.168.86.56, mask: 255.255.255.0, gw: 192.168.86.1␛[0m
␛[0;32mI (4142) wifi: got IP 192.168.86.56␛[0m
␛[0;32mI (4147) device_iot: up — backend 'https', firmware v1.0.0␛[0m
␛[0;32mI (4151) app: running on 'https' backend, v1.0.0 (vGOOD)␛[0m
I (4152) wifi:<ba-add>idx:1 (ifx:0, b0:e4:d5:6a:f3:8d), tid:0, ssn:0, winSize:64
␛[0;32mI (5356) coreMQTT: MQTT connection established with the broker.␛[0m
␛[0;32mI (5357) mqtt: MQTT connected to AWS IoT Core as 'esp32-ota-poc-02'␛[0m
␛[0;32mI (5368) mqtt: subscribed: dt/esp32-ota-poc-02/cmd␛[0m
␛[0;32mI (5369) ota: (re)subscribing to job topics␛[0m
␛[0;32mI (5376) mqtt: subscribed: $aws/things/esp32-ota-poc-02/jobs/start-next/accepted␛[0m
␛[0;32mI (5377) ota: requesting job document␛[0m
␛[0;32mI (5545) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (5545) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (5598) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.␛[0m
␛[0;32mI (5599) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (5628) AWS_OTA: otaPal_SetPlatformImageState, 2␛[0m
␛[0;32mI (5629) AWS_OTA: Set image as valid one!␛[0m
␛[0;32mI (5629) esp_ota_ops: aws_esp_ota_get_boot_flags: 1␛[0m
␛[0;32mI (5633) esp_ota_ops: [0] aflags/seq:0x2/0x2, pflags/seq:0xffffffff/0x0␛[0m
␛[0;33mW (5640) AWS_OTA: Image not in self test mode 2␛[0m
␛[0;32mI (5646) esp_ota_ops: aws_esp_ota_get_boot_flags: 1␛[0m
␛[0;32mI (5651) esp_ota_ops: [0] aflags/seq:0x2/0x2, pflags/seq:0xffffffff/0x0␛[0m
␛[0;32mI (5659) AWS_OTA: Writing to partition subtype 16 at offset 0xb0000␛[0m
␛[0;32mI (7072) AWS_OTA: esp_ota_begin succeeded␛[0m
␛[0;32mI (7073) ota: OTA job accepted (HTTP data path), 987232 bytes␛[0m
␛[0;32mI (7073) ota: reporting job AFR_OTA-esp32-ota-https-2-0-0-1782887508 -> IN_PROGRESS␛[0m
␛[0;32mI (7241) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.␛[0m
␛[0;32mI (7242) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (7583) ota: downloading ld bytes over HTTPS␛[0m
␛[0;32mI (8133) ota: downloaded 102400 / 987232 bytes (10%)␛[0m
␛[0;32mI (8871) ota: downloaded 200704 / 987232 bytes (20%)␛[0m
␛[0;32mI (9484) ota: downloaded 299008 / 987232 bytes (30%)␛[0m
␛[0;32mI (10392) ota: downloaded 397312 / 987232 bytes (40%)␛[0m
␛[0;32mI (11045) ota: downloaded 495616 / 987232 bytes (50%)␛[0m
␛[0;32mI (11730) ota: downloaded 593920 / 987232 bytes (60%)␛[0m
␛[0;32mI (12257) ota: downloaded 692224 / 987232 bytes (70%)␛[0m
␛[0;32mI (12841) ota: downloaded 790528 / 987232 bytes (80%)␛[0m
␛[0;32mI (13437) ota: downloaded 888832 / 987232 bytes (90%)␛[0m
␛[0;32mI (13905) ota: downloaded 987232 / 987232 bytes (100%)␛[0m
␛[0;32mI (13908) ota: download complete (987232 bytes)␛[0m
␛[0;32mI (13909) ota: verifying signature + closing file␛[0m
␛[0;32mI (14213) AWS_OTA: Signature verification succeeded.␛[0m
␛[0;32mI (14214) ota: ␛[1;32msignature OK -> activating + rebooting␛[0m␛[0m
␛[0;32mI (14217) esp_image: segment 0: paddr=000b0020 vaddr=3c0b0020 size=32af4h (207604) map␛[0m
␛[0;32mI (14252) esp_image: segment 1: paddr=000e2b1c vaddr=3fc99700 size=04fcch ( 20428) ␛[0m
␛[0;32mI (14255) esp_image: segment 2: paddr=000e7af0 vaddr=40374000 size=08528h ( 34088) ␛[0m
␛[0;32mI (14263) esp_image: segment 3: paddr=000f0020 vaddr=42000020 size=a3ee4h (671460) map␛[0m
␛[0;32mI (14356) esp_image: segment 4: paddr=00193f0c vaddr=4037c528 size=0d124h ( 53540) ␛[0m
␛[0;32mI (14365) esp_image: segment 0: paddr=000b0020 vaddr=3c0b0020 size=32af4h (207604) map␛[0m
␛[0;32mI (14394) esp_image: segment 1: paddr=000e2b1c vaddr=3fc99700 size=04fcch ( 20428) ␛[0m
␛[0;32mI (14398) esp_image: segment 2: paddr=000e7af0 vaddr=40374000 size=08528h ( 34088) ␛[0m
␛[0;32mI (14405) esp_image: segment 3: paddr=000f0020 vaddr=42000020 size=a3ee4h (671460) map␛[0m
␛[0;32mI (14500) esp_image: segment 4: paddr=00193f0c vaddr=4037c528 size=0d124h ( 53540) ␛[0m
I (15011) wifi:state: run -> init (0x0)
I (15014) wifi:pm stop, total sleep time: lu us / lu us

I (15014) wifi:<ba-del>idx:1, tid:0
I (15015) wifi:<ba-del>idx:0, tid:6
I (15016) wifi:new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
␛[0;31mE (15029) esp-tls-mbedtls: read error :-0x000zX:␛[0m
␛[0;31mE (15029) network_transport: Error reading: -76␛[0m
␛[0;31mE (15034) coreMQTT: Call to receiveSingleIteration failed. Status=MQTTRecvFailed␛[0m
␛[0;31mE (15042) mqtt: process loop error: MQTTRecvFailed -> reconnecting␛[0m
␛[0;33mW (15050) app: offline␛[0m
␛[0;31mE (15053) esp-tls: [sock=54] connect() error: Host is unreachable␛[0m
I (15054) wifi:flush txq
␛[0;31mE (15059) esp-tls: Failed to open new connection␛[0m
I (15060) wifi:stop sw txq
␛[0;31mE (15066) mqtt: TLS connect to a2fm5sbtk65rj6-ats.iot.us-east-1.amazonaws.com:8883 failed␛[0m
I (15068) wifi:lmac stop hw txq
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0xc (RTC_SW_CPU_RST),boot:0x28 (SPI_FAST_FLASH_BOOT)
Saved PC:0x4037acca
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce2810,len:0xfb4
load:0x403c8700,len:0x4
load:0x403c8704,len:0xad8
load:0x403cb700,len:0x2ce0
entry 0x403c8888
␛[0;32mI (235) cpu_start: Multicore app␛[0m
␛[0;32mI (244) cpu_start: Pro cpu start user code␛[0m
␛[0;32mI (244) cpu_start: cpu freq: 160000000 Hz␛[0m
␛[0;32mI (244) app_init: Application information:␛[0m
␛[0;32mI (247) app_init: Project name:     esp_ota␛[0m
␛[0;32mI (251) app_init: App version:      1.0.0␛[0m
␛[0;32mI (256) app_init: Compile time:     Jul  1 2026 01:54:44␛[0m
␛[0;32mI (262) app_init: ELF file SHA256:  47be4314d...␛[0m
␛[0;32mI (267) app_init: ESP-IDF:          5.3.1␛[0m
␛[0;32mI (272) efuse_init: Min chip rev:     v0.0␛[0m
␛[0;32mI (277) efuse_init: Max chip rev:     v0.99 ␛[0m
␛[0;32mI (282) efuse_init: Chip rev:         v0.2␛[0m
␛[0;32mI (287) heap_init: Initializing. RAM available for dynamic allocation:␛[0m
␛[0;32mI (294) heap_init: At 3FCA7A00 len 00041D10 (263 KiB): RAM␛[0m
␛[0;32mI (300) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM␛[0m
␛[0;32mI (306) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM␛[0m
␛[0;32mI (312) heap_init: At 600FE100 len 00001EE8 (7 KiB): RTCRAM␛[0m
␛[0;32mI (320) spi_flash: detected chip: generic␛[0m
␛[0;32mI (323) spi_flash: flash io: dio␛[0m
␛[0;32mI (328) sleep: Configure to isolate all GPIO pins in sleep state␛[0m
␛[0;32mI (334) sleep: Enable automatic switching of GPIO sleep configuration␛[0m
␛[0;32mI (341) esp_core_dump_flash: Init core dump to flash␛[0m
␛[0;32mI (347) esp_core_dump_flash: Found partition 'coredump' @ 3e2000 65536 bytes␛[0m
D (354) esp_core_dump_flash: Blank core dump partition!␛[0m
␛[0;32mI (360) main_task: Started on CPU0␛[0m
␛[0;32mI (364) main_task: Calling app_main()␛[0m
␛[0;32mI (368) gpio: GPIO[48]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 ␛[0m
␛[0;32mI (381) device_iot: esp_secure_cert: Thing name from cert CN = 'esp32-ota-poc-02'␛[0m
␛[0;32mI (386) device_iot: esp_secure_cert: device key plaintext in partition␛[0m
␛[0;32mI (416) self_test: ----------------------------------------------------------␛[0m
␛[0;32mI (417) self_test:  firmware v2.0.0   variant=vGOOD␛[0m
␛[0;32mI (419) self_test:  running partition: ota_0 @ 0x0b0000  (ota state 1)␛[0m
␛[0;32mI (426) self_test:  reset reason: 3   free heap: 299024␛[0m
␛[0;32mI (432) self_test: ----------------------------------------------------------␛[0m
␛[0;33mW (440) self_test: self-test watchdog armed (180000 ms)␛[0m
␛[0;32mI (447) pp: pp rom version: e7ae62f␛[0m
␛[0;32mI (450) net80211: net80211 rom version: e7ae62f␛[0m
I (456) wifi:wifi driver task: 3fcb2ec8, prio:23, stack:6144, core=0
I (474) wifi:wifi firmware version: ccaebfa
I (475) wifi:wifi certification version: v7.0
I (475) wifi:config NVS flash: enabled
I (475) wifi:config nano formating: enabled
I (479) wifi:Init data frame dynamic rx buffer num: 32
I (484) wifi:Init static rx mgmt buffer num: 5
I (488) wifi:Init management short buffer num: 32
I (492) wifi:Init dynamic tx buffer num: 32
I (496) wifi:Init static tx FG buffer num: 2
I (500) wifi:Init static rx buffer size: 1600
I (504) wifi:Init static rx buffer num: 10
I (508) wifi:Init dynamic rx buffer num: 32
␛[0;32mI (513) wifi_init: rx ba win: 6␛[0m
␛[0;32mI (516) wifi_init: accept mbox: 6␛[0m
␛[0;32mI (520) wifi_init: tcpip mbox: 32␛[0m
␛[0;32mI (524) wifi_init: udp mbox: 6␛[0m
␛[0;32mI (528) wifi_init: tcp mbox: 6␛[0m
␛[0;32mI (531) wifi_init: tcp tx win: 5760␛[0m
␛[0;32mI (536) wifi_init: tcp rx win: 5760␛[0m
␛[0;32mI (540) wifi_init: tcp mss: 1440␛[0m
␛[0;32mI (544) wifi_init: WiFi IRAM OP enabled␛[0m
␛[0;32mI (548) wifi_init: WiFi RX IRAM OP enabled␛[0m
␛[0;32mI (555) phy_init: phy_version 680,a6008b2,Jun  4 2024,16:41:10␛[0m
I (599) wifi:mode : sta (a0:f2:62:f4:56:84)
I (599) wifi:enable tsf
␛[0;32mI (601) wifi: connecting to SSID 'bananas'...␛[0m
I (3013) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
I (3014) wifi:state: init -> auth (0xb0)
I (3022) wifi:state: auth -> assoc (0x0)
I (3032) wifi:state: assoc -> run (0x10)
I (3046) wifi:connected with bananas, aid = 8, channel 6, BW20, bssid = b0:e4:d5:6a:f3:8d
I (3046) wifi:security: WPA2-PSK, phy: bgn, rssi: -53
I (3050) wifi:pm start, type: 1

I (3051) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (3059) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
I (3075) wifi:<ba-add>idx:0 (ifx:0, b0:e4:d5:6a:f3:8d), tid:6, ssn:2, winSize:64
I (3086) wifi:dp: 2, bi: 102400, li: 4, scale listen interval from 307200 us to 409600 us
I (3087) wifi:AP's beacon interval = 102400 us, DTIM period = 2
␛[0;32mI (4070) esp_netif_handlers: sta ip: 192.168.86.56, mask: 255.255.255.0, gw: 192.168.86.1␛[0m
␛[0;32mI (4070) wifi: got IP 192.168.86.56␛[0m
I (4076) wifi:<ba-add>idx:1 (ifx:0, b0:e4:d5:6a:f3:8d), tid:0, ssn:0, winSize:64
␛[0;32mI (5446) coreMQTT: MQTT connection established with the broker.␛[0m
␛[0;32mI (5447) mqtt: MQTT connected to AWS IoT Core as 'esp32-ota-poc-02'␛[0m
␛[0;33mW (5475) ota: trial boot detected — running self-test␛[0m
␛[0;32mI (5475) self_test: core-function check: free heap = 204488␛[0m
␛[0;32mI (5476) self_test: app health check -> PASS␛[0m
␛[0;32mI (5498) self_test: ␛[1;32mimage COMMITTED (rollback cancelled)␛[0m␛[0m
␛[0;32mI (5499) self_test: self-test watchdog disarmed␛[0m
␛[0;32mI (5500) ota: reporting job AFR_OTA-esp32-ota-https-2-0-0-1782887508 -> SUCCEEDED␛[0m
␛[0;32mI (5592) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (5592) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (5620) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.␛[0m
␛[0;32mI (5620) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (10510) ota: (re)subscribing to job topics␛[0m
␛[0;32mI (10521) mqtt: subscribed: $aws/things/esp32-ota-poc-02/jobs/start-next/accepted␛[0m
␛[0;32mI (10521) ota: requesting job document␛[0m
␛[0;32mI (10523) device_iot: up — backend 'https', firmware v2.0.0␛[0m
␛[0;32mI (10636) mqtt: subscribed: dt/esp32-ota-poc-02/cmd␛[0m
␛[0;32mI (10636) app: running on 'https' backend, v2.0.0 (vGOOD)␛[0m
␛[0;32mI (10812) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (10813) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (10827) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.␛[0m
␛[0;32mI (10828) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (40762) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (40762) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (70814) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (70815) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (100987) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (100988) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (130814) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (130815) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (160728) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (160729) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (190762) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (190763) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (220787) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (220787) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (250809) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (250809) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (280717) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (280718) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
Disconnected (read failed: [Errno 6] Device not configured)
Reconnecting to /dev/cu.usbmodem211301 .	 Connected!
I (2998) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
I (3000) wifi:state: init -> auth (0xb0)
I (3012) wifi:state: auth -> assoc (0x0)
I (3022) wifi:state: assoc -> run (0x10)
I (3039) wifi:connected with bananas, aid = 8, channel 6, BW20, bssid = b0:e4:d5:6a:f3:8d
I (3040) wifi:security: WPA2-PSK, phy: bgn, rssi: -55
I (3043) wifi:pm start, type: 1

I (3044) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (3052) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
I (3066) wifi:<ba-add>idx:0 (ifx:0, b0:e4:d5:6a:f3:8d), tid:6, ssn:2, winSize:64
I (3080) wifi:dp: 2, bi: 102400, li: 4, scale listen interval from 307200 us to 409600 us
I (3081) wifi:AP's beacon interval = 102400 us, DTIM period = 2
␛[0;32mI (4063) esp_netif_handlers: sta ip: 192.168.86.56, mask: 255.255.255.0, gw: 192.168.86.1␛[0m
␛[0;32mI (4063) wifi: got IP 192.168.86.56␛[0m
␛[0;32mI (4068) device_iot: up — backend 'https', firmware v2.0.0␛[0m
I (4070) wifi:<ba-add>idx:1 (ifx:0, b0:e4:d5:6a:f3:8d), tid:0, ssn:0, winSize:64
␛[0;32mI (4079) app: running on 'https' backend, v2.0.0 (vGOOD)␛[0m
␛[0;32mI (5431) coreMQTT: MQTT connection established with the broker.␛[0m
␛[0;32mI (5432) mqtt: MQTT connected to AWS IoT Core as 'esp32-ota-poc-02'␛[0m
␛[0;32mI (5443) mqtt: subscribed: dt/esp32-ota-poc-02/cmd␛[0m
␛[0;32mI (5444) ota: (re)subscribing to job topics␛[0m
␛[0;32mI (5451) mqtt: subscribed: $aws/things/esp32-ota-poc-02/jobs/start-next/accepted␛[0m
␛[0;32mI (5452) ota: requesting job document␛[0m
␛[0;32mI (5609) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (5609) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (5657) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.␛[0m
␛[0;32mI (5658) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (5686) AWS_OTA: otaPal_SetPlatformImageState, 2␛[0m
␛[0;32mI (5687) AWS_OTA: Set image as valid one!␛[0m
␛[0;32mI (5687) esp_ota_ops: aws_esp_ota_get_boot_flags: 1␛[0m
␛[0;32mI (5691) esp_ota_ops: [1] aflags/seq:0x2/0x3, pflags/seq:0x2/0x2␛[0m
␛[0;33mW (5698) AWS_OTA: Image not in self test mode 2␛[0m
␛[0;32mI (5703) esp_ota_ops: aws_esp_ota_get_boot_flags: 1␛[0m
␛[0;32mI (5709) esp_ota_ops: [1] aflags/seq:0x2/0x3, pflags/seq:0x2/0x2␛[0m
␛[0;32mI (5715) AWS_OTA: Writing to partition subtype 17 at offset 0x240000␛[0m
␛[0;32mI (7154) AWS_OTA: esp_ota_begin succeeded␛[0m
␛[0;32mI (7154) ota: OTA job accepted (HTTP data path), 987264 bytes␛[0m
␛[0;32mI (7155) ota: reporting job AFR_OTA-esp32-ota-https-3-0-0-bad-1782887825 -> IN_PROGRESS␛[0m
␛[0;32mI (7371) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.␛[0m
␛[0;32mI (7371) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (7738) ota: downloading ld bytes over HTTPS␛[0m
␛[0;32mI (8373) ota: downloaded 102400 / 987264 bytes (10%)␛[0m
␛[0;32mI (8911) ota: downloaded 200704 / 987264 bytes (20%)␛[0m
␛[0;32mI (9485) ota: downloaded 299008 / 987264 bytes (30%)␛[0m
␛[0;32mI (9980) ota: downloaded 397312 / 987264 bytes (40%)␛[0m
␛[0;32mI (10529) ota: downloaded 495616 / 987264 bytes (50%)␛[0m
␛[0;32mI (11196) ota: downloaded 593920 / 987264 bytes (60%)␛[0m
␛[0;32mI (11709) ota: downloaded 692224 / 987264 bytes (70%)␛[0m
␛[0;32mI (12260) ota: downloaded 790528 / 987264 bytes (80%)␛[0m
␛[0;32mI (12838) ota: downloaded 888832 / 987264 bytes (90%)␛[0m
␛[0;32mI (13414) ota: downloaded 987264 / 987264 bytes (100%)␛[0m
␛[0;32mI (13417) ota: download complete (987264 bytes)␛[0m
␛[0;32mI (13417) ota: verifying signature + closing file␛[0m
␛[0;32mI (13720) AWS_OTA: Signature verification succeeded.␛[0m
␛[0;32mI (13722) ota: ␛[1;32msignature OK -> activating + rebooting␛[0m␛[0m
␛[0;32mI (13724) esp_image: segment 0: paddr=00240020 vaddr=3c0b0020 size=32b14h (207636) map␛[0m
␛[0;32mI (13759) esp_image: segment 1: paddr=00272b3c vaddr=3fc99700 size=04fcch ( 20428) ␛[0m
␛[0;32mI (13763) esp_image: segment 2: paddr=00277b10 vaddr=40374000 size=08508h ( 34056) ␛[0m
␛[0;32mI (13770) esp_image: segment 3: paddr=00280020 vaddr=42000020 size=a3ee4h (671460) map␛[0m
␛[0;32mI (13864) esp_image: segment 4: paddr=00323f0c vaddr=4037c508 size=0d144h ( 53572) ␛[0m
␛[0;32mI (13872) esp_image: segment 0: paddr=00240020 vaddr=3c0b0020 size=32b14h (207636) map␛[0m
␛[0;32mI (13901) esp_image: segment 1: paddr=00272b3c vaddr=3fc99700 size=04fcch ( 20428) ␛[0m
␛[0;32mI (13904) esp_image: segment 2: paddr=00277b10 vaddr=40374000 size=08508h ( 34056) ␛[0m
␛[0;32mI (13911) esp_image: segment 3: paddr=00280020 vaddr=42000020 size=a3ee4h (671460) map␛[0m
␛[0;32mI (14007) esp_image: segment 4: paddr=00323f0c vaddr=4037c508 size=0d144h ( 53572) ␛[0m
I (14532) wifi:state: run -> init (0x0)
I (14535) wifi:pm stop, total sleep time: lu us / lu us

I (14535) wifi:<ba-del>idx:1, tid:0
I (14536) wifi:<ba-del>idx:0, tid:6
I (14537) wifi:new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
␛[0;31mE (14546) network_transport: Error reading the message␛[0m
␛[0;31mE (14550) coreMQTT: Call to receiveSingleIteration failed. Status=MQTTRecvFailed␛[0m
␛[0;31mE (14558) mqtt: process loop error: MQTTRecvFailed -> reconnecting␛[0m
␛[0;33mW (14566) app: offline␛[0m
␛[0;31mE (14569) esp-tls: [sock=54] connect() error: Host is unreachable␛[0m
␛[0;31mE (14575) esp-tls: Failed to open new connection␛[0m
I (14576) wifi:flush txq
␛[0;31mE (14580) mqtt: TLS connect to a2fm5sbtk65rj6-ats.iot.us-east-1.amazonaws.com:8883 failed␛[0m
I (14582) wifi:stop sw txq
I (14593) wifi:lmac stop hw txq



ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0xc (RTC_SW_CPU_RST),boot:0x2a (SPI_FAST_FLASH_BOOT)
Saved PC:0x4037acca
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce2810,len:0xfb4
load:0x403c8700,len:0x4
load:0x403c8704,len:0xad8
load:0x403cb700,len:0x2ce0
entry 0x403c8888
␛[0;32mI (236) cpu_start: Multicore app␛[0m
␛[0;32mI (245) cpu_start: Pro cpu start user code␛[0m
␛[0;32mI (245) cpu_start: cpu freq: 160000000 Hz␛[0m
␛[0;32mI (245) app_init: Application information:␛[0m
␛[0;32mI (248) app_init: Project name:     esp_ota␛[0m
␛[0;32mI (253) app_init: App version:      1.0.0␛[0m
␛[0;32mI (258) app_init: Compile time:     Jul  1 2026 01:57:38␛[0m
␛[0;32mI (264) app_init: ELF file SHA256:  d6396ac49...␛[0m
␛[0;32mI (269) app_init: ESP-IDF:          5.3.1␛[0m
␛[0;32mI (274) efuse_init: Min chip rev:     v0.0␛[0m
␛[0;32mI (279) efuse_init: Max chip rev:     v0.99 ␛[0m
␛[0;32mI (283) efuse_init: Chip rev:         v0.2␛[0m
␛[0;32mI (288) heap_init: Initializing. RAM available for dynamic allocation:␛[0m
␛[0;32mI (296) heap_init: At 3FCA7A00 len 00041D10 (263 KiB): RAM␛[0m
␛[0;32mI (302) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM␛[0m
␛[0;32mI (308) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM␛[0m
␛[0;32mI (314) heap_init: At 600FE100 len 00001EE8 (7 KiB): RTCRAM␛[0m
␛[0;32mI (321) spi_flash: detected chip: generic␛[0m
␛[0;32mI (325) spi_flash: flash io: dio␛[0m
␛[0;32mI (329) sleep: Configure to isolate all GPIO pins in sleep state␛[0m
␛[0;32mI (336) sleep: Enable automatic switching of GPIO sleep configuration␛[0m
␛[0;32mI (343) esp_core_dump_flash: Init core dump to flash␛[0m
␛[0;32mI (349) esp_core_dump_flash: Found partition 'coredump' @ 3e2000 65536 bytes␛[0m
D (356) esp_core_dump_flash: Blank core dump partition!␛[0m
␛[0;32mI (362) main_task: Started on CPU0␛[0m
␛[0;32mI (366) main_task: Calling app_main()␛[0m
␛[0;32mI (370) gpio: GPIO[48]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 ␛[0m
␛[0;32mI (383) device_iot: esp_secure_cert: Thing name from cert CN = 'esp32-ota-poc-02'␛[0m
␛[0;32mI (388) device_iot: esp_secure_cert: device key plaintext in partition␛[0m
␛[0;32mI (418) self_test: ----------------------------------------------------------␛[0m
␛[0;32mI (419) self_test:  firmware v3.0.0   variant=vBAD␛[0m
␛[0;32mI (421) self_test:  running partition: ota_1 @ 0x240000  (ota state 1)␛[0m
␛[0;32mI (428) self_test:  reset reason: 3   free heap: 299024␛[0m
␛[0;32mI (434) self_test: ----------------------------------------------------------␛[0m
␛[0;33mW (442) self_test: self-test watchdog armed (180000 ms)␛[0m
␛[0;32mI (449) pp: pp rom version: e7ae62f␛[0m
␛[0;32mI (452) net80211: net80211 rom version: e7ae62f␛[0m
I (458) wifi:wifi driver task: 3fcb2ec8, prio:23, stack:6144, core=0
I (476) wifi:wifi firmware version: ccaebfa
I (477) wifi:wifi certification version: v7.0
I (477) wifi:config NVS flash: enabled
I (477) wifi:config nano formating: enabled
I (481) wifi:Init data frame dynamic rx buffer num: 32
I (486) wifi:Init static rx mgmt buffer num: 5
I (490) wifi:Init management short buffer num: 32
I (494) wifi:Init dynamic tx buffer num: 32
I (498) wifi:Init static tx FG buffer num: 2
I (502) wifi:Init static rx buffer size: 1600
I (506) wifi:Init static rx buffer num: 10
I (510) wifi:Init dynamic rx buffer num: 32
␛[0;32mI (515) wifi_init: rx ba win: 6␛[0m
␛[0;32mI (518) wifi_init: accept mbox: 6␛[0m
␛[0;32mI (522) wifi_init: tcpip mbox: 32␛[0m
␛[0;32mI (526) wifi_init: udp mbox: 6␛[0m
␛[0;32mI (530) wifi_init: tcp mbox: 6␛[0m
␛[0;32mI (533) wifi_init: tcp tx win: 5760␛[0m
␛[0;32mI (537) wifi_init: tcp rx win: 5760␛[0m
␛[0;32mI (542) wifi_init: tcp mss: 1440␛[0m
␛[0;32mI (546) wifi_init: WiFi IRAM OP enabled␛[0m
␛[0;32mI (550) wifi_init: WiFi RX IRAM OP enabled␛[0m
␛[0;32mI (557) phy_init: phy_version 680,a6008b2,Jun  4 2024,16:41:10␛[0m
I (601) wifi:mode : sta (a0:f2:62:f4:56:84)
I (601) wifi:enable tsf
␛[0;32mI (603) wifi: connecting to SSID 'bananas'...␛[0m
I (3015) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
I (3016) wifi:state: init -> auth (0xb0)
I (3023) wifi:state: auth -> assoc (0x0)
I (3030) wifi:state: assoc -> run (0x10)
I (3046) wifi:connected with bananas, aid = 8, channel 6, BW20, bssid = b0:e4:d5:6a:f3:8d
I (3046) wifi:security: WPA2-PSK, phy: bgn, rssi: -50
I (3050) wifi:pm start, type: 1
I (3051) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (3059) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
I (3073) wifi:<ba-add>idx:0 (ifx:0, b0:e4:d5:6a:f3:8d), tid:6, ssn:2, winSize:64
I (3114) wifi:dp: 2, bi: 102400, li: 4, scale listen interval from 307200 us to 409600 us
I (3114) wifi:AP's beacon interval = 102400 us, DTIM period = 2
I (3424) wifi:<ba-add>idx:1 (ifx:0, b0:e4:d5:6a:f3:8d), tid:0, ssn:0, winSize:64
␛[0;32mI (4069) esp_netif_handlers: sta ip: 192.168.86.56, mask: 255.255.255.0, gw: 192.168.86.1␛[0m
␛[0;32mI (4070) wifi: got IP 192.168.86.56␛[0m
␛[0;32mI (5452) coreMQTT: MQTT connection established with the broker.␛[0m
␛[0;32mI (5453) mqtt: MQTT connected to AWS IoT Core as 'esp32-ota-poc-02'␛[0m
␛[0;33mW (5475) ota: trial boot detected — running self-test␛[0m
␛[0;32mI (5475) self_test: core-function check: free heap = 204460␛[0m
␛[0;32mI (5476) self_test: app health check -> FAIL␛[0m
␛[0;31mE (5481) ota: self-test failed (cloud=1 core=0) -> rollback␛[0m
␛[0;32mI (5488) ota: reporting job AFR_OTA-esp32-ota-https-3-0-0-bad-1782887825 -> FAILED␛[0m
␛[0;32mI (5607) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (5607) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (5621) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.␛[0m
␛[0;32mI (5622) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;31mE (6997) self_test: ␛[1;31mimage REJECTED -> rolling back to previous slot␛[0m␛[0m
␛[0;32mI (7151) esp_ota_ops: Rollback to previously worked partition. Restart.␛[0m
I (7153) wifi:state: run -> init (0x0)
I (7155) wifi:pm stop, total sleep time: lu us / lu us

I (7157) wifi:<ba-del>idx:1, tid:0
I (7160) wifi:<ba-del>idx:0, tid:6
I (7163) wifi:new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
␛[0;31mE (7173) network_transport: Error reading the message␛[0m
␛[0;31mE (7176) coreMQTT: Call to receiveSingleIteration failed. Status=MQTTRecvFailed␛[0m
␛[0;31mE (7184) mqtt: process loop error: MQTTRecvFailed -> reconnecting␛[0m
␛[0;33mW (7192) app: offline␛[0m
␛[0;31mE (7195) esp-tls: [sock=54] connect() error: Host is unreachable␛[0m
␛[0;31mE (7201) esp-tls: Failed to open new connection␛[0m
␛[0;31mE (7206) mqtt: TLS connect to a2fm5sbtk65rj6-ats.iot.us-east-1.amazonaws.com:8883 failed␛[0m
I (7215) wifi:flush txq
I (7216) wifi:stop sw txq
I (7219) wifi:lmac stop hw txq
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0xc (RTC_SW_CPU_RST),boot:0x2a (SPI_FAST_FLASH_BOOT)
Saved PC:0x403769bc
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce2810,len:0xfb4
load:0x403c8700,len:0x4
load:0x403c8704,len:0xad8
load:0x403cb700,len:0x2ce0
entry 0x403c8888
␛[0;32mI (221) cpu_start: Multicore app␛[0m
␛[0;32mI (230) cpu_start: Pro cpu start user code␛[0m
␛[0;32mI (230) cpu_start: cpu freq: 160000000 Hz␛[0m
␛[0;32mI (230) app_init: Application information:␛[0m
␛[0;32mI (233) app_init: Project name:     esp_ota␛[0m
␛[0;32mI (238) app_init: App version:      1.0.0␛[0m
␛[0;32mI (242) app_init: Compile time:     Jul  1 2026 01:54:44␛[0m
␛[0;32mI (248) app_init: ELF file SHA256:  47be4314d...␛[0m
␛[0;32mI (254) app_init: ESP-IDF:          5.3.1␛[0m
␛[0;32mI (258) efuse_init: Min chip rev:     v0.0␛[0m
␛[0;32mI (263) efuse_init: Max chip rev:     v0.99 ␛[0m
␛[0;32mI (268) efuse_init: Chip rev:         v0.2␛[0m
␛[0;32mI (273) heap_init: Initializing. RAM available for dynamic allocation:␛[0m
␛[0;32mI (280) heap_init: At 3FCA7A00 len 00041D10 (263 KiB): RAM␛[0m
␛[0;32mI (286) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM␛[0m
␛[0;32mI (292) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM␛[0m
␛[0;32mI (298) heap_init: At 600FE100 len 00001EE8 (7 KiB): RTCRAM␛[0m
␛[0;32mI (306) spi_flash: detected chip: generic␛[0m
␛[0;32mI (309) spi_flash: flash io: dio␛[0m
␛[0;32mI (314) sleep: Configure to isolate all GPIO pins in sleep state␛[0m
␛[0;32mI (320) sleep: Enable automatic switching of GPIO sleep configuration␛[0m
␛[0;32mI (327) esp_core_dump_flash: Init core dump to flash␛[0m
␛[0;32mI (333) esp_core_dump_flash: Found partition 'coredump' @ 3e2000 65536 bytes␛[0m
D (341) esp_core_dump_flash: Blank core dump partition!␛[0m
␛[0;32mI (346) main_task: Started on CPU0␛[0m
␛[0;32mI (350) main_task: Calling app_main()␛[0m
␛[0;32mI (354) gpio: GPIO[48]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 ␛[0m
␛[0;32mI (367) device_iot: esp_secure_cert: Thing name from cert CN = 'esp32-ota-poc-02'␛[0m
␛[0;32mI (372) device_iot: esp_secure_cert: device key plaintext in partition␛[0m
␛[0;32mI (402) self_test: ----------------------------------------------------------␛[0m
␛[0;32mI (402) self_test:  firmware v2.0.0   variant=vGOOD␛[0m
␛[0;32mI (404) self_test:  running partition: ota_0 @ 0x0b0000  (ota state 2)␛[0m
␛[0;32mI (412) self_test:  reset reason: 3   free heap: 299024␛[0m
␛[0;32mI (417) self_test: ----------------------------------------------------------␛[0m
␛[0;32mI (427) pp: pp rom version: e7ae62f␛[0m
␛[0;32mI (430) net80211: net80211 rom version: e7ae62f␛[0m
I (436) wifi:wifi driver task: 3fcb2e80, prio:23, stack:6144, core=0
I (454) wifi:wifi firmware version: ccaebfa
I (454) wifi:wifi certification version: v7.0
I (455) wifi:config NVS flash: enabled
I (455) wifi:config nano formating: enabled
I (459) wifi:Init data frame dynamic rx buffer num: 32
I (464) wifi:Init static rx mgmt buffer num: 5
I (468) wifi:Init management short buffer num: 32
I (472) wifi:Init dynamic tx buffer num: 32
I (476) wifi:Init static tx FG buffer num: 2
I (480) wifi:Init static rx buffer size: 1600
I (484) wifi:Init static rx buffer num: 10
I (488) wifi:Init dynamic rx buffer num: 32
␛[0;32mI (493) wifi_init: rx ba win: 6␛[0m
␛[0;32mI (496) wifi_init: accept mbox: 6␛[0m
␛[0;32mI (500) wifi_init: tcpip mbox: 32␛[0m
␛[0;32mI (504) wifi_init: udp mbox: 6␛[0m
␛[0;32mI (507) wifi_init: tcp mbox: 6␛[0m
␛[0;32mI (511) wifi_init: tcp tx win: 5760␛[0m
␛[0;32mI (515) wifi_init: tcp rx win: 5760␛[0m
␛[0;32mI (519) wifi_init: tcp mss: 1440␛[0m
␛[0;32mI (523) wifi_init: WiFi IRAM OP enabled␛[0m
␛[0;32mI (528) wifi_init: WiFi RX IRAM OP enabled␛[0m
␛[0;32mI (534) phy_init: phy_version 680,a6008b2,Jun  4 2024,16:41:10␛[0m
I (579) wifi:mode : sta (a0:f2:62:f4:56:84)
I (579) wifi:enable tsf
␛[0;32mI (581) wifi: connecting to SSID 'bananas'...␛[0m
I (2993) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
I (2994) wifi:state: init -> auth (0xb0)
I (3004) wifi:state: auth -> assoc (0x0)
I (3012) wifi:state: assoc -> run (0x10)
I (3027) wifi:connected with bananas, aid = 8, channel 6, BW20, bssid = b0:e4:d5:6a:f3:8d
I (3028) wifi:security: WPA2-PSK, phy: bgn, rssi: -49
I (3032) wifi:pm start, type: 1

I (3032) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (3040) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
I (3049) wifi:dp: 2, bi: 102400, li: 4, scale listen interval from 307200 us to 409600 us
I (3056) wifi:AP's beacon interval = 102400 us, DTIM period = 2
I (3068) wifi:<ba-add>idx:0 (ifx:0, b0:e4:d5:6a:f3:8d), tid:6, ssn:2, winSize:64
␛[0;32mI (4064) esp_netif_handlers: sta ip: 192.168.86.56, mask: 255.255.255.0, gw: 192.168.86.1␛[0m
␛[0;32mI (4065) wifi: got IP 192.168.86.56␛[0m
␛[0;32mI (4069) device_iot: up — backend 'https', firmware v2.0.0␛[0m
␛[0;32mI (4073) app: running on 'https' backend, v2.0.0 (vGOOD)␛[0m
I (4074) wifi:<ba-add>idx:1 (ifx:0, b0:e4:d5:6a:f3:8d), tid:0, ssn:0, winSize:64
␛[0;32mI (5300) coreMQTT: MQTT connection established with the broker.␛[0m
␛[0;32mI (5301) mqtt: MQTT connected to AWS IoT Core as 'esp32-ota-poc-02'␛[0m
␛[0;32mI (5312) mqtt: subscribed: dt/esp32-ota-poc-02/cmd␛[0m
␛[0;32mI (5313) ota: (re)subscribing to job topics␛[0m
␛[0;32mI (5321) mqtt: subscribed: $aws/things/esp32-ota-poc-02/jobs/start-next/accepted␛[0m
␛[0;32mI (5322) ota: requesting job document␛[0m
␛[0;32mI (5508) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.␛[0m
␛[0;32mI (5509) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (5524) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (5525) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (34341) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (34342) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (64306) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (64307) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
Disconnected (read failed: [Errno 6] Device not configured)
Reconnecting to /dev/cu.usbmodem211301 .	 Connected!
I (3033) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
I (3035) wifi:state: init -> auth (0xb0)
I (3049) wifi:state: auth -> assoc (0x0)
I (3056) wifi:state: assoc -> run (0x10)
I (3069) wifi:connected with bananas, aid = 8, channel 6, BW20, bssid = b0:e4:d5:6a:f3:8d
I (3070) wifi:security: WPA2-PSK, phy: bgn, rssi: -53
I (3074) wifi:pm start, type: 1

I (3074) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (3082) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
I (3097) wifi:<ba-add>idx:0 (ifx:0, b0:e4:d5:6a:f3:8d), tid:6, ssn:2, winSize:64
I (3105) wifi:dp: 2, bi: 102400, li: 4, scale listen interval from 307200 us to 409600 us
I (3105) wifi:AP's beacon interval = 102400 us, DTIM period = 2
I (3325) wifi:<ba-add>idx:1 (ifx:0, b0:e4:d5:6a:f3:8d), tid:0, ssn:0, winSize:64
␛[0;32mI (4092) esp_netif_handlers: sta ip: 192.168.86.56, mask: 255.255.255.0, gw: 192.168.86.1␛[0m
␛[0;32mI (4093) wifi: got IP 192.168.86.56␛[0m
␛[0;32mI (4097) device_iot: up — backend 'https', firmware v2.0.0␛[0m
␛[0;32mI (4102) app: running on 'https' backend, v2.0.0 (vGOOD)␛[0m
␛[0;32mI (5478) coreMQTT: MQTT connection established with the broker.␛[0m
␛[0;32mI (5479) mqtt: MQTT connected to AWS IoT Core as 'esp32-ota-poc-02'␛[0m
␛[0;32mI (5490) mqtt: subscribed: dt/esp32-ota-poc-02/cmd␛[0m
␛[0;32mI (5490) ota: (re)subscribing to job topics␛[0m
␛[0;32mI (5497) mqtt: subscribed: $aws/things/esp32-ota-poc-02/jobs/start-next/accepted␛[0m
␛[0;32mI (5499) ota: requesting job document␛[0m
␛[0;32mI (5806) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (5807) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (5859) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.␛[0m
␛[0;32mI (5860) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (5889) AWS_OTA: otaPal_SetPlatformImageState, 2␛[0m
␛[0;32mI (5889) AWS_OTA: Set image as valid one!␛[0m
␛[0;32mI (5890) esp_ota_ops: aws_esp_ota_get_boot_flags: 1␛[0m
␛[0;32mI (5894) esp_ota_ops: [1] aflags/seq:0x2/0x3, pflags/seq:0x3/0x0␛[0m
␛[0;33mW (5901) AWS_OTA: Image not in self test mode 2␛[0m
␛[0;32mI (5906) esp_ota_ops: aws_esp_ota_get_boot_flags: 1␛[0m
␛[0;32mI (5911) esp_ota_ops: [1] aflags/seq:0x2/0x3, pflags/seq:0x3/0x0␛[0m
␛[0;32mI (5918) AWS_OTA: Writing to partition subtype 17 at offset 0x240000␛[0m
␛[0;32mI (7344) AWS_OTA: esp_ota_begin succeeded␛[0m
␛[0;32mI (7344) ota: OTA job accepted (HTTP data path), 987232 bytes␛[0m
␛[0;32mI (7345) ota: reporting job AFR_OTA-esp32-ota-https-3-0-0-1782887946 -> IN_PROGRESS␛[0m
␛[0;32mI (7523) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.␛[0m
␛[0;32mI (7524) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (7854) ota: downloading ld bytes over HTTPS␛[0m
␛[0;32mI (8450) ota: downloaded 102400 / 987232 bytes (10%)␛[0m
␛[0;32mI (9011) ota: downloaded 200704 / 987232 bytes (20%)␛[0m
␛[0;32mI (9616) ota: downloaded 299008 / 987232 bytes (30%)␛[0m
␛[0;32mI (10117) ota: downloaded 397312 / 987232 bytes (40%)␛[0m
␛[0;32mI (10666) ota: downloaded 495616 / 987232 bytes (50%)␛[0m
␛[0;32mI (11247) ota: downloaded 593920 / 987232 bytes (60%)␛[0m
␛[0;32mI (11761) ota: downloaded 692224 / 987232 bytes (70%)␛[0m
␛[0;32mI (12340) ota: downloaded 790528 / 987232 bytes (80%)␛[0m
␛[0;32mI (12931) ota: downloaded 888832 / 987232 bytes (90%)␛[0m
␛[0;32mI (13411) ota: downloaded 987232 / 987232 bytes (100%)␛[0m
␛[0;32mI (13414) ota: download complete (987232 bytes)␛[0m
␛[0;32mI (13415) ota: verifying signature + closing file␛[0m
␛[0;32mI (13719) AWS_OTA: Signature verification succeeded.␛[0m
␛[0;32mI (13720) ota: ␛[1;32msignature OK -> activating + rebooting␛[0m␛[0m
␛[0;32mI (13722) esp_image: segment 0: paddr=00240020 vaddr=3c0b0020 size=32af4h (207604) map␛[0m
␛[0;32mI (13758) esp_image: segment 1: paddr=00272b1c vaddr=3fc99700 size=04fcch ( 20428) ␛[0m
␛[0;32mI (13762) esp_image: segment 2: paddr=00277af0 vaddr=40374000 size=08528h ( 34088) ␛[0m
␛[0;32mI (13769) esp_image: segment 3: paddr=00280020 vaddr=42000020 size=a3ee4h (671460) map␛[0m
␛[0;32mI (13863) esp_image: segment 4: paddr=00323f0c vaddr=4037c528 size=0d124h ( 53540) ␛[0m
␛[0;32mI (13871) esp_image: segment 0: paddr=00240020 vaddr=3c0b0020 size=32af4h (207604) map␛[0m
␛[0;32mI (13900) esp_image: segment 1: paddr=00272b1c vaddr=3fc99700 size=04fcch ( 20428) ␛[0m
␛[0;32mI (13904) esp_image: segment 2: paddr=00277af0 vaddr=40374000 size=08528h ( 34088) ␛[0m
␛[0;32mI (13911) esp_image: segment 3: paddr=00280020 vaddr=42000020 size=a3ee4h (671460) map␛[0m
␛[0;32mI (14005) esp_image: segment 4: paddr=00323f0c vaddr=4037c528 size=0d124h ( 53540) ␛[0m
I (14530) wifi:state: run -> init (0x0)
I (14533) wifi:pm stop, total sleep time: lu us / lu us

I (14533) wifi:<ba-del>idx:1, tid:0
I (14534) wifi:<ba-del>idx:0, tid:6
I (14535) wifi:new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
␛[0;31mE (14545) network_transport: Error reading the message␛[0m
␛[0;31mE (14548) coreMQTT: Call to receiveSingleIteration failed. Status=MQTTRecvFailed␛[0m
␛[0;31mE (14556) mqtt: process loop error: MQTTRecvFailed -> reconnecting␛[0m
␛[0;33mW (14564) app: offline␛[0m
␛[0;31mE (14567) esp-tls: [sock=54] connect() error: Host is unreachable␛[0m
I (14573) wifi:flush txq
␛[0;31mE (14573) esp-tls: Failed to open new connection␛[0m
I (14575) wifi:stop sw txq
␛[0;31mE (14580) mqtt: TLS connect to a2fm5sbtk65rj6-ats.iot.us-east-1.amazonaws.com:8883 failed␛[0m
I (14583) wifi:lmac stop hw txq
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0xc (RTC_SW_CPU_RST),boot:0x28 (SPI_FAST_FLASH_BOOT)
Saved PC:0x4037acca
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce2810,len:0xfb4
load:0x403c8700,len:0x4
load:0x403c8704,len:0xad8
load:0x403cb700,len:0x2ce0
entry 0x403c8888
␛[0;32mI (236) cpu_start: Multicore app␛[0m
␛[0;32mI (245) cpu_start: Pro cpu start user code␛[0m
␛[0;32mI (245) cpu_start: cpu freq: 160000000 Hz␛[0m
␛[0;32mI (246) app_init: Application information:␛[0m
␛[0;32mI (248) app_init: Project name:     esp_ota␛[0m
␛[0;32mI (253) app_init: App version:      1.0.0␛[0m
␛[0;32mI (258) app_init: Compile time:     Jul  1 2026 01:56:13␛[0m
␛[0;32mI (264) app_init: ELF file SHA256:  3290495eb...␛[0m
␛[0;32mI (269) app_init: ESP-IDF:          5.3.1␛[0m
␛[0;32mI (274) efuse_init: Min chip rev:     v0.0␛[0m
␛[0;32mI (279) efuse_init: Max chip rev:     v0.99 ␛[0m
␛[0;32mI (283) efuse_init: Chip rev:         v0.2␛[0m
␛[0;32mI (288) heap_init: Initializing. RAM available for dynamic allocation:␛[0m
␛[0;32mI (296) heap_init: At 3FCA7A00 len 00041D10 (263 KiB): RAM␛[0m
␛[0;32mI (302) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM␛[0m
␛[0;32mI (308) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM␛[0m
␛[0;32mI (314) heap_init: At 600FE100 len 00001EE8 (7 KiB): RTCRAM␛[0m
␛[0;32mI (321) spi_flash: detected chip: generic␛[0m
␛[0;32mI (325) spi_flash: flash io: dio␛[0m
␛[0;32mI (329) sleep: Configure to isolate all GPIO pins in sleep state␛[0m
␛[0;32mI (336) sleep: Enable automatic switching of GPIO sleep configuration␛[0m
␛[0;32mI (343) esp_core_dump_flash: Init core dump to flash␛[0m
␛[0;32mI (349) esp_core_dump_flash: Found partition 'coredump' @ 3e2000 65536 bytes␛[0m
D (356) esp_core_dump_flash: Blank core dump partition!␛[0m
␛[0;32mI (362) main_task: Started on CPU0␛[0m
␛[0;32mI (366) main_task: Calling app_main()␛[0m
␛[0;32mI (370) gpio: GPIO[48]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 ␛[0m
␛[0;32mI (383) device_iot: esp_secure_cert: Thing name from cert CN = 'esp32-ota-poc-02'␛[0m
␛[0;32mI (388) device_iot: esp_secure_cert: device key plaintext in partition␛[0m
␛[0;32mI (418) self_test: ----------------------------------------------------------␛[0m
␛[0;32mI (419) self_test:  firmware v3.0.0   variant=vGOOD␛[0m
␛[0;32mI (421) self_test:  running partition: ota_1 @ 0x240000  (ota state 1)␛[0m
␛[0;32mI (428) self_test:  reset reason: 3   free heap: 299024␛[0m
␛[0;32mI (434) self_test: ----------------------------------------------------------␛[0m
␛[0;33mW (442) self_test: self-test watchdog armed (180000 ms)␛[0m
␛[0;32mI (449) pp: pp rom version: e7ae62f␛[0m
␛[0;32mI (452) net80211: net80211 rom version: e7ae62f␛[0m
I (458) wifi:wifi driver task: 3fcb2ec8, prio:23, stack:6144, core=0
I (476) wifi:wifi firmware version: ccaebfa
I (477) wifi:wifi certification version: v7.0
I (477) wifi:config NVS flash: enabled
I (477) wifi:config nano formating: enabled
I (481) wifi:Init data frame dynamic rx buffer num: 32
I (486) wifi:Init static rx mgmt buffer num: 5
I (490) wifi:Init management short buffer num: 32
I (494) wifi:Init dynamic tx buffer num: 32
I (498) wifi:Init static tx FG buffer num: 2
I (502) wifi:Init static rx buffer size: 1600
I (507) wifi:Init static rx buffer num: 10
I (510) wifi:Init dynamic rx buffer num: 32
␛[0;32mI (515) wifi_init: rx ba win: 6␛[0m
␛[0;32mI (518) wifi_init: accept mbox: 6␛[0m
␛[0;32mI (522) wifi_init: tcpip mbox: 32␛[0m
␛[0;32mI (526) wifi_init: udp mbox: 6␛[0m
␛[0;32mI (530) wifi_init: tcp mbox: 6␛[0m
␛[0;32mI (533) wifi_init: tcp tx win: 5760␛[0m
␛[0;32mI (538) wifi_init: tcp rx win: 5760␛[0m
␛[0;32mI (542) wifi_init: tcp mss: 1440␛[0m
␛[0;32mI (546) wifi_init: WiFi IRAM OP enabled␛[0m
␛[0;32mI (550) wifi_init: WiFi RX IRAM OP enabled␛[0m
␛[0;32mI (557) phy_init: phy_version 680,a6008b2,Jun  4 2024,16:41:10␛[0m
I (608) wifi:mode : sta (a0:f2:62:f4:56:84)
I (609) wifi:enable tsf
␛[0;32mI (610) wifi: connecting to SSID 'bananas'...␛[0m
I (3022) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1, snd_ch_cfg:0x0
I (3023) wifi:state: init -> auth (0xb0)
I (3032) wifi:state: auth -> assoc (0x0)
I (3043) wifi:state: assoc -> run (0x10)
I (3060) wifi:connected with bananas, aid = 8, channel 6, BW20, bssid = b0:e4:d5:6a:f3:8d
I (3061) wifi:security: WPA2-PSK, phy: bgn, rssi: -50
I (3065) wifi:pm start, type: 1

I (3065) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (3073) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
I (3086) wifi:<ba-add>idx:0 (ifx:0, b0:e4:d5:6a:f3:8d), tid:6, ssn:2, winSize:64
I (3142) wifi:dp: 2, bi: 102400, li: 4, scale listen interval from 307200 us to 409600 us
I (3142) wifi:AP's beacon interval = 102400 us, DTIM period = 2
␛[0;32mI (4083) esp_netif_handlers: sta ip: 192.168.86.56, mask: 255.255.255.0, gw: 192.168.86.1␛[0m
␛[0;32mI (4084) wifi: got IP 192.168.86.56␛[0m
I (4090) wifi:<ba-add>idx:1 (ifx:0, b0:e4:d5:6a:f3:8d), tid:0, ssn:0, winSize:64
␛[0;32mI (5397) coreMQTT: MQTT connection established with the broker.␛[0m
␛[0;32mI (5398) mqtt: MQTT connected to AWS IoT Core as 'esp32-ota-poc-02'␛[0m
␛[0;33mW (5489) ota: trial boot detected — running self-test␛[0m
␛[0;32mI (5489) self_test: core-function check: free heap = 204488␛[0m
␛[0;32mI (5490) self_test: app health check -> PASS␛[0m
␛[0;32mI (5514) self_test: ␛[1;32mimage COMMITTED (rollback cancelled)␛[0m␛[0m
␛[0;32mI (5515) self_test: self-test watchdog disarmed␛[0m
␛[0;32mI (5516) ota: reporting job AFR_OTA-esp32-ota-https-3-0-0-1782887946 -> SUCCEEDED␛[0m
␛[0;32mI (5562) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (5563) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (5745) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.␛[0m
␛[0;32mI (5745) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (10525) ota: (re)subscribing to job topics␛[0m
␛[0;32mI (10535) mqtt: subscribed: $aws/things/esp32-ota-poc-02/jobs/start-next/accepted␛[0m
␛[0;32mI (10536) ota: requesting job document␛[0m
␛[0;32mI (10536) device_iot: up — backend 'https', firmware v3.0.0␛[0m
␛[0;32mI (10650) mqtt: subscribed: dt/esp32-ota-poc-02/cmd␛[0m
␛[0;32mI (10651) app: running on 'https' backend, v3.0.0 (vGOOD)␛[0m
␛[0;32mI (10868) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (10870) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (10883) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.␛[0m
␛[0;32mI (10884) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (40948) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (40949) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (70752) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (70752) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (100788) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (100789) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (130852) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (130853) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (160759) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (160760) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (190777) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (190778) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (220794) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
␛[0;32mI (220794) coreMQTT: State record updated. New state=MQTTPublishDone.␛[0m
␛[0;32mI (250765) coreMQTT: Ack packet deserialized with result: MQTTSuccess.␛[0m
```
