#include "DispenseControl.h"

#include "Events.h"
#include "StringHelpers.h"
#include "esp_log.h"
#include "nvs.h"

#include <inttypes.h>

#include <cstdlib>
#include <cstring>
#include <utility>

const char* DispenseControl::TAG = "DispenseControl";

namespace
{
void sendDispenseResult(const feed_request_t& request, const feed_status_t& status)
{
    if (!request.reply_queue)
    {
        return;
    }

    // xQueueOverwrite ensures delivery even if the caller missed a previous result.
    // NOTE: This requires reply_queue to have length == 1.
    xQueueOverwrite(request.reply_queue, &status);
}
} // namespace

DispenseControl::DispenseControl(std::shared_ptr<EventBus> event_bus, const Config& config,
                                 std::shared_ptr<FoodLevelSensor> food_level_sensor,
                                 std::shared_ptr<WeightSensor> weight_sensor)
    : event_bus_(std::move(event_bus)), food_level_sensor_(std::move(food_level_sensor)),
      weight_sensor_(std::move(weight_sensor)), config_(config), work_queue_(nullptr),
      worker_task_handler_(nullptr)
{
    motor_ = std::make_shared<StepperMotor5V>(config_.motor_config);
}

DispenseControl::~DispenseControl()
{
    ESP_LOGE(TAG, "DispenseControl must not be destructed in runtime. Abort.");
    abort();
}

esp_err_t DispenseControl::init()
{
    // create work queue
    work_queue_ = xQueueCreate(WORK_QUEUE_LENGTH, sizeof(WorkItem));
    if (!work_queue_)
    {
        ESP_LOGE(TAG, "Failed to create work queue");
        return ESP_ERR_NO_MEM;
    }

    // init motor
    esp_err_t ret = motor_->init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize motor: %s", esp_err_to_name(ret));
        return ret;
    }

    loadGramsPerPortionFromNVS();

    // subscribe to the event bus
    ret = event_bus_->subscribe(EVENT_FEED_REQUEST, onFeedRequestEvent, this);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to subscribe to feed events: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = event_bus_->subscribe(EVENT_CALIBRATION_REQUEST, onCalibrationRequestEvent, this);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to subscribe to calibration events: %s", esp_err_to_name(ret));
        return ret;
    }

    BaseType_t task_ret =
        xTaskCreate(workerTask, "dispense_worker", 4096, this, 5, &worker_task_handler_);
    if (task_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create worker task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG,
             "DispenseControl initialized. RPM=%" PRIu32 ", steps/portion=%" PRIu32
             ", default portions=%" PRIu32,
             config_.motor_rpm, config_.steps_per_portion,
             static_cast<uint32_t>(config_.default_portions));

    return ESP_OK;
}

void DispenseControl::onFeedRequestEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    auto* controller = static_cast<DispenseControl*>(arg);

    if (!data)
    {
        ESP_LOGE(TAG, "Received feed request without data");
        return;
    }

    auto* request = static_cast<feed_request_t*>(data);

    ESP_LOGI(TAG, "Feed request received. Source: %s. Portions: %" PRIu32,
             feed_source_to_string(request->source), static_cast<uint32_t>(request->portions));

    bool expected = false;
    if (!controller->is_busy_.compare_exchange_strong(expected, true, std::memory_order_acquire,
                                                      std::memory_order_relaxed))
    {
        ESP_LOGW(TAG, "Dispenser busy, rejecting request");
        controller->handleFeedFailed(*request, FEED_STATUS_BUSY);
        return;
    }

    WorkItem work     = {};
    work.type         = WorkType::FEED;
    work.feed_request = *request;
    if (xQueueSend(controller->work_queue_, &work, 0) != pdPASS)
    {
        // This should never happen. If so, some code path is sending to the queue without going
        // through the guard.
        configASSERT(false);
        controller->is_busy_.store(false, std::memory_order_release);
        ESP_LOGE(TAG, "Queue full while is_busy_ was false");
        controller->handleFeedFailed(*request, FEED_STATUS_BUSY);
    }
}

void DispenseControl::onCalibrationRequestEvent(void* arg, esp_event_base_t base, int32_t id,
                                                void* data)
{
    auto* controller = static_cast<DispenseControl*>(arg);

    ESP_LOGI(TAG, "Calibration request received");

    bool expected = false;
    if (!controller->is_busy_.compare_exchange_strong(expected, true, std::memory_order_acquire,
                                                      std::memory_order_relaxed))
    {
        ESP_LOGW(TAG, "Dispenser busy, rejecting calibration request");
        controller->event_bus_->publish(EVENT_CALIBRATION_FAILED);
        return;
    }

    WorkItem work = {};
    work.type     = WorkType::CALIBRATE;
    if (xQueueSend(controller->work_queue_, &work, 0) != pdPASS)
    {
        configASSERT(false);
        controller->is_busy_.store(false, std::memory_order_release);
        ESP_LOGE(TAG, "Queue full while is_busy_ was false");
        controller->event_bus_->publish(EVENT_CALIBRATION_FAILED);
    }
}

void DispenseControl::workerTask(void* arg)
{
    auto* self = static_cast<DispenseControl*>(arg);

    while (true)
    {
        WorkItem work = {};
        xQueueReceive(self->work_queue_, &work, portMAX_DELAY);
        switch (work.type)
        {
            case WorkType::FEED:
                self->handleFeedRequest(work.feed_request);
                break;
            case WorkType::CALIBRATE:
                self->executeCalibrate();
                break;
        }
        self->is_busy_.store(false, std::memory_order_release);
    }
}

esp_err_t DispenseControl::handleFeedRequest(const feed_request_t& request)
{
    // validate portions
    if (request.portions < MIN_PORTIONS || request.portions > MAX_PORTIONS)
    {
        ESP_LOGE(TAG, "Invalid portions: %" PRIu32 " (range: %" PRIu32 "-%" PRIu32 ")",
                 static_cast<uint32_t>(request.portions), static_cast<uint32_t>(MIN_PORTIONS),
                 static_cast<uint32_t>(MAX_PORTIONS));
        handleFeedFailed(request, FEED_STATUS_INVALID_AMOUNT);
        return ESP_ERR_INVALID_ARG;
    }

    // todo: do we need to check food level before feeding?

    esp_err_t result = executeDispense(request.portions);

    // check the result, publish and save record accordingly
    if (result == ESP_OK)
    {
        handleFeedCompleted(request);
        requestFeedingRecordSave(request);
        ESP_LOGI(TAG, "Dispense completed successfully. %" PRIu32 " portions from %s",
                 static_cast<uint32_t>(request.portions), feed_source_to_string(request.source));
    }
    else
    {
        // if error, determine the reason and publish
        feed_status_code_t status_code =
            (result == ESP_ERR_TIMEOUT) ? FEED_STATUS_TIMEOUT : FEED_STATUS_UNKNOWN;
        handleFeedFailed(request, status_code);
        ESP_LOGE(TAG, "Dispense failed. Error: %s", esp_err_to_name(result));
    }

    return result;
}

esp_err_t DispenseControl::executeDispense(uint8_t portions)
{
    ESP_LOGI(TAG, "Dispensing %" PRIu32 " portions", static_cast<uint32_t>(portions));

    motor_->wake();
    for (int i = 0; i < portions; i++)
    {
        esp_err_t ret = motor_->move(config_.steps_per_portion, DIRECTION_DISPENSE,
                                     config_.motor_rpm, config_.operation_timeout_ms);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Motor movement failed: %s", esp_err_to_name(ret));
            motor_->sleep();
            return ret;
        }

        vTaskDelay(pdMS_TO_TICKS(config_.portion_delay_ms));
    }
    motor_->sleep();

    return ESP_OK;
}

void DispenseControl::handleFeedCompleted(const feed_request_t& request)
{
    feed_status_t status     = {};
    status.status_code       = FEED_STATUS_SUCCESS;
    status.portions          = request.portions;
    status.source            = request.source;
    status.request_timestamp = request.timestamp;

    event_bus_->publish(EVENT_FEED_COMPLETED, status);

    sendDispenseResult(request, status);

    ESP_LOGI(TAG, "Feed completed. %s, %" PRIu32 " portions", feed_source_to_string(request.source),
             static_cast<uint32_t>(request.portions));
}

void DispenseControl::handleFeedFailed(const feed_request_t& request,
                                       feed_status_code_t status_code)
{
    feed_status_t status     = {};
    status.status_code       = status_code;
    status.portions          = request.portions;
    status.source            = request.source;
    status.request_timestamp = request.timestamp;

    event_bus_->publish(EVENT_FEED_FAILED, status);

    sendDispenseResult(request, status);

    ESP_LOGW(TAG, "Feed failed - %s, portions=%" PRIu32 ": %s",
             feed_source_to_string(request.source), static_cast<uint32_t>(request.portions),
             feed_status_to_string(status_code));
}

void DispenseControl::requestFeedingRecordSave(const feed_request_t& request)
{
    event_bus_->publish(EVENT_SAVE_FEEDING_RECORD, request);

    ESP_LOGI(TAG, "Feeding record saved - %" PRIu32 " portions, source: %s",
             static_cast<uint32_t>(request.portions), feed_source_to_string(request.source));
}

void DispenseControl::executeCalibrate()
{
    ESP_LOGI(TAG, "Starting calibration with %d portions", CALIBRATION_PORTIONS);

    if (!weight_sensor_)
    {
        ESP_LOGE(TAG, "Weight sensor not available, cannot calibrate");
        event_bus_->publish(EVENT_CALIBRATION_FAILED);
        return;
    }

    // TODO: weight_sensor_->tare()

    motor_->wake();
    for (int i = 0; i < CALIBRATION_PORTIONS; i++)
    {
        esp_err_t ret = motor_->move(config_.steps_per_portion, DIRECTION_DISPENSE,
                                     config_.motor_rpm, config_.operation_timeout_ms);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Motor movement failed during calibration: %s", esp_err_to_name(ret));
            motor_->sleep();
            event_bus_->publish(EVENT_CALIBRATION_FAILED);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(config_.portion_delay_ms));
    }
    motor_->sleep();

    ESP_LOGI(TAG, "Waiting for food to settle...");
    vTaskDelay(pdMS_TO_TICKS(config_.portion_delay_ms));

    // TODO: weight_sensor_->readGrams(total_grams)
    float total_grams = 60.0f; // placeholder

    // float total_grams = 0.0f;
    // if (total_grams <= 0.0f) { publish FAILED; return; }
    // grams_per_portion_ = total_grams / CALIBRATION_PORTIONS;
    grams_per_portion_ = total_grams / CALIBRATION_PORTIONS;

    esp_err_t ret = requestGramsPerPortionSave();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save calibration result to NVS");
        event_bus_->publish(EVENT_CALIBRATION_FAILED);
        return;
    }

    calibration_result_t result = {.grams_per_portion = grams_per_portion_};
    event_bus_->publish(EVENT_CALIBRATION_COMPLETED, result);

    ESP_LOGI(TAG, "Calibration completed: %.2f g/portion", grams_per_portion_);
}

esp_err_t DispenseControl::requestGramsPerPortionSave()
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("dispense", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    // NVS has no float type; store as raw bytes via u32
    uint32_t raw = 0;
    memcpy(&raw, &grams_per_portion_, sizeof(float));
    ret = nvs_set_u32(nvs_handle, "grams_per_portion", raw);
    if (ret == ESP_OK)
    {
        ret = nvs_commit(nvs_handle);
        ESP_LOGI(TAG, "Saved grams_per_portion to NVS: %.2f g", grams_per_portion_);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to save to NVS: %s", esp_err_to_name(ret));
    }

    nvs_close(nvs_handle);
    return ret;
}

void DispenseControl::loadGramsPerPortionFromNVS()
{
    nvs_handle_t nvs_handle;
    if (nvs_open("dispense", NVS_READONLY, &nvs_handle) != ESP_OK)
    {
        ESP_LOGW(TAG, "No NVS data found, grams_per_portion defaults to 0");
        return;
    }

    uint32_t raw = 0;
    if (nvs_get_u32(nvs_handle, "grams_per_portion", &raw) == ESP_OK)
    {
        memcpy(&grams_per_portion_, &raw, sizeof(float));
        ESP_LOGI(TAG, "Loaded grams_per_portion from NVS: %.2f g", grams_per_portion_);
    }

    nvs_close(nvs_handle);
}
