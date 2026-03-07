//
// Created by zhiyong on 3/7/26.
//
/**
#include "esp_log.h"
#include "HX711.h"

static const char *TAG = "HX711_TEST";

static HX711 hx711;

extern "C" void app_main() {
    hx711.init(GPIO_NUM_4, GPIO_NUM_5, HX711::eGAIN_128);

    // Perform dual-channel calibration
    // hx711.autoCalibrateDual(215.0f, 3000, 8000);
    hx711.setScale(24.53f);
    hx711.tareAB();

    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        float weight_A = hx711.getUnitsChannelA(20);
        float weight_B = hx711.getUnitsChannelB(20);
        float total    = weight_A + weight_B;
        ESP_LOGI(TAG, "A=%.2f B=%.2f Total=%.2f g", weight_A, weight_B, total);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
**/