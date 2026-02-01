// TCA9555.h
#ifndef TCA9555_H
#define TCA9555_H

#include "driver/i2c.h"
#include "esp_err.h"

class TCA9555 {
public:
    /**
     * Constructor - Initialize TCA9555 chip
     * @param i2c_num I2C controller number, typically I2C_NUM_0
     * @param addr Chip address, default 0x20 (A2=A1=A0=GND)
     * Example: TCA9555 io(I2C_NUM_0, 0x20);
     */
    TCA9555(i2c_port_t i2c_num = I2C_NUM_0, uint8_t addr = 0x20);

    /**
     * Initialize I2C bus
     * @param sda_pin SDA data line pin, example: GPIO_NUM_8
     * @param scl_pin SCL clock line pin, example: GPIO_NUM_9
     * @param freq Frequency, example: 400000 (400kHz fast mode)
     * @return ESP_OK=success, other=failure
     * Example: io.init(GPIO_NUM_8, GPIO_NUM_9, 400000);
     */
    esp_err_t init(gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t freq = 400000);

    // ========== Low-level Read/Write ==========
    /**
     * Read register - Read one byte from chip
     * @param reg Register address, see register list below
     * @param value Read data stored here
     * @return ESP_OK=success
     * Example: uint8_t data; io.read(REG_INPUT_PORT0, &data);
     */
    esp_err_t read(uint8_t reg, uint8_t *value);

    /**
     * Write register - Write one byte to chip
     * @param reg Register address
     * @param value Data to write
     * @return ESP_OK=success
     * Example: io.write(REG_OUTPUT_PORT0, 0x01); // Write 0x01 to output port 0
     */
    esp_err_t write(uint8_t reg, uint8_t value);

    // ========== Pin Direction Configuration ==========
    /**
     * Set pin direction - Configure a pin as input or output
     * @param pin Pin number 0-15 (P00-P07=0-7, P10-P17=8-15)
     * @param is_input true=input (high-Z, read button/sensor), false=output (control LED/relay)
     * @return ESP_OK=success
     * Example: io.set_pin_direction(0, false);  // P00 as output, control LED
     * Example: io.set_pin_direction(5, true);   // P05 as input, read button
     */
    esp_err_t set_pin_direction(uint8_t pin, bool is_input);

    /**
     * Set port direction - Configure 8 pins at once
     * @param port Port number: 0=P00-P07, 1=P10-P17
     * @param dir_mask Direction mask, each bit represents a pin, 1=input 0=output
     * @return ESP_OK=success
     * Example: io.set_port_direction(0, 0b11111100); // P00-P01 output, P02-P07 input
     */
    esp_err_t set_port_direction(uint8_t port, uint8_t dir_mask);

    // ========== Read Pin State ==========
    /**
     * Read single pin level - Read current pin high/low state
     * @param pin Pin number 0-15
     * @return true=high level (button released/sensor triggered), false=low level (button pressed/sensor inactive)
     * Note: Can read output register value even if configured as output
     * Example: bool pressed = !io.read_pin(5); // P05 with button (pressed=GND), pressed=true
     */
    bool read_pin(uint8_t pin);

    /**
     * Read port state - Read 8 pins at once
     * @param port Port number: 0=P00-P07, 1=P10-P17
     * @param value Read 8-bit data stored here
     * @return ESP_OK=success
     * Example: uint8_t status; io.read_port(0, &status); // Read P00-P07 state
     *       if(status & 0x01) { LED on } // Check if P00 is high
     */
    esp_err_t read_port(uint8_t port, uint8_t *value);

    // ========== Write Pin Level ==========
    /**
     * Write single pin level - Control output pin high/low
     * @param pin Pin number 0-15 (must be configured as output first)
     * @param level true=output high (LED off/relay open), false=output low (LED on/relay closed)
     * @return ESP_OK=success
     * Example: io.write_pin(0, true);  // P00 output high, LED off
     * Example: io.write_pin(0, false); // P00 output low, LED on (if LED connected to VCC)
     */
    esp_err_t write_pin(uint8_t pin, bool level);

    /**
     * Write port level - Control 8 pins at once
     * @param port Port number: 0=P00-P07, 1=P10-P17
     * @param value 8-bit data, each bit controls a pin, 1=high 0=low
     * @return ESP_OK=success
     * Example: io.write_port(0, 0x01); // P00 output high, rest low
     * Example: io.write_port(0, 0xFF); // P00-P07 all output high
     */
    esp_err_t write_port(uint8_t port, uint8_t value);

    // ========== Polarity Inversion ==========
    /**
     * Set pin polarity inversion - Invert read value (0 becomes 1, 1 becomes 0)
     * @param pin Pin number 0-15
     * @param invert true=invert (button pressed reads 1), false=normal (button pressed reads 0)
     * @return ESP_OK=success
     * Use case: When button connected to VCC, pressed=high, set invert to read low for easier logic
     * Example: io.set_pin_polarity(5, true); // P05 inverted, button pressed reads true
     */
    esp_err_t set_pin_polarity(uint8_t pin, bool invert);

    /**
     * Set port polarity inversion - Configure 8 pins at once
     * @param port Port number: 0=P00-P07, 1=P10-P17
     * @param invert_mask Inversion mask, 1=invert 0=normal
     * @return ESP_OK=success
     * Example: io.set_port_polarity(0, 0b11110000); // P04-P07 inverted, P00-P03 normal
     */
    esp_err_t set_port_polarity(uint8_t port, uint8_t invert_mask);

    // ========== Register Address Constants ==========
    static const uint8_t REG_INPUT_PORT0   = 0x00; // Input port 0 (P00-P07) read-only
    static const uint8_t REG_INPUT_PORT1   = 0x01; // Input port 1 (P10-P17) read-only
    static const uint8_t REG_OUTPUT_PORT0  = 0x02; // Output port 0 (P00-P07) read/write
    static const uint8_t REG_OUTPUT_PORT1  = 0x03; // Output port 1 (P10-P17) read/write
    static const uint8_t REG_POLARITY0     = 0x04; // Polarity inversion 0 (P00-P07) read/write
    static const uint8_t REG_POLARITY1     = 0x05; // Polarity inversion 1 (P10-P17) read/write
    static const uint8_t REG_CONFIG0       = 0x06; // Direction config 0 (P00-P07) read/write, 1=input 0=output
    static const uint8_t REG_CONFIG1       = 0x07; // Direction config 1 (P10-P17) read/write, 1=input 0=output

private:
    i2c_port_t _i2c_num;
    uint8_t _addr;
};

#endif