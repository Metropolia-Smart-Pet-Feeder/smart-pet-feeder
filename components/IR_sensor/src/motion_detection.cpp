#include "motion_detection.h"

static const char* TAG = "MotionDetector";

MotionDetector::MotionDetector(gpio_num_t pin_left, gpio_num_t pin_center, gpio_num_t pin_right, std::shared_ptr<EventBus> event_bus, std::shared_ptr<ImageReceiver> image_receiver, std::string device_id)
    : sensor_left(pin_left), sensor_center(pin_center), sensor_right(pin_right), event_bus(event_bus), image_receiver(image_receiver), device_id(device_id){};

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
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void MotionDetector::update_movement_status() {
    if(!sensor_left.read_sensor() || !sensor_center.read_sensor() || !sensor_right.read_sensor()){
        if(!cat_present_previously) {
            event_bus->publish(EVENT_CAT_APPROACHED);
            ESP_LOGI(TAG, "Cat detected");

            if(image_receiver && !device_id.empty()){
                ESP_LOGI(TAG, "Capturing image...");
                esp_err_t ret = image_receiver->post_image(device_id.c_str());
                if(ret != ESP_OK){
                    ESP_LOGW(TAG, "post_image failed: %s", esp_err_to_name(ret));
                }
            }
        }
        cat_present_previously = true;
    }
    else {
        if(cat_present_previously) {
            event_bus->publish(EVENT_CAT_LEFT);
            ESP_LOGI(TAG, "Cat left");
        }
        cat_present_previously = false;
    }
}
