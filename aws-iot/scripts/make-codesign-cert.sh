#!/usr/bin/env bash
# Create the ECDSA P-256 code-signing certificate, import it into ACM, and
# create the AWS Signer signing profile (platform AmazonFreeRTOS-Default).
#
# This is an ACTION (it generates a private key + imports to ACM), which is why
# it lives here and not in CDK — and CloudFormation can't create an
# AmazonFreeRTOS-Default Signer profile anyway.
#
#   scripts/make-codesign-cert.sh
#
# Outputs the Signer profile and copies the PUBLIC cert to the firmware so the
# OTA PAL can verify signatures.
set -euo pipefail
cd "$(dirname "$0")"
. ./_lib.sh

if ! backend_uses_signer; then
  log "backend '$BACKEND' has no on-device code-signing (authenticity = TLS + Secure Boot); nothing to do."
  exit 0
fi

WORK="codesign"
mkdir -p "$WORK"
KEY="$WORK/ecdsasigner.key"
CRT="$WORK/ecdsasigner.crt"
FW_CERT="../../esp/src/certs/aws_codesign.crt"

if [ ! -f "$CRT" ]; then
  log "generating ECDSA P-256 code-signing certificate"
  cat > "$WORK/cert_config.txt" <<'EOF'
[req]
prompt             = no
distinguished_name = dn
x509_extensions    = v3
[dn]
CN = ESP32 OTA POC Code Signer
O  = esp32-ota-poc
[v3]
keyUsage         = critical, digitalSignature
extendedKeyUsage = critical, codeSigning
EOF
  openssl ecparam -name prime256v1 -genkey -noout -out "$KEY"
  openssl req -new -x509 -days 365 -key "$KEY" -config "$WORK/cert_config.txt" -out "$CRT"
else
  log "reusing existing $CRT"
fi

log "importing certificate into ACM"
CERT_ARN="$(aws acm import-certificate \
  --certificate "fileb://$CRT" \
  --private-key "fileb://$KEY" \
  --query CertificateArn --output text)"
log "ACM cert: $CERT_ARN"

log "creating Signer profile '$SIGNING_PROFILE_NAME' (AmazonFreeRTOS-Default)"
aws signer put-signing-profile \
  --profile-name "$SIGNING_PROFILE_NAME" \
  --signing-material "certificateArn=$CERT_ARN" \
  --platform AmazonFreeRTOS-Default \
  --signing-parameters "certname=/firmware/cert.pem" >/dev/null

# The OTA PAL verifies image signatures against this PUBLIC cert.
cp "$CRT" "$FW_CERT"
log "copied public code-signing cert -> $FW_CERT"
log "REBUILD the firmware so it embeds this cert before flashing."
echo
echo "  SIGNING_PROFILE_NAME=$SIGNING_PROFILE_NAME"
echo "  CODE_SIGNING_CERT_ARN=$CERT_ARN"
