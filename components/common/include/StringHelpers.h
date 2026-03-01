#ifndef STRING_HELPERS_H
#define STRING_HELPERS_H

#include "Types.h"

/**
 * Convert feed source to string for logging
 */
inline const char* feed_source_to_string(feed_source_t source)
{
    switch (source)
    {
        case FEED_SOURCE_MANUAL:
            return "manual";
        case FEED_SOURCE_SCHEDULED:
            return "scheduled";
        case FEED_SOURCE_REMOTE:
            return "remote";
        default:
            return "unknown";
    }
}

/**
 * Convert feed status code for logging
 */
inline const char* feed_status_to_string(feed_status_code_t status)
{
    switch (status)
    {
        case FEED_STATUS_SUCCESS:
            return "success";
        case FEED_STATUS_TIMEOUT:
            return "timeout";
        case FEED_STATUS_UNKNOWN:
            return "unknown";
        case FEED_STATUS_BUSY:
            return "busy";
        case FEED_STATUS_LOW_FOOD:
            return "low food";
        case FEED_STATUS_INVALID_AMOUNT:
            return "invalid amount";
        default:
            return "unknown";
    }
}

/**
 * Convert feed status code to user-friendly error message.
 */
inline const char* feed_status_to_user_message(feed_status_code_t status)
{
    switch (status)
    {
        case FEED_STATUS_BUSY:
            return "System busy, please try again";
        case FEED_STATUS_TIMEOUT:
            return "Motor timeout, please check";
        case FEED_STATUS_LOW_FOOD:
            return "Food level too low";
        case FEED_STATUS_INVALID_AMOUNT:
            return "Invalid portions";
        default:
            return "Unknown error";
    }
}

#endif // STRING_HELPERS_H
