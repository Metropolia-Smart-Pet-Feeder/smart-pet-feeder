#ifndef RC522_H
#define RC522_H

#pragma once
#include <cstdint>
#include <memory>
#include <functional>
#include "SPIBus.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "EventBus.h"

class RC522 {
    public:
        RC522(std::shared_ptr<SPIBus> spi_bus, int cs_pin, int rst_pin, std::shared_ptr<EventBus> event_bus);
        ~RC522();

        esp_err_t init();
        void startTask();

    private:

        std::shared_ptr<SPIBus> spi_bus;
        int cs_pin;
        int rst_pin;
        spi_device_handle_t spi{nullptr};
        std::shared_ptr<EventBus> event_bus;

        bool initialized{false};
        TaskHandle_t task_handle{nullptr};

        esp_err_t transceive(uint8_t *tx, uint8_t *rx, size_t len);
        esp_err_t write_register(uint8_t reg, uint8_t value);
        uint8_t read_register(uint8_t reg);

        bool detect_card();
        bool read_UID(uint8_t *uid, uint8_t &uid_len);

        static void rfid_task(void *arg);
};

#endif