#ifndef HX711_H
#define HX711_H

#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class HX711
{
public:
    typedef enum {
        eGAIN_128 = 1,
        eGAIN_64  = 3,
        eGAIN_32  = 2
    } Gain;

    HX711();
    ~HX711();

    void  init(gpio_num_t dout, gpio_num_t pd_sck, Gain gain = eGAIN_128);
    bool  isReady();
    void  setGain(Gain gain);

    unsigned long read();
    unsigned long readAverage(uint8_t times);
    unsigned long getValue(uint8_t times);
    float         getUnits(uint8_t times);

    void  tare();
    void  tareAB();

    void          setScale(float scale);
    float         getScale();
    void          setOffset(unsigned long offset);
    unsigned long getOffset();

    void powerDown();
    void powerUp();

    float autoCalibrate(float known_weight, uint32_t tare_delay_ms, uint32_t weight_delay_ms);
    float autoCalibrateDual(float known_weight, uint32_t tare_delay_ms, uint32_t weight_delay_ms);

    float getUnitsChannelA(uint8_t times);
    float getUnitsChannelB(uint8_t times);

private:
    uint8_t shiftIn();

    gpio_num_t  _sck;
    gpio_num_t  _dout;
    Gain        _gain;

    unsigned long _offset;
    float         _scale;

    unsigned long _offsetA;
    unsigned long _offsetB;
    float         _scaleA;
    float         _scaleB;
};

#endif