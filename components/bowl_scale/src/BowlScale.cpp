#include "BowlScale.h"
#include "Events.h"
#include "Types.h"
#include "esp_log.h"

static const char* TAG = "BowlScale";

BowlScale::BowlScale(gpio_num_t dout, gpio_num_t sck, std::shared_ptr<EventBus> event_bus,
                     float scale_factor, float threshold_g)
    : event_bus(event_bus), threshold_g(threshold_g), last_weight(0.0f), task_handle(nullptr)
{
    hx711.init(dout, sck, HX711::eGAIN_128);
    hx711.setScale(scale_factor);
}

BowlScale::~BowlScale()
{
    stop();
}

void BowlScale::start()
{
    if (task_handle != nullptr) return;

    hx711.tareAB();
    last_weight = hx711.getUnitsChannelA(10) + hx711.getUnitsChannelB(10);
    ESP_LOGI(TAG, "Tared. Initial weight: %.1f g", last_weight);

    xTaskCreate(taskEntry, "bowl_scale_task", 4096, this, 5, &task_handle);
}

void BowlScale::stop()
{
    if (task_handle != nullptr) {
        vTaskDelete(task_handle);
        task_handle = nullptr;
    }
}

void BowlScale::taskEntry(void* arg)
{
    static_cast<BowlScale*>(arg)->foodBowlTask();
}

void BowlScale::foodBowlTask()
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        float current = hx711.getUnitsChannelA(10) + hx711.getUnitsChannelB(10);
        float delta = last_weight - current;

        if (delta > threshold_g) {
            ESP_LOGI(TAG, "Food eaten: %.1f g (%.1f -> %.1f)", delta, last_weight, current);
            event_bus->publish(EVENT_FOOD_EATEN, delta);
        }

        last_weight = current;
    }
}