#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include "freertos/queue.h"

// Feeding source
enum feed_source_t : uint8_t
{
    FEED_SOURCE_MANUAL    = 0,
    FEED_SOURCE_SCHEDULED = 1,
    FEED_SOURCE_REMOTE    = 2
};

// Feeding status codes
enum feed_status_code_t : uint8_t
{
    FEED_STATUS_SUCCESS        = 0,
    FEED_STATUS_TIMEOUT        = 1,
    FEED_STATUS_BUSY           = 2,
    FEED_STATUS_LOW_FOOD       = 3,
    FEED_STATUS_UNKNOWN        = 4,
    FEED_STATUS_INVALID_AMOUNT = 5
};

struct feed_request_t
{
    uint32_t timestamp;
    uint8_t portions;
    feed_source_t source;
    QueueHandle_t reply_queue = nullptr; // nullptr = no reply needed
};

struct feed_status_t
{
    feed_status_code_t status_code;
    uint8_t portions;
    feed_source_t source;
    uint32_t request_timestamp;
};

struct calibration_result_t
{
    float grams_per_portion;
};

struct schedule_data_t
{
    uint8_t hour;
    uint8_t minute;
    uint8_t portions;
};

static constexpr uint8_t MAX_SCHEDULES = 8;

struct schedule_list_t
{
    uint8_t count;
    schedule_data_t schedules[MAX_SCHEDULES];
    int16_t utc_offset_minutes;
};

struct visit_data_t
{
    uint32_t timestamp;
    char name[32];
};

struct wifi_event_data_t
{
    char ssid[33];
};

struct provisioning_event_data_t
{
    char device_name[33];
};

struct mqtt_message_data_t
{
    char topic[128];
    char data[256];
};

#endif // TYPES_H
