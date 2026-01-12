#ifndef EVENTS_H
#define EVENTS_H

#include <cstdint>
#include <cstddef>
#include "esp_event.h"

// declare custom event base
ESP_EVENT_DECLARE_BASE(PET_FEEDER_EVENTS);

// all event types in the pet feeder system
enum {
    // WiFi events
    EVENT_WIFI_CONNECTED,
    EVENT_WIFI_DISCONNECTED,
    
    // wifi provisioning events
    EVENT_START_PROVISIONING,
    EVENT_PROVISIONING_STARTED,
    EVENT_RESET_CREDENTIALS,
    
    // Feeding events
    EVENT_FEED_NOW_CLICKED,
    
    // // Schedule events
    // EVENT_SCHEDULE_UPDATED,
    // EVENT_SCHEDULE_SAVED,
    // EVENT_SCHEDULE_DELETED,
    
    // // Food level events
    // EVENT_FOOD_LEVEL_CHANGED,
    // EVENT_FOOD_LOW_WARNING,
    // EVENT_FOOD_EMPTY,
    
};


// data structs
struct wifi_event_data_t {
    char ssid[33];
};

struct provisioning_event_data_t {
    char device_name[33];
};

struct schedule_data_t {
    uint8_t hour;
    uint8_t minute;
    uint16_t amount_grams;
};

struct feeding_data_t {
    uint16_t amount_grams;
    bool is_scheduled;
    uint32_t timestamp;
    char reason[32];  // "manual", "scheduled", "remote"
};

struct food_level_data_t {
    uint8_t percentage;
    float weight_kg;
    uint32_t timestamp;
};

#endif 