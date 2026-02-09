#include "I2CBus.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char* TAG = "I2CBus";

I2CBus::I2CBus(i2c_port_t port, gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t clk_speed)
    : i2c_port(port), 
    sda_pin(sda_pin), 
    scl_pin(scl_pin), 
    clk_speed(clk_speed),
    bus_handle(nullptr),
    initialized(false) {}

I2CBus::~I2CBus()
{
    if (initialized && bus_handle) {
        i2c_del_master_bus(bus_handle);
    }
}

esp_err_t I2CBus::init()
{
    if (initialized) {
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_config = {
        .i2c_port = i2c_port,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7, //ignore signal pulses that are shorter than 7 i2c clock cycle
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = true  // Enable internal pull-ups
        }
    };

    esp_err_t result = i2c_new_master_bus(&bus_config, &bus_handle);
    if (result == ESP_OK) {
        initialized = true;
    }
    return result;
}
