idf_component_register(
    SRCS "bowl_scale_calibration_test_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES common bowl_scale event_bus driver
)
