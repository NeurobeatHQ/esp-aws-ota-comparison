# The "main" component — variant B.
#
# Certs embedded as a generated C byte array (see ../embed_certs.cmake), at
# configure time (add_custom_command is not allowed in IDF's requirements pass).
set(EMBEDDED_CERTS_C "${CMAKE_CURRENT_BINARY_DIR}/embedded_certs.c")
if(NOT CMAKE_SCRIPT_MODE_FILE)
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DOUT=${EMBEDDED_CERTS_C}"
        "-DCERT_DIR=${CMAKE_CURRENT_LIST_DIR}/certs"
        -P "${CMAKE_CURRENT_LIST_DIR}/../embed_certs.cmake")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_LIST_DIR}/certs/AmazonRootCA1.pem"
        "${CMAKE_CURRENT_LIST_DIR}/certs/client.crt"
        "${CMAKE_CURRENT_LIST_DIR}/certs/client.key")
endif()

idf_component_register(
    SRCS
        "app_main.c"
        "wifi.c"
        "self_test.c"
        "mqtt_es.c"            # esp-mqtt mutual-TLS wrapper (shared with manual-esp)
        "ota_download.c"       # esp_https_ota self-download (shared with manual-esp)
        "jobs_orchestrator.c"  # AWS IoT Jobs control plane (variant B)
        "${EMBEDDED_CERTS_C}"
    INCLUDE_DIRS
        "."
    REQUIRES
        # AWS Jobs lib + coreJSON (from ../esp/components via EXTRA_COMPONENT_DIRS):
        coreJSON
        Jobs-for-AWS-IoT-embedded-sdk
        # IDF-native stack (replaces coreMQTT + File Streams + PAL):
        json                  # cJSON — parse the custom job document
        mqtt                  # esp-mqtt
        esp_https_ota         # firmware self-download (does esp_ota_* internally)
        esp_http_client
        # IDF basics:
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
