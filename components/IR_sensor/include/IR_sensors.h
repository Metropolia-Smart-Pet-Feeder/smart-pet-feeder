#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "EventBus.h"
#include <string>
#include <memory>

class IR_sensors {
    public:
        explicit IR_sensors(std::string name, gpio_num_t sensor_pin, bool alert_when_high, std::shared_ptr<EventBus> event_bus);
        ~IR_sensors();
        void start_monitoring();
        

    private:
        static void enter_task(void *arg);
        void monitor_sensors();

        std::string name;
        gpio_num_t sensor_pin;
        bool alert_when_high;
        std::shared_ptr<EventBus> event_bus;
        int last_sensor_state;
};