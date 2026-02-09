idf_component_register(SRCS "main.cpp"
                        INCLUDE_DIRS "."
                        REQUIRES driver esp_timer hx711 esp_event nvs_flash
)
