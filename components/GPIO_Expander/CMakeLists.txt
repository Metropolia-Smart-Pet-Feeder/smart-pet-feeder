idf_component_register(SRCS "${CMAKE_SOURCE_DIR}/components/GPIO_Expander/src/TCA9555.cpp"
                        INCLUDE_DIRS "include/"
                        REQUIRES driver esp_timer
)
