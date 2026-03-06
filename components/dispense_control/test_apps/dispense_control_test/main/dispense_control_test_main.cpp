/**
 * DispenseControl Test App
 *
 * Three tasks simulate the three callers:
 *   UI        - no reply_queue, result via EventBus callbacks
 *   MQTT      - own task, blocks on reply_queue
 *   Scheduler - own task, blocks on reply_queue
 *
 */

#include "DispenseControl.h"
#include "EventBus.h"
#include "Events.h"
#include "Types.h"
#include "StringHelpers.h"
#include "board_config.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <memory>

static const char* TAG = "DispenseControlTest";

// an internal event bus
static EventBus* g_event_bus = nullptr;

// UI task, uses EventBus callbacks to receive results

static void onFeedCompletedForUI(void*, esp_event_base_t, int32_t, void* data)
{
    auto* s = static_cast<feed_status_t*>(data);
    ESP_LOGI("UI", "Feed completed, source=%-9s, %u portion(s), status=%s",
             feed_source_to_string(s->source),
             static_cast<uint8_t>(s->portions),
             feed_status_to_string(s->status_code));
}

static void onFeedFailedForUI(void*, esp_event_base_t, int32_t, void* data)
{
    auto* s = static_cast<feed_status_t*>(data);
    ESP_LOGW("UI", "Feed failed, source=%-9s, reason=%s",
             feed_source_to_string(s->source),
             feed_status_to_string(s->status_code));
}

static void uiTask(void*)
{
    g_event_bus->subscribe(EVENT_FEED_COMPLETED, onFeedCompletedForUI, nullptr);
    g_event_bus->subscribe(EVENT_FEED_FAILED, onFeedFailedForUI, nullptr);

    vTaskDelay(pdMS_TO_TICKS(1000));

    while (true)
    {
        ESP_LOGI("UI", "Feed Now button pressed");

        feed_request_t req = {};
        req.portions = 1;
        req.timestamp = esp_log_timestamp();
        req.source = FEED_SOURCE_MANUAL;
        // No reply_queue, result arrives via EventBus callbacks above

        g_event_bus->publish(EVENT_FEED_REQUEST, req);

        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}

// MQTT task, Has its own task, can block on reply_queue and report result

static void mqttTask(void*)
{
    QueueHandle_t reply = xQueueCreate(1, sizeof(feed_status_t));

    vTaskDelay(pdMS_TO_TICKS(5000));

    while (true)
    {
        ESP_LOGI("MQTT", "Received feed command from server (2 portions)");

        feed_request_t req = {};
        req.portions = 2;
        req.timestamp = esp_log_timestamp();
        req.source = FEED_SOURCE_REMOTE;
        req.reply_queue = reply;

        g_event_bus->publish(EVENT_FEED_REQUEST, req);

        feed_status_t result;
        if (xQueueReceive(reply, &result, pdMS_TO_TICKS(35000)) == pdTRUE)
        {
            ESP_LOGI("MQTT", "Reporting to server: status=%s, portions=%u",
                     feed_status_to_string(result.status_code),
                     static_cast<uint8_t>(result.portions));
        }
        else
        {
            ESP_LOGE("MQTT", "Timeout waiting for dispenser, no response sent to server");
        }

        vTaskDelay(pdMS_TO_TICKS(20000));
    }
}

// Scheduled  task. Has its own task, waits for result before updating next scheduled time

static void schedulerTask(void*)
{
    QueueHandle_t reply = xQueueCreate(1, sizeof(feed_status_t));

    vTaskDelay(pdMS_TO_TICKS(10000));

    while (true)
    {
        ESP_LOGI("Scheduler", "Scheduled feeding triggered (1 portion)");

        feed_request_t req = {};
        req.portions = 1;
        req.timestamp = esp_log_timestamp();
        req.source = FEED_SOURCE_SCHEDULED;
        req.reply_queue = reply;

        g_event_bus->publish(EVENT_FEED_REQUEST, req);

        feed_status_t result;
        if (xQueueReceive(reply, &result, pdMS_TO_TICKS(35000)) == pdTRUE)
        {
            if (result.status_code == FEED_STATUS_SUCCESS)
            {
                ESP_LOGI("Scheduler", "Feeding done, updating next scheduled time");
            }
            else
            {
                ESP_LOGW("Scheduler", "Feeding failed with error %s", feed_status_to_string(result.status_code));
            }
        }
        else
        {
            ESP_LOGE("Scheduler", "Timeout, dispenser may be stuck");
        }

        vTaskDelay(pdMS_TO_TICKS(25000));
    }
}

// Entry point

extern "C" void app_main()
{
    ESP_LOGI(TAG, "DispenseControl Test App");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    auto event_bus = std::make_shared<EventBus>();
    if (!event_bus->init())
    {
        ESP_LOGE(TAG, "EventBus init failed");
        return;
    }
    g_event_bus = event_bus.get();

    DispenseControl::Config config = {
        .default_portions = 1,
        .steps_per_portion = 100,
        .motor_rpm = 20,
        .operation_timeout_ms = 10000,
        .portion_delay_ms = 1000,
        .min_food_level_percent = 0,
        .motor_config = {
            .step_pin = BoardConfig::MOTOR_STEP,
            .dir_pin = BoardConfig::MOTOR_DIR,
            .sleep_pin = BoardConfig::MOTOR_SLEEP,
            .steps_per_rev = 400,
            .rmt_resolution_hz = 1000000
        }
    };

    auto dispense = std::make_shared<DispenseControl>(event_bus, config);
    ESP_ERROR_CHECK(dispense->init());

    xTaskCreate(uiTask, "ui_task", 4096, nullptr, 3, nullptr);
    xTaskCreate(mqttTask, "mqtt_task", 4096, nullptr, 3, nullptr);
    xTaskCreate(schedulerTask, "scheduler_task", 4096, nullptr, 3, nullptr);

    ESP_LOGI(TAG, "All tasks started");

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
