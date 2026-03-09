#include "Scheduler.h"
#include "esp_log.h"
#include "nvs.h"
#include <cstring>
#include <ctime>

static const char* TAG = "Scheduler";

Scheduler::Scheduler(std::shared_ptr<EventBus> event_bus)
    : event_bus(event_bus), schedules{}, mutex(nullptr), task_handle(nullptr)
{
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        last_fired[i] = -1;
    }
}

Scheduler::~Scheduler()
{
    if (task_handle) vTaskDelete(task_handle);
    if (mutex) vSemaphoreDelete(mutex);
}

esp_err_t Scheduler::init()
{
    mutex = xSemaphoreCreateMutex();
    if (!mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    loadFromNVS();

    esp_err_t ret = event_bus->subscribe(EVENT_SCHEDULE_UPDATE, onScheduleUpdateEvent, this);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to subscribe to schedule update event");
        return ret;
    }

    BaseType_t task_ret = xTaskCreate(taskEntry, "scheduler", 4096, this, 5, &task_handle);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create scheduler task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Scheduler initialized with %d schedules", schedules.count);
    return ESP_OK;
}

void Scheduler::taskEntry(void* arg)
{
    static_cast<Scheduler*>(arg)->taskLoop();
}

void Scheduler::taskLoop()
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        checkAndDispense();
        if (nvs_flag) {
            saveToNVS();
            nvs_flag = false;
        }
    }
}

void Scheduler::checkAndDispense()
{
    // time() returns seconds since epoch.
    // If less than a billion, SNTP hasn't synced yet.
    time_t now = time(NULL);
    if (now < 1000000000) {
        ESP_LOGD(TAG, "Time not synced yet, skipping check");
        return;
    }

    // Copy schedules under mutex, release before publishing events
    schedule_list_t local;
    xSemaphoreTake(mutex, portMAX_DELAY);
    local = schedules;
    xSemaphoreGive(mutex);

    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    int utc_minutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int current_minute_of_day = (utc_minutes + local.utc_offset_minutes % (24 * 60) + 24 * 60) % (24 * 60);

    for (int i = 0; i < local.count; i++) {
        int schedule_minute = local.schedules[i].hour * 60 + local.schedules[i].minute;

        if (current_minute_of_day == schedule_minute && last_fired[i] != current_minute_of_day) {
            last_fired[i] = current_minute_of_day;

            feed_request_t request = {};
            request.portions = local.schedules[i].portions;
            request.source   = FEED_SOURCE_SCHEDULED;
            event_bus->publish(EVENT_FEED_REQUEST, request);

            ESP_LOGI(TAG, "Scheduled feed: %02d:%02d, %d portions",
                     local.schedules[i].hour, local.schedules[i].minute, local.schedules[i].portions);
        }
    }
}

void Scheduler::onScheduleUpdateEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    Scheduler* self = static_cast<Scheduler*>(arg);
    if (!data) return;

    schedule_list_t* new_schedules = static_cast<schedule_list_t*>(data);

    xSemaphoreTake(self->mutex, portMAX_DELAY);
    self->schedules = *new_schedules;
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        self->last_fired[i] = -1;
    }
    self->nvs_flag = true;
    xSemaphoreGive(self->mutex);

    ESP_LOGI(TAG, "Schedules updated: %d entries", new_schedules->count);
}

void Scheduler::saveToNVS()
{
    schedule_list_t local;
    xSemaphoreTake(mutex, portMAX_DELAY);
    local = schedules;
    xSemaphoreGive(mutex);

    nvs_handle_t handle;
    esp_err_t ret = nvs_open("scheduler", NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return;
    }

    ret = nvs_set_blob(handle, "schedules", &local, sizeof(local));
    if (ret == ESP_OK) {
        nvs_commit(handle);
        ESP_LOGI(TAG, "Schedules saved to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to save schedules: %s", esp_err_to_name(ret));
    }

    nvs_close(handle);
}

void Scheduler::loadFromNVS()
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open("scheduler", NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No saved schedules found, starting empty");
        return;
    }

    size_t size = sizeof(schedules);
    ret = nvs_get_blob(handle, "schedules", &schedules, &size);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Loaded %d schedules from NVS", schedules.count);
    } else {
        ESP_LOGW(TAG, "Failed to load schedules: %s", esp_err_to_name(ret));
    }

    nvs_close(handle);
}
