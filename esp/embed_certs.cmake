# Generate a .c file embedding the PEM cert/key files as NUL-terminated byte
# arrays. Portable (no objcopy / EMBED_TXTFILES) so it works the same under
# PlatformIO and idf.py. Invoked by src/CMakeLists.txt via add_custom_command.
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
# The device cert + key are NOT embedded — they live in the esp_secure_cert partition
# (provisioned per board). Only the public root CA (server trust) and the code-signing
# cert (OTA image verify) are baked into the image.
emit_array(aws_root_ca_pem   "${CERT_DIR}/AmazonRootCA1.pem")
emit_array(codesign_cert_pem "${CERT_DIR}/aws_codesign.crt")
