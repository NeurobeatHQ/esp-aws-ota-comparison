# Root CMake — unified, backend-swappable firmware (PlatformIO framework=espidf).
#
# The OTA backend is chosen by the build env (pio run -e mqtt|https|jobs|manual);
# select_backend.py exports OTA_BACKEND, read here as $ENV{OTA_BACKEND} (an env
# var, so it reaches both the main configure and IDF's requirements script pass).
# Only the selected backend's esp-aws-iot components are added to the build.
cmake_minimum_required(VERSION 3.16)

set(OTA_BACKEND "$ENV{OTA_BACKEND}")
if(NOT OTA_BACKEND)
    set(OTA_BACKEND "mqtt")
endif()

set(ESP_AWS_IOT "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot/libraries")
if(OTA_BACKEND STREQUAL "mqtt" OR OTA_BACKEND STREQUAL "https")
    list(APPEND EXTRA_COMPONENT_DIRS
        "${ESP_AWS_IOT}/coreMQTT"
        "${ESP_AWS_IOT}/coreJSON"
        "${ESP_AWS_IOT}/backoffAlgorithm"
        "${ESP_AWS_IOT}/Jobs-for-AWS-IoT-embedded-sdk"
        "${ESP_AWS_IOT}/aws-iot-core-mqtt-file-streams-embedded-c")
elseif(OTA_BACKEND STREQUAL "jobs")
    list(APPEND EXTRA_COMPONENT_DIRS
        "${ESP_AWS_IOT}/coreJSON"
        "${ESP_AWS_IOT}/Jobs-for-AWS-IoT-embedded-sdk")
endif()
# manual: IDF-native only — no esp-aws-iot components.

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_ota)
