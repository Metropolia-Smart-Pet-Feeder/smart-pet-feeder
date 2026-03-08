/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXAMPLE_RGB565_BITS_PER_PIXEL            16


#define CONFIG_EXAMPLE_CAM_HRES_OV2640           800
#define CONFIG_EXAMPLE_CAM_VRES_OV2640           640
#define IMAGE_QUALITY                            30
#define EXAMPLE_CAM_FORMAT                      "DVP_8bit_20Minput_RAW8_800x640_15fps" // ov2640


#define EXAMPLE_ISP_DVP_CAM_XCLK_FREQ_HZ       (24000000)  // 24MHz
#define EXAMPLE_ISP_DVP_CAM_SCCB_SCL_IO        (32)
#define EXAMPLE_ISP_DVP_CAM_SCCB_SDA_IO        (33)
#define CAM_PIN_RESET   (5)
#define EXAMPLE_ISP_DVP_CAM_D0_IO              (10)
#define EXAMPLE_ISP_DVP_CAM_D1_IO              (11)
#define EXAMPLE_ISP_DVP_CAM_D2_IO              (12)
#define EXAMPLE_ISP_DVP_CAM_D3_IO              (13)
#define EXAMPLE_ISP_DVP_CAM_D4_IO              (14)
#define EXAMPLE_ISP_DVP_CAM_D5_IO              (15)
#define EXAMPLE_ISP_DVP_CAM_D6_IO              (16)
#define EXAMPLE_ISP_DVP_CAM_D7_IO              (17)
#define EXAMPLE_ISP_DVP_CAM_XCLK_IO            (-1)
#define EXAMPLE_ISP_DVP_CAM_PCLK_IO            (7)
#define EXAMPLE_ISP_DVP_CAM_DE_IO              (4) // HREF
#define EXAMPLE_ISP_DVP_CAM_VSYNC_IO           (8)
#define EXAMPLE_ISP_DVP_CAM_HSYNC_IO           (-1)
#define CAM_PIN_PWDN    (3)




#if CONFIG_EXAMPLE_CAM_HRES_640

#if CONFIG_EXAMPLE_CAM_VRES_480
#define EXAMPLE_CAM_FORMAT                     "DVP_8bit_20Minput_RAW8_640x480_26fps" // sc030iot
#endif

#elif CONFIG_EXAMPLE_CAM_HRES_800

#if CONFIG_EXAMPLE_CAM_VRES_640
#define EXAMPLE_CAM_FORMAT                     "DVP_8bit_20Minput_RAW8_800x640_15fps" // ov2640
#elif CONFIG_EXAMPLE_CAM_VRES_800
#define EXAMPLE_CAM_FORMAT                     "DVP_8bit_20Minput_RAW8_800x800_15fps" // ov2640
#endif

#elif CONFIG_EXAMPLE_CAM_HRES_1024

#if CONFIG_EXAMPLE_CAM_VRES_600
#define EXAMPLE_CAM_FORMAT                     "DVP_8bit_20Minput_RAW8_800x640_15fps" // ov2640
#endif

#endif

#ifndef EXAMPLE_CAM_FORMAT
#error "Unsupported camera format! Please adjust EXAMPLE_CAM_HRES and EXAMPLE_CAM_VRES in menuconfig"
#endif

#ifdef __cplusplus
}
#endif
