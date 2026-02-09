idf_component_register(SRCS "${CMAKE_SOURCE_DIR}/components/hx711/src/HX711.c"
        INCLUDE_DIRS "include/"
        REQUIRES driver
)