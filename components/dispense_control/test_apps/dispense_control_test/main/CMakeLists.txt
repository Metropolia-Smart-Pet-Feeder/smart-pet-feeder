idf_component_register(
    SRCS "dispense_control_test_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES event_bus nvs_flash common dispense_control stepper_motor board_config
)
