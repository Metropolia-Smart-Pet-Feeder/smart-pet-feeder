#include "IR_sensors.h"

IR_sensors::IR_sensors(gpio_num_t pin): sensor_pin(pin){
    gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << sensor_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_config);
}

bool IR_sensors::read_sensor(){
    return gpio_get_level(sensor_pin);
}