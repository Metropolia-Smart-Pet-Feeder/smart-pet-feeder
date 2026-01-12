#ifndef TOUCHDRIVER_H
#define TOUCHDRIVER_H

/**
 * @brief Interrupt-driven touch controller driver for FT5x06 I2C touchscreen and lvgl integration
 * 
 * Hardware interrupts signal touch events by giving the semaphore
 * Semaphore is taken in sampleTouch, called by lvgl as its touch callback
 * LVGL polls via timer (30ms default) and call sampleTouch only when interrupt occurred
 * 
 */

#pragma once

#include <memory>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_lcd_touch_ft5x06.h"
#include "esp_lcd_panel_io.h"
#include "I2CBus.h"

class TouchDriver
{
public:
    struct Config {
        gpio_num_t int_pin;          
        gpio_num_t rst_pin;          
        uint8_t device_addr;            
        uint32_t clk_hz;         
        uint16_t max_x;             
        uint16_t max_y;              
        bool swap_xy{false};
        bool mirror_x{false};
        bool mirror_y{false};
    };

    TouchDriver(std::shared_ptr<I2CBus> i2c_bus, const Config& config);
    ~TouchDriver();

    esp_err_t init();
    bool sampleTouch(uint16_t& x, uint16_t& y, bool& touched);
    esp_lcd_touch_handle_t getHandle() const { return touch_handle;}
    
private:
    std::shared_ptr<I2CBus> i2c_bus;
    Config config;
    esp_lcd_touch_handle_t touch_handle{nullptr};
    esp_lcd_panel_io_handle_t io_handle{nullptr};
    SemaphoreHandle_t touch_sem{nullptr};
    uint16_t last_x{0};
    uint16_t last_y{0};
    bool last_touched{false};
    bool initialized{false};

    static void isrHandler(void* arg);
    void readHardware();
    void configureInterrupt();
};

#endif