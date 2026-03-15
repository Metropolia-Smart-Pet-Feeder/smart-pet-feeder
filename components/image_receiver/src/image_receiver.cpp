#include "image_receiver.h"

static const char* TAG = "ImageReceiver";

ImageReceiver::ImageReceiver() {
    spi_mutex = xSemaphoreCreateMutex();

    frame_buffer = (uint8_t*)heap_caps_aligned_alloc(64, BUFFER_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(frame_buffer && "Failed to allocate frame_buffer");
    
    init();
}

ImageReceiver::~ImageReceiver(){
    if(http_client) {
        esp_http_client_cleanup(http_client);
        http_client = nullptr;
    }

    if(frame_buffer){
        heap_caps_free(frame_buffer);
        frame_buffer = nullptr;
    }

    if(spi_mutex){
        vSemaphoreDelete(spi_mutex);
        spi_mutex = nullptr;
    }
}

void ImageReceiver::init() {
    gpio_config_t req_io = {};
    req_io.mode = GPIO_MODE_OUTPUT;
    req_io.pin_bit_mask = (1ULL << BoardConfig::P4_REQUEST);
    ESP_ERROR_CHECK(gpio_config(&req_io));
    gpio_set_level(BoardConfig::P4_REQUEST, 0);

    gpio_config_t ready_io = {};
    ready_io.mode = GPIO_MODE_INPUT;
    ready_io.pin_bit_mask = (1ULL << BoardConfig::P4_READY);
    ESP_ERROR_CHECK(gpio_config(&ready_io));

    spi_bus_config_t bus = {};
    bus.mosi_io_num = BoardConfig::P4_MOSI;
    bus.miso_io_num = BoardConfig::P4_MISO;
    bus.sclk_io_num = BoardConfig::P4_SCLK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = BUFFER_SIZE;

    spi_slave_interface_config_t slave = {};
    slave.mode = 0;
    slave.spics_io_num = BoardConfig::P4_CS;
    slave.queue_size = 1;

    ESP_ERROR_CHECK(spi_slave_initialize(SPI2_HOST, &bus, &slave, SPI_DMA_CH_AUTO));
    gpio_pullup_en(BoardConfig::P4_CS);
    gpio_pulldown_dis(BoardConfig::P4_CS);
    ESP_LOGI(TAG, "End of init CS=%d", gpio_get_level(BoardConfig::P4_CS));
}

void ImageReceiver::init_http(const char* device_id){
    //const char* url = "http://"; //Test url
    char url[128];
    snprintf(url, sizeof(url), "http://", device_id);

    esp_http_client_config_t  config= {};
    config.url = url;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 10000;

    http_client = esp_http_client_init(&config);
    assert(http_client && "Failed to init HTTP client");
    ESP_LOGI(TAG, "HTTP client initialized for %s", url);
}

bool ImageReceiver::wait_for_ready(int timeout_ms){
    int ticks = timeout_ms/10;
    while(gpio_get_level(BoardConfig::P4_READY) == 0 && ticks-- > 0){
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ticks > 0;
}

esp_err_t ImageReceiver::post_image(const char* device_id){
    ESP_LOGI(TAG, "post_image called");
    if(!http_client){
        init_http(device_id);
    }

    xSemaphoreTake(spi_mutex, portMAX_DELAY);

    // Receive header with full size info
    memset(frame_buffer, 0, BUFFER_SIZE);
    spi_slave_transaction_t trans = {};
    trans.length = 4 * 8;
    trans.rx_buffer = frame_buffer;

    ESP_ERROR_CHECK(spi_slave_queue_trans(SPI2_HOST, &trans, portMAX_DELAY));
    gpio_set_level(BoardConfig::P4_REQUEST, 1);

    if(!wait_for_ready(5000)) {
        ESP_LOGW(TAG, "READY timeout waiting for header");
        gpio_set_level(BoardConfig::P4_REQUEST, 0);
        spi_slave_transaction_t* dummy = nullptr;
        spi_slave_get_trans_result(SPI2_HOST, &dummy, pdMS_TO_TICKS(100));
        xSemaphoreGive(spi_mutex);
        return ESP_ERR_TIMEOUT;
    }

    spi_slave_transaction_t* result = nullptr;
    ESP_ERROR_CHECK(spi_slave_get_trans_result(SPI2_HOST, &result, portMAX_DELAY));
    gpio_set_level(BoardConfig::P4_REQUEST, 0);

    uint32_t total_size = *((uint32_t*)frame_buffer);
    ESP_LOGI(TAG, "Total JPEG size: %" PRIu32, total_size);

    if (total_size == 0 || total_size > MAX_JPEG_SIZE) {
        ESP_LOGE(TAG, "Invalid total size %" PRIu32, total_size);
        xSemaphoreGive(spi_mutex);
        return ESP_ERR_INVALID_SIZE;
    }

    // Open HTTP and send header
    const char* boundary = "ESP32Boundary";
    char header[256];
    int header_len = snprintf(header, sizeof(header),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"photo\"; filename=\"image.jpg\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n",
        boundary);

    const char* footer = "\r\n--ESP32Boundary--\r\n";
    int footer_len = strlen(footer);
    int total_http_len = header_len + (int)total_size + footer_len;

    char content_type[64];
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", boundary);
    esp_http_client_set_header(http_client, "Content-Type", content_type);
    esp_http_client_set_header(http_client, "Content-Length", std::to_string(total_http_len).c_str());

    esp_err_t ret = esp_http_client_open(http_client, total_http_len);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP opening failed: %s", esp_err_to_name(ret));
        xSemaphoreGive(spi_mutex);
        return ret;
    }
    esp_http_client_write(http_client, header, header_len);

    // Loop of SPI receiving and HTTP writing
    uint32_t bytes_received = 0;

    while (bytes_received < total_size) {
        memset(frame_buffer, 0, BUFFER_SIZE);
        trans = {};
        trans.length = BUFFER_SIZE * 8;
        trans.rx_buffer = frame_buffer;

        ESP_ERROR_CHECK(spi_slave_queue_trans(SPI2_HOST, &trans, portMAX_DELAY));

        // Tell P4 we are ready for next chunk
        gpio_set_level(BoardConfig::P4_REQUEST, 1);

        if(!wait_for_ready(3000)){
            ESP_LOGW(TAG, "READY timeout waiting for header");
            gpio_set_level(BoardConfig::P4_REQUEST, 0);
            spi_slave_transaction_t* dummy = nullptr;
            spi_slave_get_trans_result(SPI2_HOST, &dummy, pdMS_TO_TICKS(100));
            xSemaphoreGive(spi_mutex);
            return ESP_ERR_TIMEOUT;
        }
        result = nullptr;
        ESP_ERROR_CHECK(spi_slave_get_trans_result(SPI2_HOST, &result, portMAX_DELAY));
        gpio_set_level(BoardConfig::P4_REQUEST, 0);

        uint32_t chunk_size = *((uint32_t*)frame_buffer);
        if(chunk_size == 0 || chunk_size > CHUNK_DATA) {
            ESP_LOGE(TAG, "Bad chunk size: %" PRIu32, chunk_size);
            esp_http_client_close(http_client);
            xSemaphoreGive(spi_mutex);
            return ESP_ERR_INVALID_SIZE;
        }

        ESP_LOGI(TAG, "Chunk: %" PRIu32 " bytes (total so far: %" PRIu32 "/%" PRIu32 ")",
            chunk_size, bytes_received + chunk_size, total_size);

        esp_http_client_write(http_client, (const char*)(frame_buffer + 4), chunk_size);
        bytes_received += chunk_size;
    }

    // Finish HTTP

    esp_http_client_write(http_client, footer, footer_len);
    int content_length = esp_http_client_fetch_headers(http_client);
    int status = esp_http_client_get_status_code(http_client);
    ESP_LOGI(TAG, "POST done, HTTP %d, content_length: %d", status, content_length);
    esp_http_client_close(http_client);

    xSemaphoreGive(spi_mutex);
    return(status == 200 || status == 201) ? ESP_OK : ESP_FAIL;
}
