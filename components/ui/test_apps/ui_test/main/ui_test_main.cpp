#include "esp_log.h"
#include "SPIBus.h"
#include "I2CBus.h"
#include "Display.h"
#include "TouchDriver.h"
#include "LVGLManager.h"
#include "UI.h"
#include "EventBus.h"
#include <memory>

static const char* TAG = "ui_test";

// Board pin definitions for test
namespace TestBoardConfig {
    constexpr gpio_num_t LCD_SCLK = GPIO_NUM_21;
    constexpr gpio_num_t LCD_MOSI = GPIO_NUM_19;
    constexpr gpio_num_t LCD_MISO = GPIO_NUM_20;
    constexpr gpio_num_t LCD_DC   = GPIO_NUM_1;
    constexpr gpio_num_t LCD_RST  = GPIO_NUM_37;
    constexpr gpio_num_t LCD_CS   = GPIO_NUM_36;
    constexpr gpio_num_t LCD_BACKLIGHT = GPIO_NUM_35;

    constexpr gpio_num_t TOUCH_SCL = GPIO_NUM_9;
    constexpr gpio_num_t TOUCH_SDA = GPIO_NUM_8;
    constexpr gpio_num_t TOUCH_RST = GPIO_NUM_10;
    constexpr gpio_num_t TOUCH_INT = GPIO_NUM_11;

    constexpr int LCD_WIDTH  = 240;
    constexpr int LCD_HEIGHT = 320;
}

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
        TestBoardConfig::LCD_MOSI,
        TestBoardConfig::LCD_MISO,
        TestBoardConfig::LCD_SCLK,
        TestBoardConfig::LCD_WIDTH * TestBoardConfig::LCD_HEIGHT * 2 + 8
    );
    ESP_ERROR_CHECK(spi_bus->init());
    ESP_LOGI(TAG, "SPI bus initialized");

    // Create and initialize I2C bus for touch
    auto i2c_bus = std::make_shared<I2CBus>(
        I2C_NUM_0,
        TestBoardConfig::TOUCH_SDA,
        TestBoardConfig::TOUCH_SCL,
        100000
    );
    ESP_ERROR_CHECK(i2c_bus->init());
    ESP_LOGI(TAG, "I2C bus initialized");

    // Initialize display
    Display::Config display_config = {
        .dc = TestBoardConfig::LCD_DC,
        .cs = TestBoardConfig::LCD_CS,
        .rst = TestBoardConfig::LCD_RST,
        .backlight = TestBoardConfig::LCD_BACKLIGHT,
        .h_res = TestBoardConfig::LCD_WIDTH,
        .v_res = TestBoardConfig::LCD_HEIGHT,
        .pixel_clock_hz = 10 * 1000 * 1000,
        .queue_depth = 10,
        .mirror_x = true,
        .mirror_y = false,
        .invert_colors = true
    };
    auto display = std::make_shared<Display>(spi_bus, display_config);
    ESP_ERROR_CHECK(display->init());
    ESP_LOGI(TAG, "Display initialized");

    // Initialize touch driver
    TouchDriver::Config touch_config = {
        .int_pin = TestBoardConfig::TOUCH_INT,
        .rst_pin = TestBoardConfig::TOUCH_RST,
        .device_addr = 0x38,
        .clk_hz = 100000,
        .max_x = TestBoardConfig::LCD_WIDTH,
        .max_y = TestBoardConfig::LCD_HEIGHT,
        .swap_xy = false,
        .mirror_x = false,
        .mirror_y = false
    };
    auto touch = std::make_shared<TouchDriver>(i2c_bus, touch_config);
    ESP_ERROR_CHECK(touch->init());
    ESP_LOGI(TAG, "Touch driver initialized");

    // Initialize LVGL
    LVGLManager::Config lvgl_config = {
        .h_res = TestBoardConfig::LCD_WIDTH,
        .v_res = TestBoardConfig::LCD_HEIGHT,
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
