# Test: save hardcoded WiFi credentials to NVS
cmake_minimum_required(VERSION 3.16)

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)

set(SDKCONFIG_DEFAULTS "${PROJECT_ROOT}/sdkconfig.defaults")

configure_file(
        "${PROJECT_ROOT}/partitions.csv"
        "${CMAKE_CURRENT_SOURCE_DIR}/partitions.csv"
        COPYONLY
)

set(EXTRA_COMPONENT_DIRS
    ${PROJECT_ROOT}/components/wifi_manager
    ${PROJECT_ROOT}/components/event_bus
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(wifi_credential_test)
