#include "LVGLManager.h"
#include "Display.h"
#include "TouchDriver.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char* TAG = "LVGLManager";

LVGLManager::LVGLManager(std::shared_ptr<Display> display,
                         std::shared_ptr<TouchDriver> touch,
                         const Config& config)
    : display(std::move(display))
    , touch(std::move(touch))
    , config(config)
{
}

LVGLManager::~LVGLManager()
{
    stop();
    
    if (buf1) heap_caps_free(buf1);
    if (buf2) heap_caps_free(buf2);
    if (lvgl_mutex) vSemaphoreDelete(lvgl_mutex);
    
    lv_deinit();
}

esp_err_t LVGLManager::init()
{
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing LVGL v%d.%d.%d", 
             lv_version_major(), lv_version_minor(), lv_version_patch());
    
    // Create mutex for thread safety
    lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    if (!lvgl_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize LVGL
    lv_init();
    
    // Initialize display and touch interface structure.
    ESP_ERROR_CHECK(initDisplay());
    ESP_ERROR_CHECK(initTouch());
    
    // Call UI builder if set
    if (ui_builder) {
        ESP_LOGI(TAG, "Building UI");
        ui_builder();
    }
    
    initialized = true;
    ESP_LOGI(TAG, "LVGL initialized successfully");
    return ESP_OK;
}

esp_err_t LVGLManager::initDisplay()
{
    // Allocate draw buffers (DMA-capable memory)
    size_t buf_size = config.h_res * config.draw_buf_lines;
    buf1 = static_cast<lv_color_t*>(heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA));

    if (!buf1) {
        ESP_LOGE(TAG, "Failed to allocate draw buffer 1");
        return ESP_ERR_NO_MEM;
    }
    
    if (config.double_buffer) {
        buf2 = static_cast<lv_color_t*>(heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA));

        if (!buf2) {
            ESP_LOGE(TAG, "Failed to allocate draw buffer 2");
            return ESP_ERR_NO_MEM;
        }
    }
    
    // Initialize display buffer
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buf_size);
    
    // Initialize ivgl's display driver structure and register it
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = config.h_res;
    disp_drv.ver_res = config.v_res;
    disp_drv.flush_cb = flushCallback; //display->drawBitmap
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = this;
    
    //lvgl will create a timer for display refresh internally. 
    //timer interval should be configured in lv_conf.h
    disp = lv_disp_drv_register(&disp_drv);
    if (!disp) {
        ESP_LOGE(TAG, "Failed to register display driver");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Set flush completion callback in Display
    // dma transaction interrupt triggers this
    display->setFlushCompleteCallback([this]() {
        lv_disp_flush_ready(&disp_drv);
    });
    
    ESP_LOGI(TAG, "Display driver registered (%dx%d, buf=%d lines%s)",
             config.h_res, config.v_res, config.draw_buf_lines,
             config.double_buffer ? ", double buffered" : "");
    
    return ESP_OK;
}

esp_err_t LVGLManager::initTouch()
{
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER; // touch screen is a pointer device
    indev_drv.read_cb = touchCallback; // touch->sampleTouch
    indev_drv.user_data = this;
    
    //lvgl will create a timer to poll the touch input internally
    indev = lv_indev_drv_register(&indev_drv);
    if (!indev) {
        ESP_LOGE(TAG, "Failed to register input driver");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Touch driver registered");
    return ESP_OK;
}

//this initiate lvgl's esp tick timer. 
//lvgl uses this to track its internal timer, such as display and touch
esp_err_t LVGLManager::initTimer()
{
    esp_timer_create_args_t timer_args = {
        .callback = tickTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick",
        .skip_unhandled_events = true
    };
    
    esp_err_t ret = esp_timer_create(&timer_args, &tick_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create tick timer");
        return ret;
    }
    
    ret = esp_timer_start_periodic(tick_timer, config.tick_period_ms * 1000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start tick timer");
        return ret;
    }
    
    ESP_LOGI(TAG, "Tick timer started (%lu ms)", config.tick_period_ms);
    return ESP_OK;
}

esp_err_t LVGLManager::start()
{
    if (!initialized) {
        ESP_LOGE(TAG, "Not initialized, call init() first");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (running) {
        ESP_LOGW(TAG, "Already running");
        return ESP_OK;
    }
    
    // Start tick timer
    ESP_ERROR_CHECK(initTimer());
    
    // Set running flag BEFORE creating task
    running = true;
    
    // Create LVGL task
    BaseType_t ret = xTaskCreate(
        taskEntry,
        "lvgl",
        config.task_stack_size,
        this,
        config.task_priority,
        &task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LVGL task");
        running = false;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "LVGL task started");
    return ESP_OK;
}

void LVGLManager::stop()
{
    if (!running) return;
    
    // Stop timer
    if (tick_timer) {
        esp_timer_stop(tick_timer);
        esp_timer_delete(tick_timer);
        tick_timer = nullptr;
    }
    
    // Delete task
    if (task_handle) {
        vTaskDelete(task_handle);
        task_handle = nullptr;
    }
    
    running = false;
    ESP_LOGI(TAG, "LVGL stopped");
}

void LVGLManager::setUIBuilder(std::function<void()> builder)
{
    ui_builder = std::move(builder);
}

bool LVGLManager::lock(uint32_t timeout_ms)
{
    if (!lvgl_mutex) return false;
    return xSemaphoreTakeRecursive(lvgl_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void LVGLManager::unlock()
{
    if (lvgl_mutex) {
        xSemaphoreGiveRecursive(lvgl_mutex);
    }
}

// Static callback wrappers
void LVGLManager::flushCallback(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_map)
{
    auto* self = static_cast<LVGLManager*>(drv->user_data);
    self->flushDisplay(drv, area, color_map);
}

void LVGLManager::touchCallback(lv_indev_drv_t* drv, lv_indev_data_t* data)
{
    auto* self = static_cast<LVGLManager*>(drv->user_data);
    self->readTouch(drv, data);
}

void LVGLManager::tickTimerCallback(void* arg)
{
    lv_tick_inc(static_cast<LVGLManager*>(arg)->config.tick_period_ms);
}

void LVGLManager::taskEntry(void* arg)
{
    auto* self = static_cast<LVGLManager*>(arg);
    self->lvglTask();
}

void LVGLManager::flushDisplay(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_map)
{
    display->drawBitmap(area->x1, area->y1, area->x2, area->y2, color_map);
    // lv_disp_flush_ready() is called by the async callback in Display driver
}

void LVGLManager::readTouch(lv_indev_drv_t* drv, lv_indev_data_t* data)
{
    // Check if touch is available
    if (!touch) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }
    
    uint16_t x, y;
    bool pressed;
    
    touch->sampleTouch(x, y, pressed);
    
    data->point.x = x;
    data->point.y = y;
    data->state = pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

void LVGLManager::lvglTask()
{
    ESP_LOGI(TAG, "LVGL task running");
    
    while (running) {
        // Lock mutex
        if (lock(10)) {
            // Process LVGL timers and render
            // it will interate through all internal timer, including display and touch
            lv_timer_handler();
            unlock();
        }
        
        vTaskDelay(pdMS_TO_TICKS(config.task_period_ms));
    }
    
    ESP_LOGI(TAG, "LVGL task exiting");
    vTaskDelete(nullptr);
}