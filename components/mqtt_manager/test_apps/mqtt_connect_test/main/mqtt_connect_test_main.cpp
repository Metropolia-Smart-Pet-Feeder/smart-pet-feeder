/**
 * Component test: MQTTManager connect, publish, subscribe
 *
 * Test contents:
 *   1. Initialize NVS, EventBus, WiFiManager, MQTTManager
 *   2. WiFi auto-connects (credentials must already be provisioned)
 *   3. MQTT auto-connects on WiFi connected event
 *   4. Subscribe to test topic, publish a message, verify loopback
 *
 * Prerequisites:
 *   - WiFi credentials already provisioned in NVS
 *     (run the ui_wifi_provisioning test first if Wi-Fi credentials are not saved in NVS)
 *
 * Steps:
 *   1. idf.py build
 *   2. idf.py flash monitor
 *   3. Observe serial log for MQTT connection and message loopback
 */

#include "esp_log.h"
#include "nvs_flash.h"
#include "EventBus.h"
#include "Events.h"
#include "WiFiManager.h"
#include "MQTTManager.h"
#include "hivemq_ca.h"
#include <memory>

static const char* TAG = "mqtt_test";

static const char* TEST_TOPIC = "petfeeder/test/loopback";
static const char* TEST_MESSAGE = "hello from esp32 mqtt test";

static std::shared_ptr<MQTTManager> g_mqtt;
static bool g_mqtt_connected = false;

static void onMQTTConnected(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    auto* wifi_data = static_cast<wifi_event_data_t*>(data);
    ESP_LOGI(TAG, "[EVENT] WiFi connected - SSID: %s", wifi_data->ssid);
    ESP_LOGI(TAG, "Waiting for MQTT auto-connect...");
}

static void onMQTTMessageReceived(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    auto* msg = static_cast<mqtt_message_data_t*>(data);
    ESP_LOGI(TAG, "[EVENT] MQTT message received - Topic: %s, Data: %s", msg->topic, msg->data);

    if (strcmp(msg->topic, TEST_TOPIC) == 0 && strcmp(msg->data, TEST_MESSAGE) == 0) {
        ESP_LOGI(TAG, "PASS: Loopback message matches!");
    } else {
        ESP_LOGW(TAG, "Received message on unexpected topic or with unexpected data");
    }
}

static void onWiFiDisconnected(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    ESP_LOGW(TAG, "[EVENT] WiFi disconnected");
    g_mqtt_connected = false;
}

extern "C" void app_main()
{
    ESP_LOGI(TAG, "=== MQTT Manager Component Test ===");

    // Initialize NVS
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
        ESP_LOGE(TAG, "FAIL: EventBus init failed");
        return;
    }
    ESP_LOGI(TAG, "EventBus initialized");

    // Subscribe logging callbacks
    event_bus->subscribe(EVENT_WIFI_CONNECTED, onMQTTConnected, nullptr);
    event_bus->subscribe(EVENT_WIFI_DISCONNECTED, onWiFiDisconnected, nullptr);
    event_bus->subscribe(EVENT_MQTT_MESSAGE_RECEIVED, onMQTTMessageReceived, nullptr);

    // Initialize MQTT Manager (will auto-connect when WiFi connects)
    MQTTManager::Config mqtt_config = {
        .broker_uri = "mqtts://455307a48c6b4ea3b9252d0bd25ff836.s1.eu.hivemq.cloud:8883",
        .client_id = "esp32_petfeeder_test",
        .username = "feeder0",
        .password = "Admin000",
        .cert_pem = hivemq_root_ca_pem
    };

    g_mqtt = std::make_shared<MQTTManager>(event_bus, mqtt_config);
    if (!g_mqtt->init()) {
        ESP_LOGE(TAG, "FAIL: MQTTManager init failed");
        return;
    }
    ESP_LOGI(TAG, "MQTTManager initialized");

    // Initialize WiFiManager (will auto-connect if credentials exist in NVS)
    auto wifi_manager = std::make_shared<WiFiManager>(event_bus);
    if (!wifi_manager->init()) {
        ESP_LOGE(TAG, "FAIL: WiFiManager init failed");
        return;
    }
    ESP_LOGI(TAG, "WiFiManager initialized - waiting for WiFi connection...");

    // Main loop: once MQTT is connected, run the publish/subscribe test
    bool test_executed = false;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (!test_executed && g_mqtt->isConnected()) {
            if (!g_mqtt_connected) {
                g_mqtt_connected = true;
                ESP_LOGI(TAG, "MQTT connected to broker");

                // Subscribe to test topic
                if (g_mqtt->subscribe(TEST_TOPIC, 1)) {
                    ESP_LOGI(TAG, "Subscribed to: %s", TEST_TOPIC);
                } else {
                    ESP_LOGE(TAG, "FAIL: Subscribe failed");
                }

                // Give broker time to process subscription
                vTaskDelay(pdMS_TO_TICKS(1000));

                // Publish test message (should loop back via subscription)
                if (g_mqtt->publish(TEST_TOPIC, TEST_MESSAGE, 1)) {
                    ESP_LOGI(TAG, "Published to %s: %s", TEST_TOPIC, TEST_MESSAGE);
                } else {
                    ESP_LOGE(TAG, "FAIL: Publish failed");
                }

                test_executed = true;
                ESP_LOGI(TAG, "Test sequence complete. Check log for loopback message.");
            }
        }
    }
}
