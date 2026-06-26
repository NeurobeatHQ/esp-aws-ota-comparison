# Generate a .c embedding the PEM cert/key files as NUL-terminated byte arrays.
# Variant B has NO on-device code-signature check, so there is no code-signing
# cert here (cf. ../esp/embed_certs.cmake). AmazonRootCA1 is the trust anchor for
# BOTH the AWS IoT MQTT connection AND the S3 firmware download (S3 chains to it).
#
# Inputs (passed with -D): OUT (output .c path), CERT_DIR (the certs directory).

function(emit_array name file)
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "embed_certs: missing ${file}")
    endif()
    file(READ "${file}" hex HEX)
    string(REGEX MATCHALL ".." bytes "${hex}")
    list(JOIN bytes ",0x" body)
    file(APPEND "${OUT}" "const unsigned char ${name}[] = {0x${body},0x00};\n")
    file(APPEND "${OUT}" "const unsigned int ${name}_len = sizeof(${name});\n\n")
endfunction()

file(WRITE "${OUT}" "/* AUTO-GENERATED from certs/ by embed_certs.cmake — do not edit. */\n\n")
emit_array(aws_root_ca_pem "${CERT_DIR}/AmazonRootCA1.pem")
emit_array(device_cert_pem "${CERT_DIR}/client.crt")
emit_array(device_key_pem  "${CERT_DIR}/client.key")
