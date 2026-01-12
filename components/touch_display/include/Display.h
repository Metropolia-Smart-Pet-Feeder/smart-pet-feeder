#ifndef DISPLAY_H
#define DISPLAY_H

/**
 * @brief Hardware abstraction layer for ILI9341 LCD controller via SPI
 * 
 * This class manages a 240x320 TFT LCD display using the ILI9341 controller chip.
 * It handles:
 * - SPI communication setup (data/command pin control, chip select)
 * - LCD initialization sequence (reset, color format, orientation)
 * - Asynchronous pixel data transfer using DMA
 * - Backlight control
 * - Event callbacks for frame synchronization with LVGL graphics library
 * 
 * The display uses RGB565 color format (16-bit per pixel) and supports hardware
 * mirroring for screen rotation. Drawing operations are non-blocking. Pixel data
 * is transferred via SPI DMA, and a callback notifies when the transfer completes.
 * This prevents screen tearing and allows the graphics engine to prepare the next
 * frame while the current one is being transmitted.
 * 
 * Used by lvgl manager in the project.
 * Invert_colors should be set to true for correct color.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <functional>
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "SPIBus.h"

class Display
{
public:
    struct Config {
        gpio_num_t dc;
        gpio_num_t cs;
        gpio_num_t rst;
        gpio_num_t backlight;
        uint16_t h_res;
        uint16_t v_res;
        uint32_t pixel_clock_hz;
        uint8_t queue_depth;
        bool mirror_x;
        bool mirror_y;
        bool invert_colors;
    };

    Display(std::shared_ptr<SPIBus> spi_bus, const Config& config);
    ~Display();

    esp_err_t init();
    void setFlushCompleteCallback(std::function<void()> cb); 
    //i need to use this to set the lvgl flush ready as the callback

    void drawBitmap(int16_t x1, int16_t y1, int16_t x2, int16_t y2, const void* pixel_data);

    esp_lcd_panel_handle_t getPanelHandle() const { return panel_handle; }
    uint16_t getHRes() const { return config.h_res; }
    uint16_t getVRes() const { return config.v_res; }

private:
    std::shared_ptr<SPIBus> spi_bus;
    Config config;

    esp_err_t initPanelIO();
    esp_err_t initPanel();
    esp_err_t initBacklight();

    static bool onColorTransDone(esp_lcd_panel_io_handle_t io,
                                 esp_lcd_panel_io_event_data_t* evdata,
                                 void* user_ctx);            
    std::function<void()> flush_done_cb;                    

    esp_lcd_panel_io_handle_t panel_io_handle {nullptr};
    esp_lcd_panel_handle_t panel_handle {nullptr};
    bool initialized {false};

};

#endif