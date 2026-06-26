#!/usr/bin/env bash
# Create + register the device certificate, attach the (CDK-created) policy and
# the Thing, and drop the cert/key into the firmware tree.
#
#   scripts/register-device.sh
#
# The AttachThingPrincipal step is the one people miss: without it the cert
# authenticates at TLS but is refused at MQTT CONNECT under a thing-scoped policy.
set -euo pipefail
cd "$(dirname "$0")"
. ./_lib.sh
load_stack

CERT_DIR="../../esp/src/certs"
OUT="device"
mkdir -p "$OUT"

log "creating device certificate + keys"
CREATE_JSON="$(aws iot create-keys-and-certificate --set-as-active)"
CERT_ARN="$(echo "$CREATE_JSON" | jq -r '.certificateArn')"
echo "$CREATE_JSON" | jq -r '.certificatePem'            > "$OUT/client.crt"
echo "$CREATE_JSON" | jq -r '.keyPair.PrivateKey'        > "$OUT/client.key"
echo "$CREATE_JSON" | jq -r '.keyPair.PublicKey'         > "$OUT/client.pub"
log "certificate: $CERT_ARN"

log "attaching policy '$POLICY_NAME' to the certificate"
aws iot attach-policy --policy-name "$POLICY_NAME" --target "$CERT_ARN"

log "attaching certificate to Thing '$THING_NAME'   (AttachThingPrincipal)"
aws iot attach-thing-principal --thing-name "$THING_NAME" --principal "$CERT_ARN"

# Hand the credentials to the firmware build.
cp "$OUT/client.crt" "$CERT_DIR/client.crt"
cp "$OUT/client.key" "$CERT_DIR/client.key"
log "copied client.crt + client.key -> $CERT_DIR"
log "set THING_NAME='$THING_NAME' in esp/src/secrets.h, then REBUILD + flash."
echo
echo "  CERT_ARN=$CERT_ARN"
