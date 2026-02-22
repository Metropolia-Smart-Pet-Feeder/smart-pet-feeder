#ifndef EVENTS_H
#define EVENTS_H

#include <cstdint>
#include <cstddef>
#include "esp_event.h"

// declare custom event base
ESP_EVENT_DECLARE_BASE(PET_FEEDER_EVENTS);

// all event types in the pet feeder system
enum {
    //ui events
    EVENT_FEED_NOW_PRESSED,

    // WiFi events
    EVENT_WIFI_CONNECTED,
    EVENT_WIFI_DISCONNECTED,
    
    // wifi provisioning events
    EVENT_START_PROVISIONING,
    EVENT_PROVISIONING_STARTED,
    EVENT_RESET_CREDENTIALS,
    
    // MQTT events
    EVENT_MQTT_MESSAGE_RECEIVED,
    
    // Feeding events
    EVENT_FEED,
    
    // Food level events
    EVENT_FOOD_LEVEL_CHANGED,

    // cat approaching event
    EVENT_CAT_APPROACHED,
    EVENT_CAT_LEFT,

    // rfid event
    EVENT_RFID_DETECTED,
    EVENT_RFID_REMOVED,

    // weight sensor event

    // data storage events
    EVENT_SAVE_FEEDING_RECORD,
    EVENT_SAVE_VISITS_RECORD,
    EVENT_SAVE_EATING_RECORD
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
    uint32_t timestamp;
    uint16_t amount_grams;
    char reason[32];  // "manual", "scheduled", "remote"
};

struct visit_data_t {
    uint32_t timestamp;
    char name[32];
};

struct eating_data_t {
    uint32_t timestamp;
    uint16_t amount_grams;
};

struct food_level_data_t {
    uint8_t percentage;
};

struct mqtt_message_data_t {
    char topic[128];
    char data[256];
};

#endif 