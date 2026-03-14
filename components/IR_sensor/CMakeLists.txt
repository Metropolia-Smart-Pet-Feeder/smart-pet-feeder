idf_component_register(
    SRCS 
        "src/IR_sensors.cpp"
        "src/motion_detection.cpp"
        "src/food_container.cpp"

    INCLUDE_DIRS 
        "include"

    REQUIRES
        event_bus
        driver
        freertos
        peripherals
        image_receiver
)