// TCA9555.cpp
#include "TCA9555.h"
#include "esp_log.h"

static const char *TAG = "TCA9555";

TCA9555::TCA9555(i2c_port_t i2c_num, uint8_t addr)
    : _i2c_num(i2c_num), _addr(addr) {
}

esp_err_t TCA9555::init(gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t freq) {
    // Create I2C config structure and initialize to 0
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;              // Set as master mode (ESP32 as main controller)
    conf.sda_io_num = sda_pin;                // Set SDA data line pin
    conf.scl_io_num = scl_pin;                // Set SCL clock line pin
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;  // Enable SDA internal pull-up resistor (ensure signal stability)
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;  // Enable SCL internal pull-up resistor
    conf.master.clk_speed = freq;             // Set I2C clock frequency, 400kHz=fast mode

    // Apply configuration to I2C controller
    esp_err_t ret = i2c_param_config(_i2c_num, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C parameter config failed");
        return ret;
    }

    // Install I2C driver
    // Parameters: controller number, mode, RX buffer, TX buffer, interrupt flags (0 for master mode)
    ret = i2c_driver_install(_i2c_num, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed");
        return ret;
    }

    ESP_LOGI(TAG, "TCA9555 initialized successfully, address: 0x%02X", _addr);
    return ESP_OK;
}

esp_err_t TCA9555::read(uint8_t reg, uint8_t *value) {
    // Write register address first, then read data
    return i2c_master_write_read_device(
        _i2c_num,           // I2C controller number
        _addr,              // TCA9555 chip address
        &reg,               // Write register address first
        1,                  // Address length 1 byte
        value,              // Store read data here
        1,                  // Read 1 byte
        pdMS_TO_TICKS(100)  // 100ms timeout
    );
}

esp_err_t TCA9555::write(uint8_t reg, uint8_t value) {
    // Register address and data to write
    uint8_t data[2] = {reg, value};

    // Write data to TCA9555 device
    return i2c_master_write_to_device(
        _i2c_num,           // I2C controller number
        _addr,              // TCA9555 chip address
        data,               // Data: [register address, data value]
        2,                  // Length 2 bytes
        pdMS_TO_TICKS(100)  // 100ms timeout
    );
}

esp_err_t TCA9555::set_pin_direction(uint8_t pin, bool is_input) {
    // Check pin number validity (0-15)
    if (pin > 15) {
        ESP_LOGE(TAG, "Invalid pin number: %d (range 0-15)", pin);
        return ESP_ERR_INVALID_ARG;
    }

    // Determine register address: P10-P17 use 0x07, P00-P07 use 0x06
    uint8_t reg = (pin >= 8) ? REG_CONFIG1 : REG_CONFIG0;

    // Calculate pin bit position in register (0-7)
    uint8_t bit = pin % 8;

    // Read current configuration
    uint8_t current;
    esp_err_t ret = read(reg, &current);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read config register failed");
        return ret;
    }

    // Modify corresponding bit: 1=input (high-Z), 0=output
    if (is_input) {
        current |= (1 << bit);   // Set to 1 = input
    } else {
        current &= ~(1 << bit);  // Set to 0 = output
    }

    // Write back configuration
    ret = write(reg, current);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write config register failed");
    }

    return ret;
}

esp_err_t TCA9555::set_port_direction(uint8_t port, uint8_t dir_mask) {
    // Check port number validity (0 or 1)
    if (port > 1) {
        ESP_LOGE(TAG, "Invalid port number: %d (must be 0 or 1)", port);
        return ESP_ERR_INVALID_ARG;
    }

    // Determine register address
    uint8_t reg = (port == 0) ? REG_CONFIG0 : REG_CONFIG1;

    // Directly write 8-bit direction configuration
    // Each bit in dir_mask: 1=input 0=output
    // Example: 0b11111100 means lower 2 bits output, upper 6 bits input
    return write(reg, dir_mask);
}

bool TCA9555::read_pin(uint8_t pin) {
    // Check pin number validity
    if (pin > 15) {
        ESP_LOGE(TAG, "Invalid pin number: %d", pin);
        return false;
    }

    // Determine register address: P10-P17 use 0x01, P00-P07 use 0x00
    uint8_t reg = (pin >= 8) ? REG_INPUT_PORT1 : REG_INPUT_PORT0;

    // Calculate pin bit position in register
    uint8_t bit = pin % 8;

    // Read port state
    uint8_t value;
    if (read(reg, &value) != ESP_OK) {
        ESP_LOGE(TAG, "Read input port failed");
        return false;
    }

    // Extract corresponding bit value and return
    // Example: value=0b10100101, bit=2, result=(value>>2)&1 = 1
    return (value >> bit) & 0x01;
}

esp_err_t TCA9555::read_port(uint8_t port, uint8_t *value) {
    // Check port number validity
    if (port > 1) {
        ESP_LOGE(TAG, "Invalid port number: %d", port);
        return ESP_ERR_INVALID_ARG;
    }

    // Determine register address
    uint8_t reg = (port == 0) ? REG_INPUT_PORT0 : REG_INPUT_PORT1;

    // Read entire port's 8-bit data
    // Each bit in return value represents a pin's level state
    // Example: 0b10100101 means P07=1, P06=0, P05=1, P04=0...
    return read(reg, value);
}

esp_err_t TCA9555::write_pin(uint8_t pin, bool level) {
    // Check pin number validity
    if (pin > 15) {
        ESP_LOGE(TAG, "Invalid pin number: %d", pin);
        return ESP_ERR_INVALID_ARG;
    }

    // Determine register address: P10-P17 use 0x03, P00-P07 use 0x02
    uint8_t reg = (pin >= 8) ? REG_OUTPUT_PORT1 : REG_OUTPUT_PORT0;

    // Calculate pin bit position
    uint8_t bit = pin % 8;

    // Read current output state
    uint8_t current;
    esp_err_t ret = read(reg, &current);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read output port failed");
        return ret;
    }

    // Modify corresponding bit: 1=output high level, 0=output low level
    if (level) {
        current |= (1 << bit);   // Set to 1 = output high
    } else {
        current &= ~(1 << bit);  // Set to 0 = output low
    }

    // Write back to output register
    ret = write(reg, current);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write output port failed");
    }

    return ret;
}

esp_err_t TCA9555::write_port(uint8_t port, uint8_t value) {
    // Check port number validity
    if (port > 1) {
        ESP_LOGE(TAG, "Invalid port number: %d", port);
        return ESP_ERR_INVALID_ARG;
    }

    // Determine register address
    uint8_t reg = (port == 0) ? REG_OUTPUT_PORT0 : REG_OUTPUT_PORT1;

    // Directly write 8-bit output data
    // Each bit in value controls a pin: 1=high level 0=low level
    // Example: 0x01 means P00=high, rest=low
    return write(reg, value);
}

esp_err_t TCA9555::set_pin_polarity(uint8_t pin, bool invert) {
    // Check pin number validity
    if (pin > 15) {
        ESP_LOGE(TAG, "Invalid pin number: %d", pin);
        return ESP_ERR_INVALID_ARG;
    }

    // Determine register address: P10-P17 use 0x05, P00-P07 use 0x04
    uint8_t reg = (pin >= 8) ? REG_POLARITY1 : REG_POLARITY0;

    // Calculate pin bit position
    uint8_t bit = pin % 8;

    // Read current polarity configuration
    uint8_t current;
    esp_err_t ret = read(reg, &current);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read polarity register failed");
        return ret;
    }

    // Modify corresponding bit: 1=invert (read value inverted), 0=normal
    if (invert) {
        current |= (1 << bit);   // Set to 1 = invert
    } else {
        current &= ~(1 << bit);  // Set to 0 = normal
    }

    // Write back to polarity register
    ret = write(reg, current);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write polarity register failed");
    }

    return ret;
}

esp_err_t TCA9555::set_port_polarity(uint8_t port, uint8_t invert_mask) {
    // Check port number validity
    if (port > 1) {
        ESP_LOGE(TAG, "Invalid port number: %d", port);
        return ESP_ERR_INVALID_ARG;
    }

    // Determine register address
    uint8_t reg = (port == 0) ? REG_POLARITY0 : REG_POLARITY1;

    // Directly write 8-bit polarity configuration
    // Each bit in invert_mask: 1=invert 0=normal
    // Example: 0b11110000 means upper 4 bits inverted, lower 4 bits normal
    return write(reg, invert_mask);
}