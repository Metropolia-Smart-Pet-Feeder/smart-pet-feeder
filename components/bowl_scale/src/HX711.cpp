#include "HX711.h"
#include "esp_log.h"
#include <rom/ets_sys.h>

#define HIGH 1
#define LOW  0
#define CLOCK_DELAY_US 20
#define TAG "HX711"

HX711::HX711()
    : _sck(GPIO_NUM_15), _dout(GPIO_NUM_14), _gain(eGAIN_128),
      _offset(0), _scale(1.0f),
      _offsetA(0), _offsetB(0), _scaleA(1.0f), _scaleB(1.0f)
{}

HX711::~HX711() {}

void HX711::init(gpio_num_t dout, gpio_num_t pd_sck, Gain gain)
{
    _sck  = pd_sck;
    _dout = dout;

    gpio_config_t cfg = {};
    cfg.intr_type    = GPIO_INTR_DISABLE;
    cfg.mode         = GPIO_MODE_OUTPUT;
    cfg.pin_bit_mask = (1ULL << _sck);
    gpio_config(&cfg);

    cfg.mode         = GPIO_MODE_INPUT;
    cfg.pin_bit_mask = (1ULL << _dout);
    cfg.pull_up_en   = GPIO_PULLUP_DISABLE;
    gpio_config(&cfg);

    setGain(gain);
}

bool HX711::isReady()
{
    return gpio_get_level(_dout) == LOW;
}

void HX711::setGain(Gain gain)
{
    _gain = gain;
    gpio_set_level(_sck, LOW);
    read();
}

uint8_t HX711::shiftIn()
{
    uint8_t val = 0;
    for (int i = 0; i < 8; ++i) {
        gpio_set_level(_sck, HIGH);
        ets_delay_us(CLOCK_DELAY_US);
        val |= gpio_get_level(_dout) << (7 - i);
        gpio_set_level(_sck, LOW);
        ets_delay_us(CLOCK_DELAY_US);
    }
    return val;
}

unsigned long HX711::read()
{
    gpio_set_level(_sck, LOW);
    while (!isReady())
        vTaskDelay(pdMS_TO_TICKS(10));

    unsigned long value = 0;

    portDISABLE_INTERRUPTS();
    for (int i = 0; i < 24; i++) {
        gpio_set_level(_sck, HIGH);
        ets_delay_us(CLOCK_DELAY_US);
        value <<= 1;
        gpio_set_level(_sck, LOW);
        ets_delay_us(CLOCK_DELAY_US);
        if (gpio_get_level(_dout)) value++;
    }
    for (unsigned int i = 0; i < (unsigned int)_gain; i++) {
        gpio_set_level(_sck, HIGH);
        ets_delay_us(CLOCK_DELAY_US);
        gpio_set_level(_sck, LOW);
        ets_delay_us(CLOCK_DELAY_US);
    }
    portENABLE_INTERRUPTS();

    return value ^ 0x800000;
}

unsigned long HX711::readAverage(uint8_t times)
{
    unsigned long sum = 0;
    for (uint8_t i = 0; i < times; i++)
        sum += read();
    return sum / times;
}

unsigned long HX711::getValue(uint8_t times)
{
    unsigned long avg = readAverage(times);
    return (avg < _offset) ? (_offset - avg) : (avg - _offset);
}

float HX711::getUnits(uint8_t times)
{
    return getValue(times) / _scale;
}

void HX711::tare()
{
    _offset = readAverage(50);
}

void HX711::tareAB()
{
    setGain(eGAIN_128);
    read();
    vTaskDelay(pdMS_TO_TICKS(50));
    tare();
    _offsetA = _offset;

    setGain(eGAIN_32);
    read();
    vTaskDelay(pdMS_TO_TICKS(50));
    tare();
    _offsetB = _offset;
}

void  HX711::setScale(float scale)  { _scale = _scaleA = _scaleB = scale; }
float HX711::getScale()             { return _scale; }
void  HX711::setOffset(unsigned long offset) { _offset = offset; }
unsigned long HX711::getOffset()    { return _offset; }

void HX711::powerDown()
{
    gpio_set_level(_sck, LOW);
    ets_delay_us(CLOCK_DELAY_US);
    gpio_set_level(_sck, HIGH);
    ets_delay_us(CLOCK_DELAY_US);
}

void HX711::powerUp()
{
    gpio_set_level(_sck, LOW);
}

float HX711::autoCalibrate(float known_weight, uint32_t tare_delay_ms, uint32_t weight_delay_ms)
{
    ESP_LOGI(TAG, "Taring... remove all weight");
    tare();
    vTaskDelay(pdMS_TO_TICKS(tare_delay_ms));

    ESP_LOGI(TAG, "Place %.0fg weight and wait...", known_weight);
    vTaskDelay(pdMS_TO_TICKS(weight_delay_ms));

    unsigned long raw = getValue(30);
    if (raw == 0) { ESP_LOGE(TAG, "Calibration failed: raw=0"); return 0; }

    float s = (float)raw / known_weight;
    ESP_LOGI(TAG, "Raw=%ld  SCALE=%.2f", raw, s);
    setScale(s);
    return s;
}

float HX711::autoCalibrateDual(float known_weight, uint32_t tare_delay_ms, uint32_t weight_delay_ms)
{
    ESP_LOGI(TAG, "Dual channel calibration start - remove all weight");

    setGain(eGAIN_128); read(); vTaskDelay(pdMS_TO_TICKS(50));
    tare(); _offsetA = _offset;
    ESP_LOGI(TAG, "Channel A tared, OFFSET_A=%ld", _offsetA);

    vTaskDelay(pdMS_TO_TICKS(tare_delay_ms));

    setGain(eGAIN_32); read(); vTaskDelay(pdMS_TO_TICKS(50));
    tare(); _offsetB = _offset;
    ESP_LOGI(TAG, "Channel B tared, OFFSET_B=%ld", _offsetB);

    ESP_LOGI(TAG, "Place %.0fg weight and wait...", known_weight);
    vTaskDelay(pdMS_TO_TICKS(weight_delay_ms));

    setGain(eGAIN_128); read(); vTaskDelay(pdMS_TO_TICKS(50));
    _offset = _offsetA;
    unsigned long rawA = getValue(30);

    setGain(eGAIN_32); read(); vTaskDelay(pdMS_TO_TICKS(50));
    _offset = _offsetB;
    unsigned long rawB = getValue(30);

    unsigned long total = rawA + rawB;
    if (total == 0)   { ESP_LOGE(TAG, "Calibration failed: total=0");  return 0; }
    if (total < 500)  { ESP_LOGE(TAG, "Calibration suspicious: total=%ld", total); return 0; }

    float s = (float)total / known_weight;
    ESP_LOGI(TAG, "Raw A=%ld B=%ld Total=%ld  SCALE=%.2f", rawA, rawB, total, s);
    _scaleA = _scaleB = s;
    return s;
}

float HX711::getUnitsChannelA(uint8_t times)
{
    setGain(eGAIN_128); read(); vTaskDelay(pdMS_TO_TICKS(10));
    _offset = _offsetA; _scale = _scaleA;
    return getUnits(times);
}

float HX711::getUnitsChannelB(uint8_t times)
{
    setGain(eGAIN_32); read(); vTaskDelay(pdMS_TO_TICKS(10));
    _offset = _offsetB; _scale = _scaleB;
    return getUnits(times);
}