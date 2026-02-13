/**
 * Integration test: UI + WiFiManager provisioning flow
 *
 * Tests contents:
 *   1. UI displays main screen with CONNECT button
 *   2. Clicking CONNECT -> EventBus publishes EVENT_START_PROVISIONING
 *   3. WiFiManager receives event -> starts BLE provisioning
 *   4. User sends WiFi credentials via phone app over BLE
 *   5. WiFi connects -> EVENT_WIFI_CONNECTED -> UI updates (icon turns green,
 *      provisioning screen switches to WiFi status screen)
 *
 * Steps:
 *   1. Build and flash
 *   2. Tap the "CONNECT" button on the screen
 *   3. Screen should switch to provisioning page showing BLE device name
 *   4. On your phone, install and open the ESP BLE Provisioning app
 *   5. Find the device (PROV_PETFEEDER_XXXXXX) and send WiFi credentials
 *   6. WiFi icon should turn green, screen switches to "Connected to: <SSID>"
 *   7. Monitor serial log for confirmation
 */

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
#include <memory>

static const char* TAG = "ui_wifi_prov";

// Event logging callbacks to verify the integration flow via serial monitor
static void onProvisioningStarted(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    auto* prov_data = static_cast<provisioning_event_data_t*>(data);
    ESP_LOGI(TAG, "[EVENT] EVENT_PROVISIONING_STARTED - device: %s", prov_data->device_name);
}

static void onWiFiConnected(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    auto* wifi_data = static_cast<wifi_event_data_t*>(data);
    ESP_LOGI(TAG, "[EVENT] EVENT_WIFI_CONNECTED - SSID: %s", wifi_data->ssid);
}

static void onWiFiDisconnected(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    ESP_LOGI(TAG, "[EVENT] EVENT_WIFI_DISCONNECTED");
}

static void onStartProvisioning(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    ESP_LOGI(TAG, "[EVENT] EVENT_START_PROVISIONING (UI button -> EventBus)");
}

extern "C" void app_main()
{
    ESP_LOGI(TAG, "Integration Test: UI + WiFi Provisioning");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    // Initialize EventBus (the glue between UI and WiFiManager)
    auto event_bus = std::make_shared<EventBus>();
    if (!event_bus->init()) {
        ESP_LOGE(TAG, "FAIL: EventBus init failed");
        return;
    }
    ESP_LOGI(TAG, "EventBus initialized");

    // Subscribe logging callbacks to observe the event flow
    event_bus->subscribe(EVENT_START_PROVISIONING, onStartProvisioning, nullptr);
    event_bus->subscribe(EVENT_PROVISIONING_STARTED, onProvisioningStarted, nullptr);
    event_bus->subscribe(EVENT_WIFI_CONNECTED, onWiFiConnected, nullptr);
    event_bus->subscribe(EVENT_WIFI_DISCONNECTED, onWiFiDisconnected, nullptr);

    // Initialize WiFiManager (subscribes to EVENT_START_PROVISIONING internally)
    auto wifi_manager = std::make_shared<WiFiManager>(event_bus);
    if (!wifi_manager->init()) {
        ESP_LOGE(TAG, "FAIL: WiFiManager init failed");
        return;
    }
    ESP_LOGI(TAG, "WiFiManager initialized");

    // Initialize display hardware
    auto spi_bus = std::make_shared<SPIBus>(
        SPI3_HOST,
        BoardConfig::LCD_MOSI,
        BoardConfig::LCD_MISO,
        BoardConfig::LCD_SCLK,
        BoardConfig::LCD_WIDTH * BoardConfig::LCD_HEIGHT * 2 + 8
    );
    ESP_ERROR_CHECK(spi_bus->init());
    ESP_LOGI(TAG, "SPI bus initialized");

    auto i2c_bus = std::make_shared<I2CBus>(
        I2C_NUM_0,
        BoardConfig::TOUCH_SDA,
        BoardConfig::TOUCH_SCL,
        100000
    );
    ESP_ERROR_CHECK(i2c_bus->init());
    ESP_LOGI(TAG, "I2C bus initialized");

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

    // Create UI (subscribes to EVENT_WIFI_CONNECTED/DISCONNECTED internally)
    auto ui = std::make_shared<UI>(lvgl, event_bus);
    lvgl->setUIBuilder([ui]() { ui->build(); });

    ESP_ERROR_CHECK(lvgl->init());
    ESP_ERROR_CHECK(lvgl->start());

    ESP_LOGI(TAG, "Test ready. Tap CONNECT on the screen.");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}