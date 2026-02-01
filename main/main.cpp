// TCA9555.cpp - Complete usage examples

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "TCA9555.h"

static const char *TAG = "MAIN";

// Create TCA9555 object
// Parameter 1: I2C_NUM_0 = use I2C controller 0
// Parameter 2: 0x20 = chip address (0x20 when A2=A1=A0 connected to GND)
TCA9555 io(I2C_NUM_0, 0x20);

/**
 * Example 1: LED blink
 * Hardware connection: P00 -> LED -> Resistor -> GND
 */
void example_led_blink(void *param) {
    ESP_LOGI(TAG, "=== Example 1: LED Blink ===");

    // Set P00 as output mode
    io.set_pin_direction(0, false);
    io.set_pin_direction(3, false);

    while (1) {
        // LED off (0-7)
        io.write_pin(3, true);
        ESP_LOGI(TAG, "LED ON");
        vTaskDelay(pdMS_TO_TICKS(1000));

        // LED on (0-7)
        io.write_pin(3, false);
        ESP_LOGI(TAG, "LED OFF");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * Example 2: Read button state
 * Hardware connection: P05 -> Button -> GND (other end of button connects to P05)
 *          P05 needs external pull-up to VCC, or enable chip's internal 100kΩ pull-up
 */
void example_button_read(void *param) {
    ESP_LOGI(TAG, "=== Example 2: Read Button State ===");

    // Set P05 as input mode
    io.set_pin_direction(5, true);

    bool last_state = false;

    while (1) {
        // Read P05 pin level
        // false=button pressed (grounded), true=button released (pulled high)
        bool current_state = io.read_pin(5);

        // Detect state change
        if (current_state != last_state) {
            if (!current_state) {
                ESP_LOGI(TAG, "Button pressed");
            } else {
                ESP_LOGI(TAG, "Button released");
            }
            last_state = current_state;
        }

        vTaskDelay(pdMS_TO_TICKS(50));  // Poll every 50ms
    }
}

/**
 * Example 3: Control multiple LEDs (using port write)
 * Hardware connection: P00-P07 connected to 8 LEDs respectively
 */
void example_multiple_leds(void *param) {
    ESP_LOGI(TAG, "=== Example 3: Control 8 LEDs ===");

    // Set P00-P07 all as output
    // 0b00000000 = all 8 bits are 0 = all output
    io.set_port_direction(0, 0b00000000);

    while (1) {
        // Running LED effect: light up from P00 to P07 sequentially
        for (int i = 0; i < 8; i++) {
            io.write_port(0, (1 << i));  // Only light up the i-th LED
            ESP_LOGI(TAG, "Light up LED%d", i);
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        // All on
        io.write_port(0, 0xFF);
        ESP_LOGI(TAG, "All ON");
        vTaskDelay(pdMS_TO_TICKS(500));

        // All off
        io.write_port(0, 0x00);
        ESP_LOGI(TAG, "All OFF");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * Example 4: Read multiple buttons (using port read)
 * Hardware connection: P10-P17 connected to 8 buttons to GND respectively
 */
void example_keypad(void *param) {
    ESP_LOGI(TAG, "=== Example 4: Read 8-key Keypad ===");

    // Set P10-P17 all as input
    // 0b11111111 = all 8 bits are 1 = all input
    io.set_port_direction(1, 0b11111111);

    uint8_t last_keys = 0xFF;  // Last key state (all released)

    while (1) {
        // Read entire port 1 state (P10-P17)
        uint8_t current_keys;
        io.read_port(1, &current_keys);

        // Detect key changes
        uint8_t changed = last_keys ^ current_keys;  // XOR to find changed bits

        if (changed) {
            for (int i = 0; i < 8; i++) {
                if (changed & (1 << i)) {  // i-th key state changed
                    if (!(current_keys & (1 << i))) {
                        ESP_LOGI(TAG, "Key P1%d pressed", i);
                    } else {
                        ESP_LOGI(TAG, "Key P1%d released", i);
                    }
                }
            }
            last_keys = current_keys;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * Example 5: Polarity inversion application
 * Use case: When button connected to VCC, pressed=high level, set inversion to read false for easier logic
 * Hardware connection: P04 -> Button -> VCC (P04 goes high when button pressed)
 */
void example_polarity_invert(void *param) {
    ESP_LOGI(TAG, "=== Example 5: Polarity Inversion ===");

    // Set P04 as input
    io.set_pin_direction(4, true);

    // Set P04 polarity inversion
    // Now: pressed=high level inverted to low, reads false
    //      released=low level inverted to high, reads true
    io.set_pin_polarity(4, true);

    while (1) {
        bool pressed = !io.read_pin(4);  // Reading false means pressed

        if (pressed) {
            ESP_LOGI(TAG, "Button pressed (reads false after inversion)");
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * Example 6: Mixed use - LED and button interaction
 * Hardware connection: P00 -> LED -> GND
 *          P05 -> Button -> GND
 */
void example_led_button_interactive(void *param) {
    ESP_LOGI(TAG, "=== Example 6: Button Controls LED ===");

    // Configure P00 as output (LED), P05 as input (button)
    io.set_pin_direction(0, false);  // P00 output
    io.set_pin_direction(5, true);   // P05 input

    bool led_state = false;
    bool last_button = true;

    while (1) {
        bool button = io.read_pin(5);

        // Detect button falling edge (moment of press)
        if (!button && last_button) {
            // Toggle LED state
            led_state = !led_state;
            io.write_pin(0, led_state);
            ESP_LOGI(TAG, "Button pressed, LED %s", led_state ? "ON" : "OFF");
        }

        last_button = button;
        vTaskDelay(pdMS_TO_TICKS(20));  // Debounce delay
    }
}

/**
 * Example 7: Low-level register operations
 * Direct register read/write (advanced usage)
 */
void example_register_access(void *param) {
    ESP_LOGI(TAG, "=== Example 7: Register Operations ===");

    // Method 1: Using constants
    io.write(TCA9555::REG_CONFIG0, 0b11111100);  // P00-P01 output, P02-P07 input

    // Method 2: Direct register address write
    io.write(0x02, 0x01);  // Output port 0 write 0x01

    // Read register
    uint8_t value;
    io.read(TCA9555::REG_INPUT_PORT0, &value);
    ESP_LOGI(TAG, "Input port 0 state: 0x%02X", value);

    vTaskDelete(NULL);
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "TCA9555 complete example program started");

    // Initialize I2C and TCA9555
    // Parameters: SDA pin=GPIO8, SCL pin=GPIO9, frequency=400kHz
    esp_err_t ret = io.init(GPIO_NUM_8, GPIO_NUM_9, 400000);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TCA9555 initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "TCA9555 initialized successfully");

    // ===== Select example to run (uncomment one of them) =====

    // Example 1: Single LED blink
    // xTaskCreate(example_led_blink, "led_blink", 2048, NULL, 5, NULL);

    // Example 2: Read single button
    // xTaskCreate(example_button_read, "button_read", 2048, NULL, 5, NULL);

    // Example 3: Control 8 LED running lights
    xTaskCreate(example_multiple_leds, "multi_leds", 2048, NULL, 5, NULL);

    // Example 4: Read 8 buttons
    // xTaskCreate(example_keypad, "keypad", 2048, NULL, 5, NULL);

    // Example 5: Polarity inversion
    // xTaskCreate(example_polarity_invert, "polarity", 2048, NULL, 5, NULL);

    // Example 6: Button controls LED
    // xTaskCreate(example_led_button_interactive, "interactive", 2048, NULL, 5, NULL);

    // Example 7: Register operations
    // xTaskCreate(example_register_access, "register", 2048, NULL, 5, NULL);
}