# Process for certificates

## Make CA

Once — make your device CA (keep deviceCA.key secret; gitignore it):

```
openssl genrsa -out deviceCA.key 2048
openssl req -x509 -new -nodes -key deviceCA.key -sha256 -days 3650 \
  -subj "/CN=esp32-ota-poc-deviceCA" -out deviceCA.crt
```

## For each board make certificates

Per board (THING = esp32-ota-poc-01, then -02, -03):

```
# Device key + cert, CN = the Thing name, signed by your CA

THING=esp32-ota-poc-01

openssl genrsa -out "$THING.key" 2048
openssl req -new -key "$THING.key" -subj "/CN=$THING" -out "$THING.csr"
openssl x509 -req -in "$THING.csr" -CA deviceCA.crt -CAkey deviceCA.key \
  -CAcreateserial -days 3650 -sha256 -out "$THING.crt"

```

## For each board wire in AWS

```
THING=esp32-ota-poc-01

CERT_ARN=$(aws iot register-certificate-without-ca \
  --certificate-pem "file://$THING.crt" --status ACTIVE \
  --query certificateArn --output text)

aws iot create-thing            --thing-name "$THING"
aws iot attach-policy           --policy-name esp32-ota-poc-01-policy --target "$CERT_ARN"
aws iot attach-thing-principal  --thing-name "$THING" --principal "$CERT_ARN"
aws iot add-thing-to-thing-group --thing-group-name esp32-ota-poc-01-group --thing-name "$THING"
```

Note: esp32-ota-poc-01-policy  and esp32-ota-poc-01-group are created by the AWS CDK - see `aws-iot/lib/ota-stack.ts`

## For each board copy cerst on device

```
THING=esp32-ota-poc-01

cp "$THING.crt" esp/src/certs/client.crt
cp "$THING.key" esp/src/certs/client.key
(cd esp && scripts/provision-secure-cert.sh -p <PORT>)   # + --ds to seal the key in the DS peripheral 

```

## Then for every board, you can flash with the same binary

```
cd esp && PLATFORMIO_BUILD_FLAGS="-DUSE_ESP_SECURE_CERT=1" pio run -e https
```

## Updates to all boards via group

```
BACKEND=https aws-iot/scripts/upload-firmware.sh <firmware.bin> 2.0.0
BACKEND=https aws-iot/scripts/push-update.sh -g esp32-ota-poc-01-group 2.0.0
```



