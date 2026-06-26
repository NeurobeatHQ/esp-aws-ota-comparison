# Root CMake — pure ESP-IDF project (driven by PlatformIO `framework = espidf`).
cmake_minimum_required(VERSION 3.16)

# esp-aws-iot is vendored as a git submodule under components/esp-aws-iot.
# Each library under libraries/<X> is its own IDF component (has a CMakeLists.txt
# that calls idf_component_register), so we add them to the component search path.
# Use list(APPEND ...) — PlatformIO injects its own dirs into EXTRA_COMPONENT_DIRS.
set(ESP_AWS_IOT "${CMAKE_CURRENT_LIST_DIR}/components/esp-aws-iot/libraries")
list(APPEND EXTRA_COMPONENT_DIRS
    "${ESP_AWS_IOT}/coreMQTT"                                 # + esp-tls network_transport port
    "${ESP_AWS_IOT}/coreJSON"
    "${ESP_AWS_IOT}/backoffAlgorithm"
    "${ESP_AWS_IOT}/Jobs-for-AWS-IoT-embedded-sdk"            # jobs + OTA job-doc parser
    "${ESP_AWS_IOT}/aws-iot-core-mqtt-file-streams-embedded-c" # MQTTFileDownloader + ESP32 OTA PAL
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32s3-aws-ota)
