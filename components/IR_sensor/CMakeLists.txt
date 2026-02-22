idf_component_register(
    SRCS "src/IR_sensors.cpp"
    INCLUDE_DIRS "include"
    REQUIRES 
        event_bus
        driver 
        freertos
        peripherals
)