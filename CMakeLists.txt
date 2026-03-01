# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
# idf_build_set_property(MINIMAL_BUILD ON)

# Use lv_conf.h from project root
idf_build_set_property(COMPILE_OPTIONS "-DLV_CONF_INCLUDE_SIMPLE" APPEND)
idf_build_set_property(INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}" APPEND)

project(smart-pet-feeder)
