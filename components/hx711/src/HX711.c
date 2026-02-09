#include "HX711.h"
#include "esp_log.h"
#include <rom/ets_sys.h>

#define HIGH 1
#define LOW 0
#define CLOCK_DELAY_US 20

#define DEBUGTAG "HX711"

static gpio_num_t GPIO_PD_SCK = GPIO_NUM_15;	// Power Down and Serial Clock Input Pin
static gpio_num_t GPIO_DOUT = GPIO_NUM_14;		// Serial Data Output Pin
static HX711_GAIN GAIN = eGAIN_128;		// amplification factor
static unsigned long OFFSET = 0;	// used for tare weight
static float SCALE = 1;	// used to return weight in grams, kg, ounces, whatever

// DUAL CHANNEL VARIABLES
static unsigned long OFFSET_A = 0;
static unsigned long OFFSET_B = 0;
static float SCALE_A = 1;
static float SCALE_B = 1;

void HX711_init(gpio_num_t dout, gpio_num_t pd_sck, HX711_GAIN gain )
{
	GPIO_PD_SCK = pd_sck;
	GPIO_DOUT = dout;

	gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL<<GPIO_PD_SCK);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL<<GPIO_DOUT);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 0;
	gpio_config(&io_conf);

	HX711_set_gain(gain);
}

bool HX711_is_ready()
{
	return gpio_get_level(GPIO_DOUT);
}

void HX711_set_gain(HX711_GAIN gain)
{
	GAIN = gain;
	gpio_set_level(GPIO_PD_SCK, LOW);
	HX711_read();
}

uint8_t HX711_shiftIn()
{
	uint8_t value = 0;

    for(int i = 0; i < 8; ++i)
    {
        gpio_set_level(GPIO_PD_SCK, HIGH);
        ets_delay_us(CLOCK_DELAY_US);
        value |= gpio_get_level(GPIO_DOUT) << (7 - i); //get level returns
        gpio_set_level(GPIO_PD_SCK, LOW);
        ets_delay_us(CLOCK_DELAY_US);
    }

    return value;
}

unsigned long HX711_read()
{
	gpio_set_level(GPIO_PD_SCK, LOW);
	// wait for the chip to become ready
	while (HX711_is_ready())
	{
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}

	unsigned long value = 0;

	//--- Enter critical section ----
	portDISABLE_INTERRUPTS();

	for(int i=0; i < 24 ; i++)
	{
		gpio_set_level(GPIO_PD_SCK, HIGH);
        ets_delay_us(CLOCK_DELAY_US);
        value = value << 1;
        gpio_set_level(GPIO_PD_SCK, LOW);
        ets_delay_us(CLOCK_DELAY_US);

        if(gpio_get_level(GPIO_DOUT))
        	value++;
	}

	// set the channel and the gain factor for the next reading using the clock pin
	for (unsigned int i = 0; i < GAIN; i++)
	{
		gpio_set_level(GPIO_PD_SCK, HIGH);
		ets_delay_us(CLOCK_DELAY_US);
		gpio_set_level(GPIO_PD_SCK, LOW);
		ets_delay_us(CLOCK_DELAY_US);
	}
	portENABLE_INTERRUPTS();
	//--- Exit critical section ----

	value =value^0x800000;

	return (value);
}



unsigned long  HX711_read_average(char times)
{
	ESP_LOGI(DEBUGTAG, "===================== READ AVERAGE START ====================");
	unsigned long sum = 0;
	for (char i = 0; i < times; i++)
	{
		sum += HX711_read();
	}
	ESP_LOGI(DEBUGTAG, "===================== READ AVERAGE END : %ld ====================",(sum / times));
	return sum / times;
}

unsigned long HX711_get_value(char times)
{
	unsigned long avg = HX711_read_average(times);
	if(avg < OFFSET){
		ESP_LOGI(DEBUGTAG, "===================== READ value : %ld ====================",(OFFSET - avg));
		ESP_LOGI(DEBUGTAG, "===================== READ value222 : %ld ====================",(OFFSET - avg)/62);
		return OFFSET - avg ;
	}

	return 0;
}

float HX711_get_units(char times)
{

	return HX711_get_value(times) / SCALE;
}

void HX711_tare( )
{
	// ESP_LOGI(DEBUGTAG, "===================== START TARE ====================");
	unsigned long sum = 0;
	sum = HX711_read_average(50);
	HX711_set_offset(sum);
	// ESP_LOGI(DEBUGTAG, "===================== END TARE: %ld ====================",sum);
}

void HX711_set_scale(float scale )
{
	SCALE = scale;
}

float HX711_get_scale()
 {
	return SCALE;
}

void HX711_set_offset(unsigned long offset)
 {
	OFFSET = offset;
}

unsigned long HX711_get_offset(unsigned long offset)
{
	return OFFSET;
}

void HX711_power_down()
{
	gpio_set_level(GPIO_PD_SCK, LOW);
	ets_delay_us(CLOCK_DELAY_US);
	gpio_set_level(GPIO_PD_SCK, HIGH);
	ets_delay_us(CLOCK_DELAY_US);
}

void HX711_power_up()
{
	gpio_set_level(GPIO_PD_SCK, LOW);
}


float HX711_auto_calibrate(float known_weight, uint32_t tare_delay_ms, uint32_t weight_delay_ms)
{
	ESP_LOGI(DEBUGTAG, "Taring... remove all weight");
	HX711_tare();
	vTaskDelay(pdMS_TO_TICKS(tare_delay_ms));

	ESP_LOGI(DEBUGTAG, "Place %.0fg weight and wait...", known_weight);
	vTaskDelay(pdMS_TO_TICKS(weight_delay_ms));

	unsigned long raw = HX711_get_value(30);
	if(raw == 0) {
		ESP_LOGE(DEBUGTAG, "Calibration failed: raw value is 0");
		return 0;
	}

	float calculated_scale = (float)raw / known_weight;

	ESP_LOGI(DEBUGTAG, "Raw value = %ld", raw);
	ESP_LOGI(DEBUGTAG, "Calculated SCALE = %.2f", calculated_scale);

	HX711_set_scale(calculated_scale);

	return calculated_scale;
}

float HX711_auto_calibrate_dual(float known_weight, uint32_t tare_delay_ms, uint32_t weight_delay_ms)
{
	ESP_LOGI(DEBUGTAG, "Dual channel calibration start - remove all weight");

	// 1. A channel tare
	HX711_set_gain(eGAIN_128);
	HX711_read(); // MAKE channel switch effective
	vTaskDelay(pdMS_TO_TICKS(50));
	HX711_tare();
	OFFSET_A = OFFSET;
	ESP_LOGI(DEBUGTAG, "Channel A tared, OFFSET_A=%ld", OFFSET_A);

	vTaskDelay(pdMS_TO_TICKS(tare_delay_ms));

	// 2. B channel tare
	HX711_set_gain(eGAIN_32);
	HX711_read(); // 使通道切换生效
	vTaskDelay(pdMS_TO_TICKS(50));
	HX711_tare();
	OFFSET_B = OFFSET;
	ESP_LOGI(DEBUGTAG, "Channel B tared, OFFSET_B=%ld", OFFSET_B);

	// 3. wait for user to place known weight
	ESP_LOGI(DEBUGTAG, "Place %.0fg weight and wait...", known_weight);
	vTaskDelay(pdMS_TO_TICKS(weight_delay_ms));

	// 4. read A Channel
	HX711_set_gain(eGAIN_128);
	HX711_read();
	vTaskDelay(pdMS_TO_TICKS(50));
	OFFSET = OFFSET_A;
	unsigned long raw_A = HX711_get_value(30);

	// 5. read B Channel
	HX711_set_gain(eGAIN_32);
	HX711_read();
	vTaskDelay(pdMS_TO_TICKS(50));
	OFFSET = OFFSET_B;
	unsigned long raw_B = HX711_get_value(30);

	// 6. SCALE
	unsigned long raw_total = raw_A + raw_B;
	if(raw_total == 0) {
		ESP_LOGE(DEBUGTAG, "Calibration failed: total raw is 0");
		return 0;
	}

	float calculated_scale = (float)raw_total / known_weight;

	ESP_LOGI(DEBUGTAG, "Raw A=%ld, Raw B=%ld, Total=%ld", raw_A, raw_B, raw_total);
	ESP_LOGI(DEBUGTAG, "Calculated SCALE=%.2f", calculated_scale);

	SCALE_A = calculated_scale;
	SCALE_B = calculated_scale;

	return calculated_scale;
}

float HX711_get_units_channel_A(char times)
{
	HX711_set_gain(eGAIN_128);
	HX711_read();
	vTaskDelay(pdMS_TO_TICKS(10));
	OFFSET = OFFSET_A;
	SCALE = SCALE_A;
	return HX711_get_units(times);
}

float HX711_get_units_channel_B(char times)
{
	HX711_set_gain(eGAIN_32);
	HX711_read();
	vTaskDelay(pdMS_TO_TICKS(10));
	OFFSET = OFFSET_B;
	SCALE = SCALE_B;
	return HX711_get_units(times);
}