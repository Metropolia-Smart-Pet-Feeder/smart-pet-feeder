#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "EventBus.h"
#include "Events.h"
#include "Types.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include <memory>

class Scheduler
{
public:
    explicit Scheduler(std::shared_ptr<EventBus> event_bus);
    ~Scheduler();

    esp_err_t init();

private:
    std::shared_ptr<EventBus> event_bus;
    schedule_list_t schedules;
    SemaphoreHandle_t mutex;
    TaskHandle_t task_handle;
    int last_fired[MAX_SCHEDULES];  // minute-of-day when each schedule last fired, -1 = never
    bool nvs_flag{false};

    static void onScheduleUpdateEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
    static void taskEntry(void* arg);

    void taskLoop();
    void checkAndDispense();
    void saveToNVS();
    void loadFromNVS();
};

#endif
