#ifndef MOTION_DETECTION_H
#define MOTION_DETECTION_H

#pragma once
#include <stdio.h>
#include <memory>
#include "IR_sensors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "EventBus.h"


class MotionDetector{
    public:
        MotionDetector(gpio_num_t pin_left, gpio_num_t pin_center, gpio_num_t pin_right, std::shared_ptr<EventBus> event_bus);
        void start_monitoring();
        void stop_monitoring();

    private:
        IR_sensors sensor_left;
        IR_sensors sensor_center;
        IR_sensors sensor_right;
        std::shared_ptr<EventBus> event_bus;
        bool cat_present_previously = false;
        TaskHandle_t task_handle = nullptr;

        static void enter_task(void* arg);
        void task_loop();
        void update_movement_status();
};
#endif