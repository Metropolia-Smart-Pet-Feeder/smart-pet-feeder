#ifndef IR_SENSORS_H
#define IR_SENSORS_H

#pragma once
#include "driver/gpio.h"



class IR_sensors {
    public:
        IR_sensors(gpio_num_t pin);
        bool read_sensor();

    private:
        gpio_num_t sensor_pin;

};
#endif