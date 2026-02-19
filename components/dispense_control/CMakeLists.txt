idf_component_register(
    SRCS "src/DispenseControl.cpp"
    INCLUDE_DIRS "include"
    REQUIRES event_bus stepper_motor nvs_flash common
)
