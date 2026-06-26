# Root CMake — variant B (jobs-esp). Pure ESP-IDF (PlatformIO framework=espidf).
cmake_minimum_required(VERSION 3.16)

# Reference ONLY the standalone Jobs lib + coreJSON from the shared esp-aws-iot
# submodule (single-sourced under ../esp/components — not duplicated). No
# coreMQTT, no MQTT File Streams, no cbor: B uses esp-mqtt + esp_https_ota.
set(ESP_AWS_IOT "${CMAKE_CURRENT_LIST_DIR}/../esp/components/esp-aws-iot/libraries")
list(APPEND EXTRA_COMPONENT_DIRS
    "${ESP_AWS_IOT}/coreJSON"                        # required by the Jobs lib
    "${ESP_AWS_IOT}/Jobs-for-AWS-IoT-embedded-sdk"   # MQTT-agnostic topic helpers
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(jobs_esp)
