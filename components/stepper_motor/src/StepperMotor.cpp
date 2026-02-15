#include "StepperMotor.h"
#include "esp_log.h"
#include <cstring>

#include "esp_check.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

const char* StepperMotor::TAG = "StepperMotor";

StepperMotor::StepperMotor(const Config &config) : config_(config) {}

esp_err_t StepperMotor::init() {
    gpio_set_direction(config_.dir_pin, GPIO_MODE_OUTPUT);

    // create rmt tx channel for step pin
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num            = config_.step_pin,
        .clk_src             = RMT_CLK_SRC_DEFAULT,
        .resolution_hz       = config_.rmt_resolution_hz,
        .mem_block_symbols   = 64,
        .trans_queue_depth   = 1,
    };

    ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&tx_cfg, &channel_), TAG, "rmt_new_tx_channel failed");

    rmt_copy_encoder_config_t copy_cfg = {};
    ESP_RETURN_ON_ERROR(rmt_new_copy_encoder(&copy_cfg, &encoder_), TAG, "rmt_new_copy_encoder failed");

    ESP_RETURN_ON_ERROR(rmt_enable(channel_), TAG, "rmt_enable failed");

    ESP_LOGI(TAG, "StepperMotor initialized");
    return ESP_OK;
}

StepperMotor::~StepperMotor() {
    if (channel_) {
        rmt_disable(channel_);
        rmt_del_channel(channel_);
    }
    if (encoder_) {
        rmt_del_encoder(encoder_);
    }
}

esp_err_t StepperMotor::move(uint32_t steps, uint8_t direction, uint32_t rpm) const
{
    // set direction
    gpio_set_level(config_.dir_pin, direction);

    // construct pulse symbols
    uint32_t half_ticks = calcHalfPeriodTicks(rpm);
    rmt_symbol_word_t pulse = {
        .duration0 = static_cast<uint16_t>(half_ticks),
        .level0    = 1,
        .duration1 = static_cast<uint16_t>(half_ticks),
        .level1    = 0,
    };

    // use fixed-size stack buffer to avoid heap allocation
    constexpr size_t BUFFER_SIZE = 64;  // matches mem_block_symbols
    rmt_symbol_word_t symbols[BUFFER_SIZE];

    // fill buffer with the same pulse pattern
    for (size_t i = 0; i < BUFFER_SIZE; i++) {
        symbols[i] = pulse;
    }

    // don't loop
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };

    // send in batches if steps > BUFFER_SIZE
    uint32_t remaining = steps;
    while (remaining > 0) {
        uint32_t batch_size = (remaining > BUFFER_SIZE) ? BUFFER_SIZE : remaining;

        ESP_RETURN_ON_ERROR(
            rmt_transmit(channel_, encoder_, symbols, batch_size * sizeof(rmt_symbol_word_t), &tx_config),
            TAG, "rmt_transmit failed"
        );

        ESP_RETURN_ON_ERROR(
            rmt_tx_wait_all_done(channel_, portMAX_DELAY),
            TAG, "rmt_tx_wait_all_done failed"
        );

        ESP_LOGI(TAG, "Remaining steps: %d", remaining);

        remaining -= batch_size;
    }

    return ESP_OK;
}

uint32_t StepperMotor::degreesToSteps(float degrees) const {
    return static_cast<uint32_t>(degrees / 360.0f * static_cast<float>(config_.steps_per_rev));
}

uint32_t StepperMotor::calcHalfPeriodTicks(uint32_t rpm) const {
    // rpm * steps_per_rev / 60
    // ticks = resolution_hz / steps_per_sec
    // half_ticks = ticks / 2
    const uint32_t steps_per_sec = rpm * config_.steps_per_rev / 60;
    const uint32_t half_ticks    = config_.rmt_resolution_hz / steps_per_sec / 2;
    return half_ticks;
}
