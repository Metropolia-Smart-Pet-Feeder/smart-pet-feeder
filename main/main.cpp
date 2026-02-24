#include "esp_log.h"
extern "C" {
#include "HX711.h"
}
static const char *TAG = "HX711_TEST";


extern "C" void app_main() {
    HX711_init(GPIO_NUM_4, GPIO_NUM_5,eGAIN_128);

    // Perform dual-channel calibration
    // HX711_auto_calibrate_dual(215.0f, 3000, 8000);
    HX711_set_scale(24.53f); // Set scale based on previous calibration
    HX711_tareAB();

    vTaskDelay(pdMS_TO_TICKS(2000));

    // Test readings
    while(1)
    {
        float weight_A = HX711_get_units_channel_A(20);
        float weight_B = HX711_get_units_channel_B(20);
        float total = weight_A + weight_B;
        ESP_LOGI(TAG, "A=%.2f B=%.2f Total=%.2f g", weight_A, weight_B, total);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}