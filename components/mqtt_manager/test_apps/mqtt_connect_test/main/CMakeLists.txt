idf_component_register(
    SRCS "mqtt_connect_test_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES mqtt_manager event_bus wifi_manager nvs_flash
)
