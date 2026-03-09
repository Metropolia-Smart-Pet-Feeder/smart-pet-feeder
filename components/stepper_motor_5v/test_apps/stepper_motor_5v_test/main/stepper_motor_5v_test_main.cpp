/**
 * StepperMotor5V Direct Motor Test
 *
 * Phase 1 – Pin Diagnostic:
 *   Each GPIO pin is driven HIGH one at a time for 2 seconds.
 *   Watch which LED on the driver board lights up and note the log output
 *   to build the GPIO → driver-board-input mapping.
 *   If a pin stays dark, that GPIO cannot drive HIGH on this board and
 *   must be replaced with a working one.
 *
 * Phase 2 – Motor Spin:
 *   After the diagnostic, the motor is driven CW/CCW in a loop.
 */

#include "StepperMotor5V.h"
#include "board_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "5V_MotorTest";

// Step 1: single-pin diagnostic

static void pinDiagnostic()
{
    const gpio_num_t pins[4] = {
        BoardConfig::STEPPER_IN1,
        BoardConfig::STEPPER_IN2,
        BoardConfig::STEPPER_IN3,
        BoardConfig::STEPPER_IN4,
    };
    const char* labels[4] = {"IN1", "IN2", "IN3", "IN4"};

    ESP_LOGI(TAG, "=== Pin Diagnostic ===");
    ESP_LOGI(TAG, "Each GPIO will be HIGH for 2s. Note which LED lights up.");

    // configure all pins as output, start LOW
    for (int i = 0; i < 4; i++) {
        gpio_reset_pin(pins[i]);
        gpio_set_direction(pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(pins[i], 0);
    }

    for (int i = 0; i < 4; i++) {
        ESP_LOGI(TAG, ">> GPIO %d (%s) HIGH — which LED is on?", pins[i], labels[i]);
        gpio_set_level(pins[i], 1);
        vTaskDelay(pdMS_TO_TICKS(2000));
        gpio_set_level(pins[i], 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGI(TAG, "=== Diagnostic done. Fix STEPPER_IN1..IN4 in board_config.h if needed ===");
    vTaskDelay(pdMS_TO_TICKS(2000));
}

// Step 2: motor spin test

extern "C" void app_main()
{
    ESP_LOGI(TAG, "StepperMotor5V Test Started");
    ESP_LOGI(TAG, "Pins: IN1=%d IN2=%d IN3=%d IN4=%d",
             BoardConfig::STEPPER_IN1, BoardConfig::STEPPER_IN2,
             BoardConfig::STEPPER_IN3, BoardConfig::STEPPER_IN4);

    pinDiagnostic();

    StepperMotor5V::Config motor_config = {
        .in1_pin       = BoardConfig::STEPPER_IN1,
        .in2_pin       = BoardConfig::STEPPER_IN2,
        .in3_pin       = BoardConfig::STEPPER_IN3,
        .in4_pin       = BoardConfig::STEPPER_IN4,
        .steps_per_rev = 4096,
    };

    StepperMotor5V motor(motor_config);

    esp_err_t ret = motor.init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Motor init failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Motor initialized — starting spin test");

    const uint32_t TEST_RPM   = 10;
    const uint32_t TEST_STEPS = 512; // 1/8 revolution

    for (int i = 1; i <= 5; i++) {
        ESP_LOGI(TAG, "Cycle %d/5", i);

        ESP_LOGI(TAG, "CW %lu steps @ %lu RPM", TEST_STEPS, TEST_RPM);
        motor.wake();
        ret = motor.move(TEST_STEPS, StepperMotor5V::DIRECTION_CW, TEST_RPM);
        motor.sleep();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "CW move failed: %s", esp_err_to_name(ret));
            break;
        }
        ESP_LOGI(TAG, "CW done");
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "CCW %lu steps @ %lu RPM", TEST_STEPS, TEST_RPM);
        motor.wake();
        ret = motor.move(TEST_STEPS, StepperMotor5V::DIRECTION_CCW, TEST_RPM);
        motor.sleep();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "CCW move failed: %s", esp_err_to_name(ret));
            break;
        }
        ESP_LOGI(TAG, "CCW done");
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    ESP_LOGI(TAG, "Test finished");
}