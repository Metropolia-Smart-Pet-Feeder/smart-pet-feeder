idf_component_register(
    SRCS "src/DispenseControl.cpp"
    INCLUDE_DIRS "include"
    REQUIRES event_bus stepper_motor_5v nvs_flash common
)
