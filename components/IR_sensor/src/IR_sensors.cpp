#include "IR_sensors.h"

IR_sensors::IR_sensors(std::string name,gpio_num_t sensor_pin, bool alert_when_high, std::shared_ptr<EventBus> event_bus)
    : name(name),
      sensor_pin(sensor_pin),
      alert_when_high(alert_when_high),
      event_bus(event_bus),
      last_sensor_state(-1) {
      };

IR_sensors::~IR_sensors() {
    // Destructor logic if needed
}

void IR_sensors::enter_task(void *arg) {
    IR_sensors *self = static_cast<IR_sensors *>(arg);
    self->monitor_sensors();
}

void IR_sensors::start_monitoring() {
    xTaskCreate(enter_task, "IR_sensor_monitor_task", 4096, this, 5, nullptr);
}   

void IR_sensors::monitor_sensors() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << sensor_pin), // Configure the specific sensor pin
        .mode = GPIO_MODE_INPUT,              // Set as input
        .pull_up_en = GPIO_PULLUP_ENABLE,     // Enable pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,// Disable pull-down
        .intr_type = GPIO_INTR_DISABLE        // Disable interrupt
    };

    gpio_config(&io_conf); // Configure with settings

    int alert_state = alert_when_high ? 1 : 0; // Determine the state that triggers an alert
    int normal_state = alert_when_high ? 0 : 1; // Determine the normal state

    std::string sensor_type;
    if(name.find("Food") != std::string::npos) { // Sensor name contains "food" if it's a food level sensor, can change if want different way to identify
        sensor_type = "Food Level";
    }
    else if(name.find("Movement") != std::string::npos) { // Sensor name contains "movement" if it's a movement sensor, can change if want different way to identify
        sensor_type = "Movement";
    }

    while (true) {
        int sensor_value = gpio_get_level(sensor_pin); // Read sensor

        if (sensor_value == alert_state && last_sensor_state != alert_state) { // IF alert condition met and state changed
            ESP_LOGI("SENSOR", "Alert triggered on pin %d", sensor_pin);
            if(sensor_type == "Food Level") {
                event_bus->publish(EVENT_FOOD_LEVEL_CHANGED, "Food below level sensor " + name);
            }
            if(sensor_type == "Movement") {
                event_bus->publish(EVENT_CAT_APPROACHED);
            }

        } else if(sensor_value == normal_state && last_sensor_state != normal_state) { // IF normal condition met and state changed
            ESP_LOGI("SENSOR", "Alert cleared on pin %d", sensor_pin);
            if(sensor_type == "Food Level") {
                event_bus->publish(EVENT_FOOD_LEVEL_CHANGED, "Food back above level sensor " + name);
            }
            if(sensor_type == "Movement") {
                event_bus->publish(EVENT_CAT_LEFT);
            }
        }
        last_sensor_state = sensor_value;
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to avoid busy-waiting
    }
}
