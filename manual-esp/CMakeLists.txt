# Root CMake — variant A (manual-esp). Pure ESP-IDF (PlatformIO framework=espidf).
#
# Zero AWS libraries: no esp-aws-iot submodule reference at all. A uses only
# IDF-native components (esp-mqtt + esp_https_ota + cJSON) and a hand-rolled
# MQTT request/response control protocol. AWS IoT Core is used purely as the
# MQTT transport (mutual TLS).
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(manual_esp)
