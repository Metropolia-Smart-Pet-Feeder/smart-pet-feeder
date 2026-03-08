//
// Created by zhiyong on 12/8/25.
//

#include "include/camera_ov2640.h"

#include <example_sensor_init.h>

#include "esp_attr.h"
#include "esp_lcd_panel_ops.h"
#include "esp_cache.h"
#include "driver/isp.h"
#include "driver/ledc.h"
#include "driver/jpeg_encode.h"
#include "esp_heap_caps.h"

static const char* TAG = "dvp_isp_dsi";

camera_ov2640::camera_ov2640(int quantity)
    : quantity(quantity)
{
    bool init_success = false;
    bool sensor_init_success = false;
    sem = xSemaphoreCreateBinary();
    if (sem == nullptr)
    {
        ESP_LOGE(TAG, "sem create failed");
        goto cleanup;
    }
    {
        esp_ldo_channel_config_t ldo_mipi_phy_config = {
            .chan_id = CONFIG_EXAMPLE_USED_LDO_CHAN_ID,
            .voltage_mv = CONFIG_EXAMPLE_USED_LDO_VOLTAGE_MV,
        };
        ret = esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ldo_acquire_channel failed: %s", esp_err_to_name(ret));
            goto cleanup;
        }

        //--------Camera Sensor and SCCB Init-----------//
        gpio_config_t conf = {};
        conf.pin_bit_mask = (1ULL << CAM_PIN_RESET) | (1ULL << CAM_PIN_PWDN);
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

        gpio_set_level((gpio_num_t)CAM_PIN_PWDN, 0); // PWDN low

        gpio_set_level((gpio_num_t)CAM_PIN_RESET, 0); // RESET low
        vTaskDelay(pdMS_TO_TICKS(10));

        gpio_set_level(static_cast<gpio_num_t>(CAM_PIN_RESET), 1); // RESET high
        vTaskDelay(pdMS_TO_TICKS(100));

        gpio_pullup_en((gpio_num_t)EXAMPLE_ISP_DVP_CAM_SCCB_SDA_IO);
        gpio_pullup_en((gpio_num_t)EXAMPLE_ISP_DVP_CAM_SCCB_SCL_IO);

        example_sensor_config_t cam_sensor_config = {
            .i2c_port_num = I2C_NUM_1,
            .i2c_sda_io_num = EXAMPLE_ISP_DVP_CAM_SCCB_SDA_IO,
            .i2c_scl_io_num = EXAMPLE_ISP_DVP_CAM_SCCB_SCL_IO,
            .port = ESP_CAM_SENSOR_DVP,
            .format_name = EXAMPLE_CAM_FORMAT,
        };
        ret = example_sensor_init(&cam_sensor_config, &sensor_handle);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "example_sensor_init failed: %s", esp_err_to_name(ret));
            goto cleanup;
        }
        sensor_init_success = true;
        //---------------ISP Init------------------//
        esp_isp_processor_cfg_t isp_config = {
            .clk_hz = 80 * 1000 * 1000,
            .input_data_source = ISP_INPUT_DATA_SOURCE_DVP,
            .input_data_color_type = ISP_COLOR_RAW8,
            .output_data_color_type = ISP_COLOR_RGB565,
            .has_line_start_packet = false,
            .has_line_end_packet = false,
            .h_res = CONFIG_EXAMPLE_CAM_HRES_OV2640,
            .v_res = CONFIG_EXAMPLE_CAM_VRES_OV2640,
        };
        ret = esp_isp_new_processor(&isp_config, &isp_proc);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_isp_new_processor failed: %s", esp_err_to_name(ret));
            goto cleanup;
        }

        ret = esp_isp_enable(isp_proc);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_isp_enable failed: %s", esp_err_to_name(ret));
            goto cleanup;
        }

        //----------CAM Controller Init------------//
        esp_cam_ctlr_isp_dvp_cfg_t dvp_config = {
            .data_width = CAM_CTLR_DATA_WIDTH_8,
            .data_io = {
                EXAMPLE_ISP_DVP_CAM_D0_IO,
                EXAMPLE_ISP_DVP_CAM_D1_IO,
                EXAMPLE_ISP_DVP_CAM_D2_IO,
                EXAMPLE_ISP_DVP_CAM_D3_IO,
                EXAMPLE_ISP_DVP_CAM_D4_IO,
                EXAMPLE_ISP_DVP_CAM_D5_IO,
                EXAMPLE_ISP_DVP_CAM_D6_IO,
                EXAMPLE_ISP_DVP_CAM_D7_IO,
            },
            .pclk_io = EXAMPLE_ISP_DVP_CAM_PCLK_IO,
            .hsync_io = EXAMPLE_ISP_DVP_CAM_HSYNC_IO,
            .vsync_io = EXAMPLE_ISP_DVP_CAM_VSYNC_IO,
            .de_io = EXAMPLE_ISP_DVP_CAM_DE_IO,
            .io_flags = {
                .vsync_invert = 1,
            },
            .queue_items = 1,
        };
        ret = esp_cam_new_isp_dvp_ctlr(isp_proc, &dvp_config, &cam_handle);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "isp dvp init fail: 0x%x (%s)", ret, esp_err_to_name(ret));
            goto cleanup;
        }

        //--------Allocate Camera Buffer----------//
        size_t cam_buffer_size = CONFIG_EXAMPLE_CAM_HRES_OV2640 * CONFIG_EXAMPLE_CAM_VRES_OV2640 *
            EXAMPLE_RGB565_BITS_PER_PIXEL / 8;
        void* cam_buffer = esp_cam_ctlr_alloc_buffer(cam_handle, cam_buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
        if (!cam_buffer)
        {
            ESP_LOGE("", "no mem for cam_buffer");
            goto cleanup;
        }
        cam_trans = {
            .buffer = cam_buffer,
            .buflen = cam_buffer_size,
        };

        esp_cam_ctlr_evt_cbs_t cbs = {
            .on_get_new_trans = s_camera_get_new_vb,
            .on_trans_finished = s_camera_get_finished_trans,
        };
        if (esp_cam_ctlr_register_event_callbacks(cam_handle, &cbs, this) != ESP_OK)
        {
            ESP_LOGE(TAG, "ops register fail");
            goto cleanup;
        }

        ESP_ERROR_CHECK(esp_cam_ctlr_enable(cam_handle));

        if (esp_cam_ctlr_start(cam_handle) != ESP_OK)
        {
            ESP_LOGE(TAG, "Driver start fail");
            goto cleanup;
        }

        // -------JPEG Buffer Pool Init---------//
        jpeg_queue = xQueueCreate(BUFFER_POOL_SIZE, sizeof(int));
        size_t jpeg_buf_size = CONFIG_EXAMPLE_CAM_HRES_OV2640 * CONFIG_EXAMPLE_CAM_VRES_OV2640;

        for (int i = 0; i < BUFFER_POOL_SIZE; i++)
        {
            jpeg_buffer_pool[i] = (uint8_t*)heap_caps_aligned_alloc(
                64, jpeg_buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
            );
            jpeg_size_pool[i] = 0;

            if (jpeg_buffer_pool[i] == nullptr)
            {
                ESP_LOGE(TAG, "Failed to allocate jpeg buffer %d", i);
                ret = ESP_ERR_NO_MEM;
                goto cleanup;
            }
        }
    }
    init_success = true;
cleanup:
    if (!init_success)
    {
        for (int i = 0; i < BUFFER_POOL_SIZE; i++)
        {
            if (jpeg_buffer_pool[i] != nullptr)
            {
                free(jpeg_buffer_pool[i]);
                jpeg_buffer_pool[i] = nullptr;
            }
        }

        if (jpeg_queue != nullptr)
        {
            vQueueDelete(jpeg_queue);
            jpeg_queue = nullptr;
        }

        if (cam_handle != nullptr)
        {
            esp_cam_ctlr_stop(cam_handle);
            esp_cam_ctlr_disable(cam_handle);
            if (cam_trans.buffer != nullptr)
            {
                free(cam_trans.buffer);
            }
            esp_cam_ctlr_del(cam_handle);
            cam_handle = nullptr;
        }

        if (isp_proc != nullptr)
        {
            esp_isp_disable(isp_proc);
            esp_isp_del_processor(isp_proc);
            isp_proc = nullptr;
        }

        if (sensor_init_success)
        {
            example_sensor_deinit(sensor_handle);
        }

        if (ldo_mipi_phy != nullptr)
        {
            esp_ldo_release_channel(ldo_mipi_phy);
            ldo_mipi_phy = nullptr;
        }

        if (sem != nullptr)
        {
            vSemaphoreDelete(sem);
            sem = nullptr;
        }

        ESP_LOGE(TAG, "camera_ov2640 initialization failed");
    }
}

camera_ov2640::~camera_ov2640()
{
    // 0. del JPEG buffer pool and queue
    if (jpeg_queue != nullptr)
    {
        vQueueDelete(jpeg_queue);
        jpeg_queue = nullptr;
    }

    for (int i = 0; i < BUFFER_POOL_SIZE; i++)
    {
        if (jpeg_buffer_pool[i] != nullptr)
        {
            free(jpeg_buffer_pool[i]);
            jpeg_buffer_pool[i] = nullptr;
        }
    }

    // 1. stop and disable camera controller
    if (cam_handle != nullptr)
    {
        esp_cam_ctlr_stop(cam_handle);
        esp_cam_ctlr_disable(cam_handle);
    }

    // 2. release camera buffer
    if (cam_trans.buffer != nullptr)
    {
        free(cam_trans.buffer);
        cam_trans.buffer = nullptr;
    }

    // 3. del camera control
    if (cam_handle != nullptr)
    {
        esp_cam_ctlr_del(cam_handle);
        cam_handle = nullptr;
    }

    // 4. stop and del ISP processor
    if (isp_proc != nullptr)
    {
        esp_isp_disable(isp_proc);
        esp_isp_del_processor(isp_proc);
        isp_proc = nullptr;
    }

    // 5. del seneor
    example_sensor_deinit(sensor_handle);

    // 6. relax LDO channel
    if (ldo_mipi_phy != nullptr)
    {
        esp_ldo_release_channel(ldo_mipi_phy);
        ldo_mipi_phy = nullptr;
    }

    // 7. del sem
    if (sem != nullptr)
    {
        vSemaphoreDelete(sem);
        sem = nullptr;
    }

    ESP_LOGI(TAG, "camera_ov2640 destroyed");
}



void camera_ov2640::get_data()
{
    while (1)
    {

        if (xSemaphoreTake(sem, portMAX_DELAY) == pdTRUE)
        {
            is_busy = true;
            esp_cache_msync(cam_trans.buffer, cam_trans.buflen, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
            uint32_t jpeg_size = 0;
            size_t buf_capacity = CONFIG_EXAMPLE_CAM_HRES_OV2640 * CONFIG_EXAMPLE_CAM_VRES_OV2640;

            // 压缩为 JPEG
            ret = compress_to_jpeg(
                cam_trans.buffer,
                CONFIG_EXAMPLE_CAM_HRES_OV2640,
                CONFIG_EXAMPLE_CAM_VRES_OV2640,
                quantity,
                jpeg_buffer_pool[current_write_idx],
                buf_capacity,
                &jpeg_size
            );

            if (ret == ESP_OK && jpeg_size > 0)
            {
                jpeg_size_pool[current_write_idx] = jpeg_size;
                xQueueOverwrite(jpeg_queue, &current_write_idx);
                current_write_idx = (current_write_idx + 1) % BUFFER_POOL_SIZE;
            }
            is_busy = false;
        }

    }
}

const uint8_t* camera_ov2640::get_latest_jpeg(uint32_t* size)
{
    int idx;
    if (xQueuePeek(jpeg_queue, &idx, 0) == pdTRUE)
    {
        *size = jpeg_size_pool[idx];
        return jpeg_buffer_pool[idx];
    }

    *size = 0;
    return nullptr;
}

esp_cam_ctlr_trans_t camera_ov2640::get_cam_trans()
{
    return cam_trans;
}

bool camera_ov2640::s_camera_get_new_vb(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t* trans, void* user_data)
{
    auto* self = static_cast<camera_ov2640*>(user_data);
    trans->buffer = self->cam_trans.buffer;
    trans->buflen = self->cam_trans.buflen;
    return false;
}

bool camera_ov2640::s_camera_get_finished_trans(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t* trans,
                                                void* user_data)
{
    // 此处中断 发送信号通知get_data有新数据
    camera_ov2640* self = static_cast<camera_ov2640*>(user_data);
    if (!self->is_busy)
    {
        BaseType_t higher_priority_task_woken = pdFALSE;
        xSemaphoreGiveFromISR(self->sem, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);  // 如果唤醒了高优先级任务则立即切换
    }
    // 通知帧已准备好
    return false;
}

esp_err_t camera_ov2640::compress_to_jpeg(
    const void* in_buf, int width, int height, int quality,
    uint8_t* dest_buf, size_t dest_buf_size,
    uint32_t* out_size)
{
    esp_err_t ret = ESP_OK;
    jpeg_encoder_handle_t jpeg_handle = NULL;

    jpeg_encode_engine_cfg_t encode_eng_cfg = {
        .intr_priority = 0,
        .timeout_ms = 200,
    };

    ret = jpeg_new_encoder_engine(&encode_eng_cfg, &jpeg_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "New jpeg encoder engine failed");
        return ret;
    }

    jpeg_encode_cfg_t enc_config = {
        .height = static_cast<uint32_t>(height),
        .width = static_cast<uint32_t>(width),
        .src_type = JPEG_ENCODE_IN_FORMAT_RGB565,
        .sub_sample = JPEG_DOWN_SAMPLING_YUV422,
        .image_quality = static_cast<uint8_t>(quality),
    };

    ret = jpeg_encoder_process(
        jpeg_handle, &enc_config,
        static_cast<const uint8_t*>(in_buf), (width * height * 2),
        dest_buf, dest_buf_size, out_size
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "JPEG encode process failed: %s", esp_err_to_name(ret));
        *out_size = 0;
        jpeg_del_encoder_engine(jpeg_handle);
        return ret;
    }

    jpeg_del_encoder_engine(jpeg_handle);
    return ESP_OK;
}
