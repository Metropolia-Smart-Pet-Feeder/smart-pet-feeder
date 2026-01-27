idf_component_register(
        SRCS
        "camera_ov2640.cpp"
        "cam_driver/dsi_init/example_dsi_init.c"
        "cam_driver/sensor_init/example_sensor_init.c"
        INCLUDE_DIRS
        "include"
        "cam_driver/dsi_init/include"
        "cam_driver/sensor_init/include"
        REQUIRES
        esp_mm esp_driver_isp esp_driver_cam esp_driver_i2c dsi_init sensor_init
        esp_http_server nvs_flash esp_wifi esp_netif esp32-camera
        esp_driver_jpeg esp_lcd esp_cam_sensor esp_lcd_ili9881c esp_lcd_ek79007
)
