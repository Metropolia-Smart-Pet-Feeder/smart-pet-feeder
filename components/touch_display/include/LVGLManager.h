#ifndef LVGLMANAGER_H
#define LVGLMANAGER_H

/**
 * @brief LVGL integration layer
 * 
 * Manages LVGL's display/input drivers, timing, and thread safety.
 * - Registers display flush callback → forwards to Display driver
 * - Registers touch read callback → forwards to TouchDriver
 * - Runs tick timer to update LVGL's clock
 * - Runs LVGL task that calls lv_timer_handler() to process rendering/input
 * 
 * Important:
 * - Set LV_COLOR_16_SWAP to 1 in lv_conf.h
 * - Run idf.py menuconfig, go to Component config -> LVGL configuration -> "Uncheck this to use custom lv_conf.h"
 * 
 */

#pragma once
#include <memory>
#include <functional>
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "Display.h"
#include "TouchDriver.h"

class LVGLManager
{
public:
    struct Config {
        uint16_t h_res;                    
        uint16_t v_res;                    
        uint16_t draw_buf_lines;           
        bool double_buffer;                
        uint32_t tick_period_ms;           
        uint32_t task_period_ms;           
        size_t task_stack_size;           
        UBaseType_t task_priority; 
    };
    
    LVGLManager(std::shared_ptr<Display> display, 
                std::shared_ptr<TouchDriver> touch,
                const Config& config);
    ~LVGLManager();

    // Initialize LVGL (call before start)
    esp_err_t init();
    
    // Start LVGL task and timer
    esp_err_t start();
    
    // Stop LVGL task and timer
    void stop();
    
    // Set UI builder callback (called after LVGL init)
    void setUIBuilder(std::function<void()> builder);
    
    // Get LVGL mutex for thread-safe operations
    SemaphoreHandle_t getMutex() const { return lvgl_mutex; }
    
    // Lock/unlock for external LVGL calls
    bool lock(uint32_t timeout_ms = 100);
    void unlock();

private:
    // LVGL callbacks (static wrappers)
    static void flushCallback(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_map);
    static void touchCallback(lv_indev_drv_t* drv, lv_indev_data_t* data);
    static void tickTimerCallback(void* arg);
    static void taskEntry(void* arg);
    
    // Instance methods
    void flushDisplay(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_map);
    void readTouch(lv_indev_drv_t* drv, lv_indev_data_t* data);
    void lvglTask();
    
    // Initialize subsystems: internal display and touch timer; and lgvl's esp tick timer
    esp_err_t initDisplay();
    esp_err_t initTouch();
    esp_err_t initTimer();

    std::shared_ptr<Display> display;
    std::shared_ptr<TouchDriver> touch;
    Config config;
    
    /*
    * LVGL display and input device driver structures.
    * the name is misleading to be honest 
    * it's called drv but it's more like an interface that lvgl use to access the hardware implementation
    * lvgl doesn't care what harware you use. it's cross platform.
    */
    lv_disp_draw_buf_t disp_buf;
    lv_disp_drv_t disp_drv;
    lv_disp_t* disp{nullptr};
    lv_indev_drv_t indev_drv;
    lv_indev_t* indev{nullptr};
    
    // Draw buffers
    lv_color_t* buf1{nullptr};
    lv_color_t* buf2{nullptr};
    
    SemaphoreHandle_t lvgl_mutex{nullptr};
    
    // Task & timer
    TaskHandle_t task_handle{nullptr};
    esp_timer_handle_t tick_timer{nullptr};
    
    // UI builder callback
    std::function<void()> ui_builder;
    
    bool initialized{false};
    bool running{false};
};

#endif