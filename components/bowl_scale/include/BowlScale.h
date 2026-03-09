#pragma once

#include <memory>
#include "HX711.h"
#include "EventBus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class BowlScale
{
public:
    BowlScale(gpio_num_t dout, gpio_num_t sck, std::shared_ptr<EventBus> event_bus,
              float scale_factor, float threshold_g = 2.0f);
    ~BowlScale();

    void start();
    void stop();

private:
    HX711 hx711;
    std::shared_ptr<EventBus> event_bus;
    float threshold_g;
    float last_weight;
    TaskHandle_t task_handle;

    static void taskEntry(void* arg);
    void foodBowlTask();
};