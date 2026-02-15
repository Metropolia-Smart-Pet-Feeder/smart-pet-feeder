# Component test: Motor
cmake_minimum_required(VERSION 3.16)

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)

set(EXTRA_COMPONENT_DIRS
    ${PROJECT_ROOT}/components/stepper_motor
    ${PROJECT_ROOT}/components/event_bus
    ${PROJECT_ROOT}/components/board_config
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(stepper_motor_test)
