#pragma once

#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "board_config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include <string>
#include "esp_timer.h"

class ImageReceiver {
    public:
        ImageReceiver();
        ~ImageReceiver();
        esp_err_t post_image(const char* device_id);

    private:
        void init();
        void init_http(const char* device_id);
        esp_err_t get_image(uint32_t* size_out);

        SemaphoreHandle_t spi_mutex = nullptr;
        uint8_t* frame_buffer = nullptr;;
        esp_http_client_handle_t http_client = nullptr;

        static constexpr uint32_t MAX_JPEG_SIZE = 40000;
        static constexpr uint32_t BUFFER_SIZE = MAX_JPEG_SIZE + 4 + 64;
};