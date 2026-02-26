#ifndef FOOD_CONTAINER_H
#define FOOD_CONTAINER_H

#pragma once
#include <stdio.h>
#include <memory>
#include "IR_sensors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "EventBus.h"


class FoodLevelMonitor{
    public:
        FoodLevelMonitor(gpio_num_t pin25, gpio_num_t pin50, gpio_num_t pin75, std::shared_ptr <EventBus> event_bus);
        void start_monitoring();
        void stop_monitoring();

    private:
        IR_sensors sensor25;
        IR_sensors sensor50;
        IR_sensors sensor75;
        std::shared_ptr<EventBus> event_bus;
        TaskHandle_t task_handle = nullptr;

        static void enter_task(void* arg);
        void task_loop();
        void update_food_level();
};
#endif