/**
 * Bowl Scale Calibration Test
 *
 * Phase 1 — Calibration:
 *   1. Flash this app with an empty bowl on the scale.
 *   2. Keep still — tare runs automatically (3 s).
 *   3. Place a known weight on the bowl within 8 seconds.
 *   4. Read the logged SCALE_FACTOR and update board_config.h.
 *
 * Phase 2 — Event test:
 *   BowlScale starts with the calibrated factor.
 *   Remove weight from the bowl to trigger EVENT_FOOD_EATEN.
 */

#include "HX711.h"
#include "BowlScale.h"
#include "EventBus.h"
#include "Events.h"
#include "Types.h"
#include "board_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <memory>

static const char* TAG = "ScaleCalib";

static constexpr float KNOWN_WEIGHT_G = 218.0f;

static void on_food_eaten(void*, esp_event_base_t, int32_t, void* data)
{
    float amount = *static_cast<float*>(data);
    ESP_LOGI("FoodEaten", "EVENT_FOOD_EATEN fired! Amount: %.1f g", amount);
}

extern "C" void app_main()
{
    ESP_LOGI(TAG, "=== Phase 1: Calibration ===");
    ESP_LOGI(TAG, "Known weight: %.1f g", KNOWN_WEIGHT_G);

    HX711 hx711;
    hx711.init(BoardConfig::SCALE_DOUT, BoardConfig::SCALE_SCK, HX711::eGAIN_128);

    ESP_LOGI(TAG, "Remove everything from the scale. Taring in 3 seconds...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    float scale_factor = hx711.autoCalibrateDual(KNOWN_WEIGHT_G, 3000, 8000);

    ESP_LOGI(TAG, "----------------------------------------------------");
    ESP_LOGI(TAG, "  SCALE_FACTOR = %.4f", scale_factor);
    ESP_LOGI(TAG, "  Update BoardConfig::SCALE_FACTOR in board_config.h");
    ESP_LOGI(TAG, "----------------------------------------------------");

    if (scale_factor <= 0.0f) {
        ESP_LOGE(TAG, "Calibration failed (scale_factor=%.4f). Check wiring and try again.", scale_factor);
        return;
    }

    ESP_LOGI(TAG, "=== Phase 2: Event Test ===");
    ESP_LOGI(TAG, "Remove all weight — BowlScale will tare now.");

    auto event_bus = std::make_shared<EventBus>();
    event_bus->init();
    event_bus->subscribe(EVENT_FOOD_EATEN, on_food_eaten);

    auto bowl_scale = std::make_shared<BowlScale>(
        BoardConfig::SCALE_DOUT, BoardConfig::SCALE_SCK,
        event_bus, scale_factor, BoardConfig::SCALE_THRESHOLD_G
    );
    bowl_scale->start();

    ESP_LOGI(TAG, "Place weight on the bowl, then remove it to trigger EVENT_FOOD_EATEN.");
    ESP_LOGI(TAG, "(Live readings printed by BowlScale task)");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
