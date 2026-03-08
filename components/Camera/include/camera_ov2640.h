//
// Created by zhiyong on 12/8/25.
//

#ifndef ESP32P4_CAMERA_OV2640_H
#define ESP32P4_CAMERA_OV2640_H

#include <cstdlib>
#include <esp_cam_sensor_types.h>
#include <esp_sccb_types.h>
#include <example_sensor_init.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include "driver/i2c_master.h"
#include "esp_cam_ctlr_isp_dvp.h"
#include "esp_cam_ctlr.h"
#include "ov2640_config.h"
#include "freertos/semphr.h"


class camera_ov2640
{
private:
    volatile bool is_busy = false;
    esp_cam_ctlr_trans_t cam_trans = {};
    esp_err_t ret = ESP_FAIL; // 错误码
    int quantity = IMAGE_QUALITY; // JPEG
    SemaphoreHandle_t sem;
    esp_cam_ctlr_handle_t cam_handle = nullptr;
    isp_proc_handle_t isp_proc = nullptr;
    example_sensor_handle_t sensor_handle = {
        .sccb_handle = nullptr,
        .i2c_bus_handle = nullptr,
    };
    esp_ldo_channel_handle_t ldo_mipi_phy = nullptr;

    static const int BUFFER_POOL_SIZE = 1;
    uint8_t* jpeg_buffer_pool[BUFFER_POOL_SIZE];
    uint32_t jpeg_size_pool[BUFFER_POOL_SIZE];
    int current_write_idx = 0;
    QueueHandle_t jpeg_queue;

public:
    camera_ov2640(int quantity);
    ~camera_ov2640();
    void get_data();
    esp_cam_ctlr_trans_t get_cam_trans();

    const uint8_t* get_latest_jpeg(uint32_t* size);

    esp_err_t get_ret() const { return ret; }
    static bool s_camera_get_new_vb(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t* trans, void* user_data);
    static bool s_camera_get_finished_trans(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t* trans, void* user_data);
    esp_err_t compress_to_jpeg(const void* in_buf, int width, int height, int quality,
    uint8_t* dest_buf, size_t dest_buf_size,
    uint32_t* out_size);
};


#endif //ESP32P4_CAMERA_OV2640_H
