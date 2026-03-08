#include "StepperMotor5V.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

const char* StepperMotor5V::TAG = "StepperMotor5V";

StepperMotor5V::StepperMotor5V(const Config& config) : config_(config) {}

esp_err_t StepperMotor5V::init()
{
    gpio_num_t pins[4] = {config_.in1_pin, config_.in2_pin, config_.in3_pin, config_.in4_pin};
    for (gpio_num_t pin : pins) {
        gpio_reset_pin(pin);
        gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    }
    sleep();
    ESP_LOGI(TAG, "StepperMotor5V initialized (IN1=%d IN2=%d IN3=%d IN4=%d)",
             config_.in1_pin, config_.in2_pin, config_.in3_pin, config_.in4_pin);
    return ESP_OK;
}

esp_err_t StepperMotor5V::move(uint32_t steps, uint8_t direction, uint32_t rpm,
                                uint32_t /*timeout_ms*/)
{
    if (rpm == 0 || config_.steps_per_rev == 0) {
        ESP_LOGE(TAG, "Invalid rpm or steps_per_rev");
        return ESP_ERR_INVALID_ARG;
    }

    // microseconds per half-step
    const uint32_t step_delay_us = 60000000UL / (rpm * config_.steps_per_rev);

    for (uint32_t i = 0; i < steps; i++) {
        if (direction == DIRECTION_CW) {
            step_index_ = (step_index_ + 1) % 8;
        } else {
            step_index_ = (step_index_ + 7) % 8; // wrap backwards
        }
        applyStep(step_index_);
        esp_rom_delay_us(step_delay_us);
    }

    return ESP_OK;
}

uint32_t StepperMotor5V::degreesToSteps(float degrees) const
{
    return static_cast<uint32_t>(degrees / 360.0f * static_cast<float>(config_.steps_per_rev));
}

void StepperMotor5V::wake()
{
    // coils are energized automatically on the first step; nothing to do here
}

void StepperMotor5V::sleep()
{
    gpio_set_level(config_.in1_pin, 0);
    gpio_set_level(config_.in2_pin, 0);
    gpio_set_level(config_.in3_pin, 0);
    gpio_set_level(config_.in4_pin, 0);
}

void StepperMotor5V::applyStep(int index)
{
    const uint8_t* phase = HALF_STEP_SEQ[index & 7];
    gpio_set_level(config_.in1_pin, phase[0]);
    gpio_set_level(config_.in2_pin, phase[1]);
    gpio_set_level(config_.in3_pin, phase[2]);
    gpio_set_level(config_.in4_pin, phase[3]);
}