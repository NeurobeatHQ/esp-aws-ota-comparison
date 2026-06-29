# The "main" component — unified, with a build-time backend selection.
#
# OTA_BACKEND (env var, set by ../select_backend.py from the build env) picks
# exactly one transport + orchestrator and the matching component REQUIRES.
set(OTA_BACKEND "$ENV{OTA_BACKEND}")
if(NOT OTA_BACKEND)
    set(OTA_BACKEND "mqtt")
endif()

# Common, backend-agnostic sources (the app + the facade + the shared helpers).
set(srcs
    "app_main.c"
    "device_iot.c"
    "wifi.c"
    "self_test.c"
)
set(reqs
    nvs_flash esp_wifi esp_event esp_netif esp-tls
    app_update esp_partition esp_timer esp_system driver
)

if(OTA_BACKEND STREQUAL "mqtt")
    list(APPEND srcs "transports/transport_coremqtt.c" "orchestrators/ota_filestreams.c")
    list(APPEND reqs coreMQTT coreJSON backoffAlgorithm
                     Jobs-for-AWS-IoT-embedded-sdk aws-iot-core-mqtt-file-streams-embedded-c)
elseif(OTA_BACKEND STREQUAL "https")
    list(APPEND srcs "transports/transport_coremqtt.c" "orchestrators/ota_http.c")
    list(APPEND reqs coreMQTT coreJSON backoffAlgorithm
                     Jobs-for-AWS-IoT-embedded-sdk aws-iot-core-mqtt-file-streams-embedded-c
                     esp_http_client)
elseif(OTA_BACKEND STREQUAL "jobs")
    list(APPEND srcs "transports/transport_espmqtt.c" "orchestrators/ota_jobs.c" "ota_download.c")
    list(APPEND reqs coreJSON Jobs-for-AWS-IoT-embedded-sdk json mqtt esp_https_ota esp_http_client)
elseif(OTA_BACKEND STREQUAL "manual")
    list(APPEND srcs "transports/transport_espmqtt.c" "orchestrators/ota_manual.c" "ota_download.c")
    list(APPEND reqs json mqtt esp_https_ota esp_http_client)
else()
    message(FATAL_ERROR "unknown OTA_BACKEND '${OTA_BACKEND}' (use mqtt|https|jobs|manual)")
endif()

# Certs embedded as a generated C byte array (../embed_certs.cmake), at configure
# time (add_custom_command is not allowed in IDF's requirements pass).
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
list(APPEND srcs "${EMBEDDED_CERTS_C}")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    REQUIRES ${reqs}
)

# Expose the backend name to the firmware (device_iot_backend_name()).
target_compile_definitions(${COMPONENT_LIB} PRIVATE "OTA_BACKEND_NAME=\"${OTA_BACKEND}\"")
