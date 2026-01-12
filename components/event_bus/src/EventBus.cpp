#include "EventBus.h"
#include "esp_log.h"

const char* EventBus::TAG = "EventBus";

// Define the event base
ESP_EVENT_DEFINE_BASE(PET_FEEDER_EVENTS);

EventBus::EventBus()
    : event_loop(nullptr)
{
}

EventBus::~EventBus()
{
    if (event_loop) {
        esp_event_loop_delete(event_loop);
    }
}

bool EventBus::init()
{
    esp_event_loop_args_t loop_args = {
        .queue_size = 32,              // Can hold up to 32 events
        .task_name = "event_bus",      // Task name for debugging
        .task_priority = 5,            // Medium priority
        .task_stack_size = 4096,       // 4KB stack
        .task_core_id = tskNO_AFFINITY // Can run on any core
    };
    
    esp_err_t err = esp_event_loop_create(&loop_args, &event_loop);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "EventBus initialized successfully");
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(err));
        return false;
    }
}

esp_err_t EventBus::subscribe(int32_t event_id, esp_event_handler_t handler, void* handler_arg)
{
    if (!event_loop) {
        ESP_LOGE(TAG, "EventBus not initialized!");
        return ESP_FAIL;
    }
    
    esp_err_t err = esp_event_handler_instance_register_with(
        event_loop,
        PET_FEEDER_EVENTS,
        event_id,
        handler,
        handler_arg,
        nullptr  // We don't need the instance handle for now
    );
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Handler registered for event %ld", event_id);
    } else {
        ESP_LOGE(TAG, "Failed to register handler for event %ld: %s", 
                 event_id, esp_err_to_name(err));
    }
    
    return err;
}

esp_err_t EventBus::unsubscribe(int32_t event_id, esp_event_handler_t handler)
{
    if (!event_loop) {
        ESP_LOGE(TAG, "EventBus not initialized!");
        return ESP_FAIL;
    }
    
    return esp_event_handler_unregister_with(
        event_loop,
        PET_FEEDER_EVENTS,
        event_id,
        handler
    );
}

esp_err_t EventBus::publish(int32_t event_id)
{
    return publish(event_id, nullptr, 0);
}

//publish with data
esp_err_t EventBus::publish(int32_t event_id, const void* data, size_t data_size)
{
    if (!event_loop) {
        ESP_LOGE(TAG, "EventBus not initialized!");
        return ESP_FAIL;
    }
    
    esp_err_t err = esp_event_post_to(
        event_loop,
        PET_FEEDER_EVENTS,
        event_id,
        data,
        data_size,
        portMAX_DELAY  // Wait forever if queue is full
    );
    
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "Event %ld posted", event_id);
    } else {
        ESP_LOGE(TAG, "Failed to post event %ld: %s", 
                 event_id, esp_err_to_name(err));
    }
    
    return err;
}