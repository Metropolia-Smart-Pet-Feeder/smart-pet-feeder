#pragma once

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "camera_ov2640.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <cstdint>
#include <memory>

class ImageSender {
    public:
        ImageSender(std::shared_ptr<camera_ov2640> camera);
        void start();
        void setTestMode(bool enable);

    private:
        static void task_entry(void* arg);
        void task_loop();
       
        bool load_from_camera(uint8_t* out, uint32_t* size);
        bool load_from_file(uint8_t* out, uint32_t* size);

        std::shared_ptr<camera_ov2640> camera;
        spi_device_handle_t spi;
        TaskHandle_t task_handle = nullptr;
        bool test_mode = false;

        uint8_t* image_buffer;
        static constexpr uint32_t MAX_IMAGE = 40000;
        static constexpr uint32_t BUFFER_SIZE = MAX_IMAGE + 4 + 64;
};
