#include "image_sender.h"

#define REQUEST_PIN     GPIO_NUM_50
#define READY_PIN       GPIO_NUM_49
#define CS_PIN          GPIO_NUM_46

static const char* TAG = "ImageSender";

ImageSender::ImageSender(std::shared_ptr<camera_ov2640> camera) : camera(camera){
    //Set up READY pin
    gpio_config_t req_io = {};
    req_io.mode = GPIO_MODE_INPUT;
    req_io.pin_bit_mask = (1ULL << REQUEST_PIN);
    req_io.pull_down_en = GPIO_PULLDOWN_ENABLE;
    gpio_config(&req_io);

    gpio_config_t ready_io ={};
    ready_io.mode = GPIO_MODE_OUTPUT;
    ready_io.pin_bit_mask = (1ULL << READY_PIN);
    gpio_config(&ready_io);
    gpio_set_level(READY_PIN, 0);

    image_buffer = (uint8_t*)heap_caps_aligned_alloc(64, MAX_IMAGE + 4 + 64, MALLOC_CAP_SPIRAM);
    assert(image_buffer && "Failed to allocate image_buffer");
    ESP_LOGI(TAG, "image_buffer addr: %p", image_buffer);

    //SPI master config
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = GPIO_NUM_35;
    bus_config.miso_io_num = GPIO_NUM_36;
    bus_config.sclk_io_num = GPIO_NUM_30;
    bus_config.quadwp_io_num = -1; 
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = 8192;

    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev = {};
    dev.clock_speed_hz = 1000000;
    dev.mode = 0;
    dev.spics_io_num = CS_PIN;
    dev.queue_size = 1;

    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &spi));
    ESP_LOGI(TAG, "ImageSender initialized as SPI master");
}

void ImageSender::setTestMode(bool enable){
    test_mode = enable;
}

void ImageSender::start(){
    xTaskCreate(task_entry, "image sender", 4096, this, 5, &task_handle);

}

void ImageSender::task_entry(void* arg){
    static_cast<ImageSender*>(arg)->task_loop();

}

bool ImageSender::load_from_camera(uint8_t* out, uint32_t* size){
    *size = 0;
    if(!camera){
        return false;
    }

    uint32_t jpeg_size = 0;
    const uint8_t* jpeg = camera-> get_latest_jpeg(&jpeg_size);

    if(!jpeg || jpeg_size == 0 || jpeg_size > MAX_IMAGE) {
        return false;
    }
    
    memcpy(out, jpeg, jpeg_size);
    *size = jpeg_size;
    return true;
}

bool ImageSender::load_from_file(uint8_t* out, uint32_t* size){
    extern const uint8_t _binary_image_jpg_start[];
    extern const uint8_t _binary_image_jpg_end[];
    uint32_t size_in =(uint32_t)(_binary_image_jpg_end - _binary_image_jpg_start);
    
    if(size_in == 0 || size_in > (MAX_IMAGE)) {
        return false;
    }
    memcpy(out, _binary_image_jpg_start, size_in);
    *size = size_in;
    return true;
}

void ImageSender::task_loop(){
    while(true){
        ESP_LOGI(TAG, "Waiting for image request...");
        while(gpio_get_level(REQUEST_PIN) == 0){
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        ESP_LOGI(TAG, "Request received");

        uint32_t size = 0;
        bool ok = test_mode ? load_from_file(image_buffer + 4, &size) :load_from_camera(image_buffer + 4, &size);
            
        if (!ok || size == 0) {
            ESP_LOGE(TAG, "no image");
            while(gpio_get_level(REQUEST_PIN)==1){
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            continue;
        }
            
        *((uint32_t*)image_buffer) = size;
        ESP_LOGI(TAG, "Image ready, size=%" PRIu32, size);

        gpio_set_level(READY_PIN, 1);
        ESP_LOGI(TAG, "READY asserted");

        vTaskDelay(pdMS_TO_TICKS(3000));

        ESP_LOGI(TAG, "Sending...");

        memset(image_buffer + 4 + size, 0, BUFFER_SIZE - 4 - size);
        uint32_t offset = 0;
        while(offset < BUFFER_SIZE) {
            uint32_t chunk = (BUFFER_SIZE - offset) < 8192 ? (BUFFER_SIZE - offset) : 8192;

            spi_transaction_t send = {};
            send.length = chunk * 8;
            send.tx_buffer = image_buffer + offset;
            ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &send));

            offset += chunk;
        }

        gpio_set_level(READY_PIN, 0);
        ESP_LOGI(TAG, "Image sent(%" PRIu32 " bytes in %" PRIu32 " chunks", size, (BUFFER_SIZE + 8191) / 8192);

        while(gpio_get_level(REQUEST_PIN) == 1){
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}