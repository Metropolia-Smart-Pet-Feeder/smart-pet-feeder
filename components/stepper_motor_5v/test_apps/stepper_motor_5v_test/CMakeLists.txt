# StepperMotor5V direct motor test
cmake_minimum_required(VERSION 3.16)

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../..)

set(SDKCONFIG_DEFAULTS "${PROJECT_ROOT}/sdkconfig.defaults")

configure_file(
        "${PROJECT_ROOT}/partitions.csv"
        "${CMAKE_CURRENT_SOURCE_DIR}/partitions.csv"
        COPYONLY
)

set(EXTRA_COMPONENT_DIRS
    ${PROJECT_ROOT}/components/common
    ${PROJECT_ROOT}/components/stepper_motor_5v
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(stepper_motor_5v_test)