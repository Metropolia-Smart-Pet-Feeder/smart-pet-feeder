#pragma once

#include "driver/gpio.h"

namespace BoardConfig {
    // SPI Display Pins
    constexpr gpio_num_t LCD_SCLK = GPIO_NUM_21;
    constexpr gpio_num_t LCD_MOSI = GPIO_NUM_19;
    constexpr gpio_num_t LCD_MISO = GPIO_NUM_20;
    constexpr gpio_num_t LCD_DC = GPIO_NUM_1;
    constexpr gpio_num_t LCD_RST = GPIO_NUM_37;
    constexpr gpio_num_t LCD_CS = GPIO_NUM_36;
    constexpr gpio_num_t LCD_BACKLIGHT = GPIO_NUM_35;
    
    // I2C Touch Pins
    constexpr gpio_num_t TOUCH_SCL = GPIO_NUM_9;
    constexpr gpio_num_t TOUCH_SDA = GPIO_NUM_8;
    constexpr gpio_num_t TOUCH_RST = GPIO_NUM_18;
    constexpr gpio_num_t TOUCH_INT = GPIO_NUM_17;

    //RFID pins Uses same SCLK, MISO & MOSI as LCD.
    constexpr gpio_num_t RFID_RST = GPIO_NUM_48;
    constexpr gpio_num_t RFID_CS = GPIO_NUM_47;

    // IR sensor pins
    constexpr gpio_num_t IR_FOOD_LEVEL_75 = GPIO_NUM_41;
    constexpr gpio_num_t IR_FOOD_LEVEL_50 = GPIO_NUM_42;
    constexpr gpio_num_t IR_FOOD_LEVEL_25 = GPIO_NUM_2;
    constexpr gpio_num_t IR_MOTION_LEFT = GPIO_NUM_4;
    constexpr gpio_num_t IR_MOTION_CENTER = GPIO_NUM_5;
    constexpr gpio_num_t IR_MOTION_RIGHT = GPIO_NUM_6;

    // weight sensor pins
    constexpr gpio_num_t SCALE_DOUT     = GPIO_NUM_40;
    constexpr gpio_num_t SCALE_SCK      = GPIO_NUM_39;
    constexpr float      SCALE_FACTOR   = 110.16f;
    constexpr float      SCALE_THRESHOLD_G = 2.0f;

    // motor control pins (old driver board)
    // constexpr gpio_num_t MOTOR_SLEEP = GPIO_NUM_7;
    // constexpr gpio_num_t MOTOR_STEP  = GPIO_NUM_10;
    // constexpr gpio_num_t MOTOR_DIR   = GPIO_NUM_45;

    // 4-wire stepper motor pins
    constexpr gpio_num_t STEPPER_IN1 = GPIO_NUM_7;
    constexpr gpio_num_t STEPPER_IN2 = GPIO_NUM_10;
    constexpr gpio_num_t STEPPER_IN3 = GPIO_NUM_14;
    constexpr gpio_num_t STEPPER_IN4 = GPIO_NUM_45;

    // SPI to P4 pins
    constexpr gpio_num_t P4_SCLK = GPIO_NUM_12;
    constexpr gpio_num_t P4_MOSI = GPIO_NUM_11;
    constexpr gpio_num_t P4_MISO = GPIO_NUM_13;
    constexpr gpio_num_t P4_EXTRA_1 = GPIO_NUM_15;
    constexpr gpio_num_t P4_EXTRA_2 = GPIO_NUM_16;
    constexpr gpio_num_t P4_EXTRA_3 = GPIO_NUM_3;

    // 

    constexpr int LCD_WIDTH = 320;
    constexpr int LCD_HEIGHT = 240;
    constexpr size_t LCD_MAX_TRANSFER_SIZE = (LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t));
    constexpr int LCD_PIXEL_CLOCK_HZ = 10 * 1000 * 1000;
    constexpr int LCD_CMD_BITS = 8;
    constexpr int LCD_PARAM_BITS = 8;
    constexpr int LCD_DRAW_BUF_LINES = 20;
    constexpr bool LCD_BACKLIGHT_ON_LEVEL = true;
    constexpr int LCD_SPI_HOST = 3;  // SPI3_HOST
    
    // Touch Settings
    constexpr int TOUCH_I2C_ADDR = 0x38;
    constexpr int TOUCH_I2C_FREQ_HZ = 100000;
    constexpr int TOUCH_I2C_PORT = 0;  // I2C_NUM_0
    
    // LVGL Settings
    constexpr int LVGL_TICK_PERIOD_MS = 2;
    constexpr int LVGL_TASK_STACK_SIZE = 8 * 1024;
    constexpr int LVGL_TASK_PRIORITY = 2;
    constexpr int LVGL_TASK_DELAY_MS = 5;
}