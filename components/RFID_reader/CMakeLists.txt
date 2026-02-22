idf_component_register(
    SRCS "src/RC522.cpp"
    INCLUDE_DIRS "include"
    REQUIRES 
        event_bus
        driver 
        freertos
        peripherals

)