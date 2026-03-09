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

esp_err_t ImageReceiver::get_image(uint32_t* size_out){
    *size_out = 0;

    memset(frame_buffer, 0, BUFFER_SIZE);
    spi_slave_transaction_t receive = {};
    receive.length = BUFFER_SIZE * 8;
    receive.rx_buffer = frame_buffer;
    ESP_ERROR_CHECK(spi_slave_queue_trans(SPI2_HOST, &receive, portMAX_DELAY));
    ESP_LOGI(TAG, "Slave queued");

    gpio_set_level(BoardConfig::P4_REQUEST, 1);
    ESP_LOGI(TAG, "REQUEST asserted");

    int timeout = 3000;
    while(gpio_get_level(BoardConfig::P4_READY) == 0 && timeout-- > 0){
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if(timeout <= 0){
        ESP_LOGW(TAG, "READY timeout");
        gpio_set_level(BoardConfig::P4_REQUEST, 0);
        spi_slave_transaction_t* dummy = nullptr;
        spi_slave_get_trans_result(SPI2_HOST, &dummy, pdMS_TO_TICKS(100));
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "Ready to recieve");

    spi_slave_transaction_t* result = nullptr;
    ESP_ERROR_CHECK(spi_slave_get_trans_result(SPI2_HOST, &result, portMAX_DELAY));
    ESP_LOGI(TAG, "trans_len = %" PRIu32 " bits", (uint32_t)receive.trans_len);
    
    if(result->trans_len < 32){
        ESP_LOGW(TAG, "Spurious transaction, requeueing...");
        memset(frame_buffer, 0, BUFFER_SIZE);
        receive.trans_len = 0;
        ESP_ERROR_CHECK(spi_slave_queue_trans(SPI2_HOST, &receive, portMAX_DELAY));
        result = nullptr;
        ESP_ERROR_CHECK(spi_slave_get_trans_result(SPI2_HOST, &result, portMAX_DELAY));
        ESP_LOGI(TAG, "Real trans_len=%" PRIu32 " bits",(uint32_t)result->trans_len);
    }
    
    gpio_set_level(BoardConfig::P4_REQUEST, 0);

    uint32_t received_size = *((uint32_t*)frame_buffer);
    ESP_LOGI(TAG, "Received size=%"PRIu32, received_size);

    if(received_size == 0 || received_size > MAX_JPEG_SIZE) {
        ESP_LOGE(TAG, "Invalid size_ %" PRIu32, received_size);
        return ESP_ERR_INVALID_SIZE;
    }

    *size_out = received_size;
    return ESP_OK;
}

void ImageReceiver::init_http(const char* device_id){
    const char* url = "http://104.168.122.188:3000/api/photos/upload/testfeeder"; //Test url
    //char url[128];
    //snprintf(url, sizeof(url), "http://104.168.122.188:3000/api/photos/upload/%s", device_id);

    esp_http_client_config_t  config= {};
    config.url = url;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 10000;

    http_client = esp_http_client_init(&config);
    assert(http_client && "Failed to init HTTP client");
    //esp_http_client_set_header(http_client, "Content-Type", "image/jpeg");
    ESP_LOGI(TAG, "HTTP client initialized for %s", url);
}

esp_err_t ImageReceiver::post_image(const char* device_id){
    ESP_LOGI(TAG, "post_image called");
    uint32_t jpeg_size = 0;

    if(!http_client){
        init_http(device_id);
    }

    xSemaphoreTake(spi_mutex, portMAX_DELAY);
    esp_err_t ret = get_image(&jpeg_size);
    xSemaphoreGive(spi_mutex);

    if(ret != ESP_OK){
        ESP_LOGE(TAG, "fetching image failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Posting to url");

    /*esp_http_client_set_post_field(http_client, (const char*)(frame_buffer + 4), jpeg_size);

    ret = esp_http_client_perform(http_client);
    if(ret == ESP_OK) {
        int status = esp_http_client_get_status_code(http_client);
        ESP_LOGI(TAG, "POST complete, HTTP status: %d", status);
        ret = (status == 200 || status == 201) ? ESP_OK : ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(ret));
    }
    return ret;*/

    const char* boundary = "ESP32Boundary";
    char header[256];
    int header_len = snprintf(header, sizeof(header),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"photo\"; filename=\"image.jpg\"\r\n"
        "Content-Type: image/jpeg\r\n"
        "\r\n",
        boundary);

    const char* footer = "\r\n--ESP32Boundary--\r\n";
    int footer_len = strlen(footer);

    int total_len = header_len + jpeg_size + footer_len;

    // Allocate full multipart body
    char content_type[64];
    snprintf(content_type, sizeof(content_type), "multipart/form-data; boundary=%s", boundary);
    esp_http_client_set_header(http_client, "Content-Type", content_type);
    esp_http_client_set_header(http_client, "Content-Length", std::to_string(total_len).c_str());

    // Open connection manually so we can write in chunks
    ret = esp_http_client_open(http_client, total_len);
    if(ret != ESP_OK){
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(ret));
        return ret;
    }
    // Write multipart header
    esp_http_client_write(http_client, header, header_len);

    // Write JPEG data directly from frame_buffer — no extra allocation
    esp_http_client_write(http_client, (const char*)(frame_buffer + 4), jpeg_size);

    // Write multipart footer
    esp_http_client_write(http_client, footer, footer_len);

    // Fetch response
    int content_length = esp_http_client_fetch_headers(http_client);
    int status = esp_http_client_get_status_code(http_client);
    ESP_LOGI(TAG, "POST complete, HTTP status: %d, content_length: %d", status, content_length);

    esp_http_client_close(http_client);

    return (status == 200 || status == 201) ? ESP_OK : ESP_FAIL;

}