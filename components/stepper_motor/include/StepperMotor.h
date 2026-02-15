#ifndef STEPPERMOTOR_H
#define STEPPERMOTOR_H
#include <cstdint>
#include "soc/gpio_num.h"
#include "esp_err.h"
#include "driver/rmt_types.h"

class StepperMotor
{
public:
    struct Config
    {
        gpio_num_t step_pin;
        gpio_num_t dir_pin;
        uint32_t steps_per_rev;
        uint32_t rmt_resolution_hz;
    };

    explicit StepperMotor(const Config& config);
    ~StepperMotor();

    esp_err_t init();
    esp_err_t move(uint32_t steps, uint8_t direction, uint32_t rpm) const;
    [[nodiscard]] uint32_t degreesToSteps(float degrees) const;

private:
    static const char* TAG;

    Config config_;
    rmt_channel_handle_t channel_ = nullptr;
    rmt_encoder_handle_t encoder_ = nullptr;

    [[nodiscard]] uint32_t calcHalfPeriodTicks(uint32_t rpm) const;
};

#endif
