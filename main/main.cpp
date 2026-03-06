#include "esp_log.h"
#include "nvs_flash.h"
#include "board_config.h"
#include "SPIBus.h"
#include "I2CBus.h"
#include "Display.h"
#include "TouchDriver.h"
#include "LVGLManager.h"
#include "UI.h"
#include "EventBus.h"
#include "Events.h"
#include "WiFiManager.h"
#include "MQTTManager.h"
#include "Types.h"
#include "DispenseControl.h"
#include "RC522.h"
#include "food_container.h"
#include "motion_detection.h"
#include "Scheduler.h"
#include <memory>

static const char* TAG = "main";


extern "C" void app_main()
{
    ESP_LOGI(TAG, "Starting Pet Feeder Application");
    
    // Initialize NVS for wifi credential storage
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");
    
    // Initialize EventBus
    auto event_bus = std::make_shared<EventBus>();
    if (!event_bus->init()) {
        ESP_LOGE(TAG, "Failed to initialize EventBus!");
        return;
    }
    ESP_LOGI(TAG, "EventBus initialized");
    
    // Initialize WiFiManager first (we need device_id for MQTT config)
    auto wifi_manager = std::make_shared<WiFiManager>(event_bus);
    if (!wifi_manager->init()) {
        ESP_LOGE(TAG, "Failed to initialize WiFiManager!");
        return;
    }
    ESP_LOGI(TAG, "WiFiManager initialized");

    // mqtt config
    MQTTManager::Config mqtt_config = {
        .broker_uri = "mqtt://104.168.122.188:1883",
        .client_id = wifi_manager->getDeviceId(),
        .username = "petfeeder_admin",
        .password = "admin",
        .device_id = wifi_manager->getDeviceId()
    };

    // Initialize MQTT Manager
    auto mqtt_manager = std::make_shared<MQTTManager>(event_bus, mqtt_config);
    if (!mqtt_manager->init()) {
        ESP_LOGE(TAG, "Failed to initialize MQTT Manager!");
        return;
    }
    ESP_LOGI(TAG, "MQTT Manager initialized");

    // Initialize Scheduler
    auto scheduler = std::make_shared<Scheduler>(event_bus);
    scheduler->init();
    ESP_LOGI(TAG, "Scheduler initialized");

    // Initialize DispenseControl
    DispenseControl::Config dispense_config = {
        .default_portions       = 1,
        .steps_per_portion      = 100,
        .motor_rpm              = 20,
        .operation_timeout_ms   = 10000,
        .portion_delay_ms       = 1000,
        .min_food_level_percent = 0,
        .motor_config = {
            .step_pin           = BoardConfig::MOTOR_STEP,
            .dir_pin            = BoardConfig::MOTOR_DIR,
            .steps_per_rev      = 400,
            .rmt_resolution_hz  = 1000000
        }
    };
    auto dispense = std::make_shared<DispenseControl>(event_bus, dispense_config);
    ESP_ERROR_CHECK(dispense->init());
    ESP_LOGI(TAG, "DispenseControl initialized");
    
    
    // Create and initialize SPI bus for display
    auto spi_bus = std::make_shared<SPIBus>(
        SPI3_HOST,
        BoardConfig::LCD_MOSI,
        BoardConfig::LCD_MISO,
        BoardConfig::LCD_SCLK,
        240 * 320 * 2 + 8
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
        .mirror_y = false,
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
        .max_x = BoardConfig::LCD_WIDTH,
        .max_y = BoardConfig::LCD_HEIGHT,
        .swap_xy = false,
        .mirror_x = false,
        .mirror_y = false
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
        .task_period_ms = 20,  // Increased from 5 to 20ms to avoid starving IDLE task
        .task_stack_size = 4096,
        .task_priority = 4
    };
    auto lvgl = std::make_shared<LVGLManager>(display, touch, lvgl_config);
    
    // Initialize RFID 
    auto rfid = std::make_shared<RC522>(spi_bus, BoardConfig::RFID_CS, BoardConfig::RFID_RST, event_bus);
    ESP_ERROR_CHECK(rfid->init());
    rfid->startTask();
    ESP_LOGI(TAG, "RFID reader initialized and task started");

    // Initialize food container monitoring
    auto food_monitor = std::make_shared<FoodLevelMonitor>(BoardConfig::IR_FOOD_LEVEL_25, BoardConfig::IR_FOOD_LEVEL_50, BoardConfig::IR_FOOD_LEVEL_75, event_bus);
    food_monitor->start_monitoring();

    // Initialize motion detection
    auto motion_detector = std::make_shared<MotionDetector>(BoardConfig::IR_MOTION_LEFT, BoardConfig::IR_MOTION_CENTER, BoardConfig::IR_MOTION_RIGHT, event_bus);
    motion_detector->start_monitoring();

    // Create UI with EventBus
    auto ui = std::make_shared<UI>(lvgl, event_bus);
    lvgl->setUIBuilder([ui]() { ui->build(); });
    
    // Initialize and start LVGL
    ESP_ERROR_CHECK(lvgl->init());
    ESP_ERROR_CHECK(lvgl->start());
    
    ESP_LOGI(TAG, "Application running");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}