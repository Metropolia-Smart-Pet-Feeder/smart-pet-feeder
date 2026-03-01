# Standalone test project for UI component
cmake_minimum_required(VERSION 3.16)

# Project root: smart-pet-feeder/
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)

set(SDKCONFIG_DEFAULTS "${PROJECT_ROOT}/sdkconfig.defaults")

configure_file(
        "${PROJECT_ROOT}/partitions.csv"
        "${CMAKE_CURRENT_SOURCE_DIR}/partitions.csv"
        COPYONLY
)

set(EXTRA_COMPONENT_DIRS
    ${PROJECT_ROOT}/components/ui
    ${PROJECT_ROOT}/components/touch_display
    ${PROJECT_ROOT}/components/event_bus
    ${PROJECT_ROOT}/components/peripherals
    ${PROJECT_ROOT}/components/common
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Use lv_conf.h from project root
idf_build_set_property(COMPILE_OPTIONS "-DLV_CONF_INCLUDE_SIMPLE" APPEND)
idf_build_set_property(INCLUDE_DIRECTORIES "${PROJECT_ROOT}" APPEND)

project(ui_test)
