#ifndef I2CBUS_H
#define I2CBUS_H

#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <cstdint>

class I2CBus
{
public:
    I2CBus(i2c_port_t port, gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t clk_speed = 100000);
    ~I2CBus();

    esp_err_t init();
    i2c_master_bus_handle_t getBusHandle() const { return bus_handle; }
    i2c_port_t getPort() const { return i2c_port; }
    bool isInitialized() const { return initialized; }

private:
    i2c_port_t i2c_port;
    gpio_num_t sda_pin;
    gpio_num_t scl_pin;
    uint32_t clk_speed;
    i2c_master_bus_handle_t bus_handle;
    bool initialized{false};

};

#endif