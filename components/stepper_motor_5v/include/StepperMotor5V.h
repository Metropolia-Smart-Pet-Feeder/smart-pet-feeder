#pragma once
#include <cstdint>
#include "driver/gpio.h"
#include "esp_err.h"

class StepperMotor5V
{
public:
    struct Config
    {
        gpio_num_t in1_pin;
        gpio_num_t in2_pin;
        gpio_num_t in3_pin;
        gpio_num_t in4_pin;
        uint32_t steps_per_rev; // number of half-steps per full revolution
    };

    static constexpr uint8_t DIRECTION_CW  = 1;
    static constexpr uint8_t DIRECTION_CCW = 0;

    explicit StepperMotor5V(const Config& config);

    esp_err_t init();
    esp_err_t move(uint32_t steps, uint8_t direction, uint32_t rpm, uint32_t timeout_ms = 0);
    [[nodiscard]] uint32_t degreesToSteps(float degrees) const;
    void wake();  // no-op: coils energize on first step
    void sleep(); // de-energize all coils

private:
    static const char* TAG;
    Config config_;
    int step_index_{0};

    // Half-step sequence (8 phases): {IN1, IN2, IN3, IN4}
    static constexpr uint8_t HALF_STEP_SEQ[8][4] = {
        {1, 0, 0, 0},
        {1, 1, 0, 0},
        {0, 1, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 1, 1},
        {0, 0, 0, 1},
        {1, 0, 0, 1},
    };

    void applyStep(int index);
};
