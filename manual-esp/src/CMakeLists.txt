# The "main" component — variant A (manual-esp). No AWS libraries.
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
        "mqtt_es.c"            # esp-mqtt mutual-TLS wrapper (shared with jobs-esp)
        "ota_download.c"       # esp_https_ota self-download (shared with jobs-esp)
        "control_protocol.c"   # hand-rolled MQTT control plane (variant A)
        "${EMBEDDED_CERTS_C}"
    INCLUDE_DIRS
        "."
    REQUIRES
        # IDF-native only — NO coreJSON, NO Jobs lib, NO coreMQTT/File Streams:
        json                  # cJSON
        mqtt                  # esp-mqtt
        esp_https_ota
        esp_http_client
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
