#ifndef EVENTBUS_H
#define EVENTBUS_H

#pragma once

#include "Events.h"
#include "esp_event.h"

class EventBus
{
public:
    EventBus();
    ~EventBus();
    
    bool init();
    
    esp_err_t subscribe(int32_t event_id, esp_event_handler_t handler, void* handler_arg = nullptr);
    esp_err_t unsubscribe(int32_t event_id, esp_event_handler_t handler);
    
    // Publish event without data
    esp_err_t publish(int32_t event_id);
    // Publish event with data
    esp_err_t publish(int32_t event_id, const void* data, size_t data_size);
    
    // Type-safe helper
    template<typename T>
    esp_err_t publish(int32_t event_id, const T& data) {
        return publish(event_id, &data, sizeof(T));
    }
    

private:
    esp_event_loop_handle_t event_loop;
    static const char* TAG;

};

#endif