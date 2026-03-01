#ifndef EVENTS_H
#define EVENTS_H

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

    // Feeding events
    EVENT_FEED_REQUEST,
    EVENT_FEED_COMPLETED,
    EVENT_FEED_FAILED,

    // Calibration events
    EVENT_CALIBRATION_REQUEST,
    EVENT_CALIBRATION_COMPLETED,
    EVENT_CALIBRATION_FAILED,

    // Food level events
    // EVENT_FOOD_LEVEL_CHANGED,

    // cat approaching event

    // rfid event

    // weight sensor event

    // Schedule events
    EVENT_SCHEDULE_UPDATE,

    // data storage events
    EVENT_SAVE_FEEDING_RECORD,
    EVENT_SAVE_VISITS_RECORD,
    EVENT_SAVE_EATING_RECORD
};

#endif // EVENTS_H
