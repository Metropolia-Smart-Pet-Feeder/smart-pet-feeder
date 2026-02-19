idf_component_register(
    SRCS "ui_test_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES ui touch_display event_bus peripherals common
)
