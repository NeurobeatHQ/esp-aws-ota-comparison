# The "main" component.
#
# Certs/key are embedded as a compiled C byte array generated from src/certs/*
# (see ../embed_certs.cmake). Portable across PlatformIO and idf.py — the
# objcopy/EMBED_TXTFILES paths are unreliable under PlatformIO's espidf builder.
#
# Generated at configure time with execute_process (add_custom_command is not
# allowed in IDF's requirements-extraction script pass). Guarded out of that
# pass; CMAKE_CONFIGURE_DEPENDS retriggers it when a cert file changes.
set(EMBEDDED_CERTS_C "${CMAKE_CURRENT_BINARY_DIR}/embedded_certs.c")
if(NOT CMAKE_SCRIPT_MODE_FILE)
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DOUT=${EMBEDDED_CERTS_C}"
        "-DCERT_DIR=${CMAKE_CURRENT_LIST_DIR}/certs"
        -P "${CMAKE_CURRENT_LIST_DIR}/../embed_certs.cmake")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/certs/AmazonRootCA1.pem"
        "${CMAKE_CURRENT_LIST_DIR}/certs/client.crt"
        "${CMAKE_CURRENT_LIST_DIR}/certs/client.key"
        "${CMAKE_CURRENT_LIST_DIR}/certs/aws_codesign.crt")
endif()

idf_component_register(
    SRCS
        "app_main.c"
        "wifi.c"
        "mqtt_client.c"
        "self_test.c"
        "ota_orchestrator.c"
        "${EMBEDDED_CERTS_C}"
    INCLUDE_DIRS
        "."
    REQUIRES
        # esp-aws-iot components (see ../CMakeLists.txt EXTRA_COMPONENT_DIRS):
        coreMQTT
        coreJSON
        backoffAlgorithm
        Jobs-for-AWS-IoT-embedded-sdk
        aws-iot-core-mqtt-file-streams-embedded-c
        # ESP-IDF components used directly by app code:
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        esp-tls
        app_update
        esp_partition
        esp_timer
        esp_system
        driver
)

# NOTE: certificates are embedded as the generated ${EMBEDDED_CERTS_C} above
# (../embed_certs.cmake), not via objcopy/EMBED_TXTFILES — those are unreliable
# under PlatformIO's espidf builder. Array symbols: see mqtt_client.c.
