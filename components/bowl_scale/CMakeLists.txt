idf_component_register(
    SRCS 
        "src/HX711.cpp"
        "src/BowlScale.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        event_bus common
)