# Component test: MQTTManager connect, publish, subscribe
cmake_minimum_required(VERSION 3.16)

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)

set(EXTRA_COMPONENT_DIRS
    ${PROJECT_ROOT}/components/event_bus
    ${PROJECT_ROOT}/components/common
    ${PROJECT_ROOT}/components/dispense_control
    ${PROJECT_ROOT}/components/stepper_motor
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(dispense_control_test)