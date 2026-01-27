/*
 * ESP32-P4 Camera Web Viewer
 *
 * 这个示例启动一个 Web 服务器,让你可以在浏览器中查看摄像头图像
 */

#include <memory>

#include "camera_ov2640.h"
#include "esp_http_server.h"
#include "driver/uart.h"

static const char* TAG = "cam_main";

void camera_task(void* arg) {
    auto* cam = static_cast<camera_ov2640*>(arg);
    cam->get_data();
}

extern "C" void app_main() {
    uart_config_t uart_config = {
        .baud_rate = 5*1024000,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_0, &uart_config);

    camera_ov2640 cam(5);

    if (cam.get_ret() != ESP_OK) {
        ESP_LOGE("main", "摄像头初始化失败");
        return;
    }

    // 启动采集任务
    xTaskCreate(camera_task, "cam_task", 4096, &cam, 1, nullptr);

    // 读取JPEG
    while (1) {
        uint32_t size;
        const uint8_t* jpeg = cam.get_latest_jpeg(&size);
        if (jpeg && size > 0) {
            // JPEG数据
            printf("JPEG_START\n");
            for (uint32_t i = 0; i < size; i++)
            {
                printf("%02x", jpeg[i]);
                if ((i + 1) % 1024 == 0)
                {
                    printf("\n");
                    fflush(stdout);
                    vTaskDelay(pdMS_TO_TICKS(5));
                }
            }
            printf("\nJPEG_END\n");
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
