#ifndef DISPENSE_CONTROL_H
#define DISPENSE_CONTROL_H

/**
 * @file DispenseControl.h
 * @brief Food dispensing controller.
 *
 * REQUESTING A FEED
 *   Publish EVENT_FEED_REQUEST with a feed_request_t on the EventBus.
 *   Any component (UI, MQTT, scheduler) can trigger a feed this way.
 *   Concurrent requests are rejected with FEED_STATUS_BUSY.
 *
 * RECEIVING THE RESULT
 *   There are two mechanisms, intended for different callers:
 *
 *   1. EventBus broadcast (for observers)
 *      EVENT_FEED_COMPLETED or EVENT_FEED_FAILED is always published when a request finishes.
 *      Subscribe to these if you need to react to feeding outcomes without having initiated
 *      the request (e.g. UI, history log).
 *
 *   2. reply_queue (for the requester)
 *      Set feed_request_t::reply_queue to a FreeRTOS queue of length 1 and type feed_status_t.
 *      The result is written to the queue when the request finishes.
 *      Use this when you need to block and wait for the outcome (e.g. MQTT ACK, UI update).
 *      Leave reply_queue as nullptr if you do not need a direct response from queue.
 *
 *   A caller should use one mechanism, handling both is not necessary.
 *
 * ============================================================
 * USAGE
 * ============================================================
 *
 * --- Setup ---
 *
 *   DispenseControl::Config cfg = {
 *       .default_portions       = 1,     // fallback portion count
 *       .steps_per_portion      = 200,   // motor steps for one portion
 *       .motor_rpm              = 60,    // motor rotation speed
 *       .operation_timeout_ms   = 5000,  // abort if motor takes longer
 *       .portion_delay_ms       = 500,   // pause between portions
 *       .min_food_level_percent = 10,    // reserved for future use
 *       .motor_config           = { ... },
 *   };
 *
 *   food_level_sensor is for low-food checks before dispensing.
 *   weight_sensor is for calibration grams per portion.
 *
 *   ctrl->init();   // must be called before publishing any events
 *
 *   DispenseControl is designed to live for the entire lifetime. Destruction causes abort().
 *
 * --- Triggering a feed ---
 *
 *   Publish EVENT_FEED_REQUEST with a feed_request_t payload.
 *
 *   feed_request_t fields:
 *     .timestamp   — Unix timestamp of the request (uint32_t)
 *     .portions    — number of portions to dispense (1–10)
 *     .source      — who triggered the feed:
 *                      FEED_SOURCE_MANUAL
 *                      FEED_SOURCE_SCHEDULED
 *                      FEED_SOURCE_REMOTE
 *     .reply_queue — optional FreeRTOS queue (length must be 1).
 *
 * --- Triggering calibration ---
 *
 *   Publish EVENT_CALIBRATION_REQUEST (no payload).
 *   The controller will dispense 3 portions, read the weight, compute grams-per-portion, and store.
 *
 * --- Result events ---
 *
 *   EVENT_FEED_COMPLETED     — payload: feed_status_t
 *     .status_code     = FEED_STATUS_SUCCESS
 *     .portions        = number of portions dispensed
 *     .source          = original feed source
 *     .request_timestamp = original request timestamp
 *
 *   EVENT_FEED_FAILED        — payload: feed_status_t
 *     .status_code:
 *       FEED_STATUS_BUSY
 *       FEED_STATUS_TIMEOUT
 *       FEED_STATUS_INVALID_AMOUNT
 *       FEED_STATUS_UNKNOWN
 *
 *   EVENT_CALIBRATION_COMPLETED  — payload: calibration_result_t
 *     .grams_per_portion = measured grams per portion
 *   EVENT_CALIBRATION_FAILED     — no payload (busy or motor error)
 *
 * ============================================================
 * INTERNALS
 * ============================================================
 *
 * - A single FreeRTOS task pulls work items from a length-1 queue and executes them sequentially.
 *
 * - Concurrency guard: is_busy_ (atomic<bool>) is set before enqueuing and cleared after the
 * task finishes. Requests that arrive while busy are rejected immediately.
 */

#include "EventBus.h"
#include "StepperMotor.h"
#include "Types.h"
#include "esp_err.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <atomic>
#include <memory>

// forward declarations for dependencies
class FoodLevelSensor;
class WeightSensor;

class DispenseControl
{
  public:
    struct Config
    {
        uint8_t default_portions;
        uint32_t steps_per_portion;
        uint32_t motor_rpm;            // the speed of the motor in RPM
        uint32_t operation_timeout_ms; // the timeout for motor operation, in case of motor stuck
        uint32_t portion_delay_ms;     // delay between portions to allow food to drop
        uint8_t min_food_level_percent;
        StepperMotor::Config motor_config;
    };

    DispenseControl(std::shared_ptr<EventBus> event_bus, const Config& config,
                    std::shared_ptr<FoodLevelSensor> food_level_sensor = nullptr,
                    std::shared_ptr<WeightSensor> weight_sensor        = nullptr);

    ~DispenseControl();

    esp_err_t init();

  private:
    std::shared_ptr<EventBus> event_bus_;
    std::shared_ptr<StepperMotor> motor_;
    std::shared_ptr<FoodLevelSensor> food_level_sensor_;
    std::shared_ptr<WeightSensor> weight_sensor_;
    Config config_;

    enum class WorkType : uint8_t
    {
        FEED,
        CALIBRATE
    };

    struct WorkItem
    {
        WorkType type{WorkType::FEED};
        feed_request_t feed_request{};
    };

    QueueHandle_t work_queue_;
    TaskHandle_t worker_task_handler_;
    std::atomic<bool> is_busy_{false};

    static const char* TAG;
    static constexpr uint8_t DIRECTION_DISPENSE    = 1;
    static constexpr uint8_t MIN_PORTIONS          = 1;
    static constexpr uint8_t MAX_PORTIONS          = 10;
    static constexpr UBaseType_t WORK_QUEUE_LENGTH = 1;
    static constexpr uint8_t CALIBRATION_PORTIONS  = 3;

    // callback for EventBus
    static void onFeedRequestEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
    static void onCalibrationRequestEvent(void* arg, esp_event_base_t base, int32_t id, void* data);

    // worker task
    static void workerTask(void* arg);

    esp_err_t handleFeedRequest(const feed_request_t& request);
    esp_err_t executeDispense(uint8_t portions);
    void executeCalibrate();

    // publish event for UI, and reply to
    void handleFeedCompleted(const feed_request_t& request);
    void handleFeedFailed(const feed_request_t& request, feed_status_code_t status_code);

    void requestFeedingRecordSave(const feed_request_t& request);

    float grams_per_portion_{20.0f}; // default value

    esp_err_t requestGramsPerPortionSave();
    void loadGramsPerPortionFromNVS();
};

#endif // DISPENSE_CONTROL_H
