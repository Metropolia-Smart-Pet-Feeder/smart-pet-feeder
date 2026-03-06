#include "TouchDriver.h"
#include "I2CBus.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/portmacro.h"
#include "esp_lcd_panel_io.h"

static const char* TAG = "TouchDriver";

TouchDriver::TouchDriver(std::shared_ptr<I2CBus> bus, const Config& cfg)
    : i2c_bus(std::move(bus)), config(cfg) {}

TouchDriver::~TouchDriver() {
    if (touch_handle) {
        esp_lcd_touch_del(touch_handle);
    }
    if (io_handle) {
        esp_lcd_panel_io_del(io_handle);
    }
    if (touch_sem) {
        vSemaphoreDelete(touch_sem);
    }
    if (config.int_pin != GPIO_NUM_NC) {
        gpio_isr_handler_remove(config.int_pin);
    }
}

esp_err_t TouchDriver::init() {
    ESP_LOGI(TAG, "Initializing touch driver");
    
    //i2c device config
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = config.device_addr,  
        .control_phase_bytes = 1,
        .dc_bit_offset = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags = {
            .dc_low_on_data = 0,
            .disable_control_phase = 1,
        },
        .scl_speed_hz = 100000,  
    };
    
    esp_err_t ret = esp_lcd_new_panel_io_i2c(i2c_bus->getBusHandle(), &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel IO: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Touch config
    esp_lcd_touch_config_t touch_config = {
        .x_max = config.max_x,
        .y_max = config.max_y,
        .rst_gpio_num = config.rst_pin,
        .int_gpio_num = config.int_pin,
        .levels = {
            .reset = 0,
            .interrupt = 0, //falling edge triggers
        },
        .flags = {
            .swap_xy = config.swap_xy ? 1U : 0U,
            .mirror_x = config.mirror_x ? 1U : 0U,
            .mirror_y = config.mirror_y ? 1U : 0U,
        },
    };
    
    ret = esp_lcd_touch_new_i2c_ft5x06(io_handle, &touch_config, &touch_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create touch driver: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create semaphore for interrupt signaling
    touch_sem = xSemaphoreCreateBinary();
    if (!touch_sem) {
        ESP_LOGE(TAG, "Failed to create touch semaphore");
        return ESP_ERR_NO_MEM;
    }
    
    configureInterrupt();
    initialized = true;
    
    ESP_LOGI(TAG, "Touch driver initialized successfully");
    return ESP_OK;
}

//interrupt pin configuration
void TouchDriver::configureInterrupt() {
    if (config.int_pin == GPIO_NUM_NC) return;

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << config.int_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE //falling edge trigger
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    /*
    *creates a shared ISR handler service that manages interrupts for multiple GPIO pins. 
    *It runs its own dedicated task to handle GPIO interrupts
    *register individual callbacks for each GPIO using gpio_isr_handler_add
    *this is the standard practice as far as i know
    */
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(config.int_pin, isrHandler, this));
}

//give sem when touch triggers isr. taken in sampleTouch.
void TouchDriver::isrHandler(void* arg) {
    auto* self = static_cast<TouchDriver*>(arg);
    BaseType_t hp_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(self->touch_sem, &hp_task_woken);
    if (hp_task_woken) portYIELD_FROM_ISR();
}

void TouchDriver::readHardware() {

    esp_lcd_touch_read_data(touch_handle);//read raw data from chip

    uint16_t xs[1] = {0};
    uint16_t ys[1] = {0};
    uint8_t count = 0;
    bool pressed = esp_lcd_touch_get_coordinates(touch_handle, xs, ys, nullptr, &count, 1);

    if (pressed && count > 0) {
        last_x = xs[0];
        last_y = ys[0];
        last_touched = true;
        // ESP_LOGI(TAG, "Touch: x=%d, y=%d", last_x, last_y);
    } else {
        last_touched = false;
    }
}

//used by lvgl in its touch callback. 
//lvgl does its polling with timer, but we only read the touch when there's an interrupt
bool TouchDriver::sampleTouch(uint16_t& x, uint16_t& y, bool& pressed) {
    if (!initialized) {
        pressed = false;
        return false;
    }

    if (xSemaphoreTake(touch_sem, 0) == pdTRUE) {
        readHardware();
    }

    x = last_x;
    y = last_y;
    pressed = last_touched;
    return last_touched;
}

