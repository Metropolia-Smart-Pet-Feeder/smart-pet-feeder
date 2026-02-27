#include "food_container.h"

FoodLevelMonitor::FoodLevelMonitor(gpio_num_t pin25, gpio_num_t pin50, gpio_num_t pin75, std::shared_ptr<EventBus> event_bus)
    : sensor25(pin25), sensor50(pin50), sensor75(pin75), event_bus(event_bus){};

void FoodLevelMonitor::start_monitoring() {
    if(task_handle != nullptr) {
        return;
    }

    xTaskCreate(enter_task, "Food level monitor task", 4096, this, 5, &task_handle);
}

void FoodLevelMonitor::stop_monitoring() {
    if(task_handle != nullptr) {
        vTaskDelete(task_handle);
        task_handle = nullptr;
    }
}

void FoodLevelMonitor::enter_task(void* arg){
    static_cast<FoodLevelMonitor*>(arg)->task_loop();
}

void FoodLevelMonitor::task_loop(){
    while(true) {
        update_food_level();
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void FoodLevelMonitor::update_food_level() {
    int level;
    if(!sensor75.read_sensor()){ // sensor reading low means the sensor is blocked, which means the food level is above that sensor.
        level = 4;
    }
    else if(!sensor50.read_sensor()){
        level = 3;
    }
    else if(!sensor25.read_sensor()){
        level = 2;
    }
    else{
        level = 1;
    }

    if(level != previous_level) {
        event_bus->publish(EVENT_FOOD_LEVEL_CHANGED, level);
        ESP_LOGI("FoodLevelMonitor", "Food level updated to: %d", level);
        previous_level = level;
    }
}