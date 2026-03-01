idf_component_register(
    SRCS "src/MQTTManager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES mqtt event_bus common json
)
