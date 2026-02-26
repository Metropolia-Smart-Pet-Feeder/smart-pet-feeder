#include "motion_detection.h"

MotionDetector::MotionDetector(gpio_num_t pin_left, gpio_num_t pin_center, gpio_num_t pin_right, std::shared_ptr<EventBus> event_bus)
    : sensor_left(pin_left), sensor_center(pin_center), sensor_right(pin_right), event_bus(event_bus){};

void MotionDetector::start_monitoring() {
    if(task_handle != nullptr) {
        return;
    }

    xTaskCreate(enter_task, "Motion detector task", 4096, this, 5, &task_handle);
}

void MotionDetector::stop_monitoring() {
    if(task_handle != nullptr) {
        vTaskDelete(task_handle);
        task_handle = nullptr;
    }
}

void MotionDetector::enter_task(void* arg){
    static_cast<MotionDetector*>(arg)->task_loop();
}

void MotionDetector::task_loop(){
    while(true) {
        update_movement_status();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void MotionDetector::update_movement_status() {

    if(!sensor_left.read_sensor() || !sensor_center.read_sensor() || !sensor_right.read_sensor()){
        event_bus->publish(EVENT_CAT_APPROACHED);
        ESP_LOGI("MotionDetector", "Cat detected");
    }
    else {
        event_bus->publish(EVENT_CAT_LEFT);
        ESP_LOGI("MotionDetector", "Cat left");
    }
}