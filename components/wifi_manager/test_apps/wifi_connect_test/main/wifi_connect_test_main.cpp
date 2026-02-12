#include "esp_log.h"
#include "nvs_flash.h"
#include "WiFiManager.h"
#include "EventBus.h"
#include "Events.h"
#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <memory>

static const char* TAG = "wifi_conn_test";

static led_strip_handle_t led_strip;
static std::shared_ptr<WiFiManager> wifi_manager;

static void led_init()
{
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = GPIO_NUM_38;
    strip_config.max_leds = 1;
    strip_config.led_model = LED_MODEL_WS2812;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = 10 * 1000 * 1000; // 10 MHz

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}

static void led_set(uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);
}

static void led_off()
{
    led_strip_clear(led_strip);
}

extern "C" void app_main()
{
    ESP_LOGI(TAG, "WiFi Connect Test");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    led_init();

    // Initialize EventBus
    auto event_bus = std::make_shared<EventBus>();
    if (!event_bus->init()) {
        ESP_LOGE(TAG, "Failed to initialize EventBus!");
        return;
    }

    // Initialize WiFiManager (will auto-connect if credentials exist in NVS)
    wifi_manager = std::make_shared<WiFiManager>(event_bus);
    if (!wifi_manager->init()) {
        ESP_LOGE(TAG, "Failed to initialize WiFiManager!");
        return;
    }
    ESP_LOGI(TAG, "WiFiManager initialized, waiting for connection...");

    // LED loop: blink blue while disconnected, solid green when connected
    //
    while (1) {
        if (wifi_manager->isConnected()) {
            led_set(16, 0, 0); // green
            vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            led_set(0, 0, 16); // blue
            vTaskDelay(pdMS_TO_TICKS(200));
            led_off();
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}