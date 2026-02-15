#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "StepperMotor.h"
#include "board_config.h"

static const char* TAG = "stepper_motor_test";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Stepper Motor Test Started");

    StepperMotor::Config motor_config = {
        .step_pin = BoardConfig::MOTOR_STEP,
        .dir_pin = BoardConfig::MOTOR_DIR,
        .steps_per_rev = 400,   // roughly 400 steps per revolution (half step)
        .rmt_resolution_hz = 1000000,   // 1MHz RMT clock resolution
    };

    StepperMotor motor(motor_config);

    // init
    esp_err_t ret = motor.init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize motor: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Motor initialized successfully");

    const uint32_t TEST_RPM = 10;
    const uint32_t STEPS_PER_TEST = 400;
    const uint8_t DIR_CLOCKWISE = 1;
    const uint8_t DIR_COUNTERCLOCKWISE = 0;

    ESP_LOGI(TAG, "Starting motor test. RPM: %lu", TEST_RPM);

    // test loop 5 times
    for (int i = 1; i <= 5; i++) {
        ESP_LOGI(TAG, "Test cycle %d/5", i);

        // 1. turn
        ESP_LOGI(TAG, "Rotating clockwise (%lu steps)...", STEPS_PER_TEST);
        ret = motor.move(STEPS_PER_TEST, DIR_CLOCKWISE, TEST_RPM);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Clockwise rotation failed: %s", esp_err_to_name(ret));
            break;
        }
        ESP_LOGI(TAG, "Clockwise rotation complete");

        // 2. pause
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 3. turn the other way
        ESP_LOGI(TAG, "Rotating counter-clockwise (%lu steps)...", STEPS_PER_TEST);
        ret = motor.move(STEPS_PER_TEST, DIR_COUNTERCLOCKWISE, TEST_RPM);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Counter-clockwise rotation failed: %s", esp_err_to_name(ret));
            break;
        }
        ESP_LOGI(TAG, "Counter-clockwise rotation complete");

        // 4. pause
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    ESP_LOGI(TAG, "Test finished");
}
