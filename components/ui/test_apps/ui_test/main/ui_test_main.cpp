#include "esp_log.h"
#include "board_config.h"
#include "SPIBus.h"
#include "I2CBus.h"
#include "Display.h"
#include "TouchDriver.h"
#include "LVGLManager.h"
#include "UI.h"
#include "EventBus.h"
#include <memory>

static const char* TAG = "ui_test";

extern "C" void app_main()
{
    ESP_LOGI(TAG, "UI Component Test");

    // Initialize EventBus
    auto event_bus = std::make_shared<EventBus>();
    if (!event_bus->init()) {
        ESP_LOGE(TAG, "Failed to initialize EventBus!");
        return;
    }
    ESP_LOGI(TAG, "EventBus initialized");

    // Create and initialize SPI bus for display
    auto spi_bus = std::make_shared<SPIBus>(
        SPI3_HOST,
        BoardConfig::LCD_MOSI,
        BoardConfig::LCD_MISO,
        BoardConfig::LCD_SCLK,
        BoardConfig::LCD_WIDTH * BoardConfig::LCD_HEIGHT * 2 + 8
    );
    ESP_ERROR_CHECK(spi_bus->init());
    ESP_LOGI(TAG, "SPI bus initialized");

    // Create and initialize I2C bus for touch
    auto i2c_bus = std::make_shared<I2CBus>(
        I2C_NUM_0,
        BoardConfig::TOUCH_SDA,
        BoardConfig::TOUCH_SCL,
        100000
    );
    ESP_ERROR_CHECK(i2c_bus->init());
    ESP_LOGI(TAG, "I2C bus initialized");

    // Initialize display
    Display::Config display_config = {
        .dc = BoardConfig::LCD_DC,
        .cs = BoardConfig::LCD_CS,
        .rst = BoardConfig::LCD_RST,
        .backlight = BoardConfig::LCD_BACKLIGHT,
        .h_res = BoardConfig::LCD_WIDTH,
        .v_res = BoardConfig::LCD_HEIGHT,
        .pixel_clock_hz = 10 * 1000 * 1000,
        .queue_depth = 10,
        .mirror_x = true,
        .mirror_y = true,
        .swap_xy = true,
        .invert_colors = true
    };
    auto display = std::make_shared<Display>(spi_bus, display_config);
    ESP_ERROR_CHECK(display->init());
    ESP_LOGI(TAG, "Display initialized");

    // Initialize touch driver
    TouchDriver::Config touch_config = {
        .int_pin = BoardConfig::TOUCH_INT,
        .rst_pin = BoardConfig::TOUCH_RST,
        .device_addr = 0x38,
        .clk_hz = 100000,
        .max_x = BoardConfig::LCD_HEIGHT,   // physical touch width before swap (240)
        .max_y = BoardConfig::LCD_WIDTH,    // physical touch height before swap (320)
        .swap_xy = true,
        .mirror_x = false,
        .mirror_y = true
    };
    auto touch = std::make_shared<TouchDriver>(i2c_bus, touch_config);
    ESP_ERROR_CHECK(touch->init());
    ESP_LOGI(TAG, "Touch driver initialized");

    // Initialize LVGL
    LVGLManager::Config lvgl_config = {
        .h_res = BoardConfig::LCD_WIDTH,
        .v_res = BoardConfig::LCD_HEIGHT,
        .draw_buf_lines = 40,
        .double_buffer = true,
        .tick_period_ms = 5,
        .task_period_ms = 20,
        .task_stack_size = 4096,
        .task_priority = 4
    };
    auto lvgl = std::make_shared<LVGLManager>(display, touch, lvgl_config);

    // Create UI with EventBus
    auto ui = std::make_shared<UI>(lvgl, event_bus);
    lvgl->setUIBuilder([ui]() { ui->build(); });

    // Initialize and start LVGL
    ESP_ERROR_CHECK(lvgl->init());
    ESP_ERROR_CHECK(lvgl->start());

    ESP_LOGI(TAG, "UI test running");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
