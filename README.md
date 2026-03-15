# Smart Pet Feeder

An ESP32-S3 based automatic pet feeder with a touchscreen UI, RFID cat identification, food bowl weight monitoring, motion detection, scheduled feeding, and remote control via MQTT.

## Introduction

The system runs on an ESP32-S3 and integrates several hardware components managed by dedicated software components that communicate through a central EventBus. The device connects to WiFi using BLE provisioning on first boot and stores credentials for subsequent connections. Once online, it connects to an MQTT broker to receive remote feed commands and schedule updates, and publishes hardware events (motion, RFID, food level, food eaten) back to the server. The touchscreen provides a local UI for manual feeding, provisioning, and status display.

The camera module runs on a separate **ESP32-P4** board and communicates with the main ESP32-S3 through SPI. Its code is maintained on the `feature/camera` branch. 

## Component Wiring

### Display (ILI9341, SPI)
| Signal | GPIO |
|--------|------|
| SCLK   | 21   |
| MOSI   | 19   |
| MISO   | 20   |
| CS     | 36   |
| DC     | 1    |
| RST    | 37   |
| BL     | 35   |

### Touch Controller (FT5x06, I2C)
| Signal | GPIO |
|--------|------|
| SDA    | 8    |
| SCL    | 9    |
| RST    | 18   |
| INT    | 17   |

### RFID Reader (RC522, SPI — shares bus with display)
| Signal | GPIO |
|--------|------|
| SCLK   | 21   |
| MOSI   | 19   |
| MISO   | 20   |
| CS     | 47   |
| RST    | 48   |

### Food Bowl Scale (HX711)
| Signal | GPIO |
|--------|------|
| DOUT   | 40   |
| SCK    | 39   |

Typical load cell wiring:
- Red → E+
- Black → E-
- White → A- / B-
- Green → A+ / B+

### IR Sensors
| Sensor          | GPIO |
|-----------------|------|
| Food level 75%  | 41   |
| Food level 50%  | 42   |
| Food level 25%  | 2    |
| Motion left     | 4    |
| Motion center   | 5    |
| Motion right    | 6    |

### Stepper Motor
| Signal | GPIO |
|--------|------|
| IN1    | 7    |
| IN2    | 10   |
| IN3    | 14   |
| IN4    | 45   |

### ESP32-P4 Camera Module (SPI)
| Signal  | ESP32-S3 GPIO |
|---------|---------------|
| SCLK    | 12            |
| MOSI    | 11            |
| MISO    | 13            |
| CS      | 15            |
| READY   | 16            |
| REQUEST | 3             |

---

## Food Bowl Scale Calibration

Before building the main app, run the calibration test app once to determine the scale factor for your load cells.

```bash
cd components/bowl_scale/test_apps/bowl_scale_calibration_test
idf.py reconfigure
idf.py build flash monitor
```

Follow the serial monitor instructions:
1. Remove everything from the scale — tare runs automatically after 3 seconds
2. Place a known weight on the bowl within 8 seconds (default: 218 g)
3. Read the `SCALE_FACTOR` value printed in the log
4. Update `BoardConfig::SCALE_FACTOR` in `components/common/include/board_config.h`

---

## Building and Flashing

Always run `reconfigure` first when setting up or after changing `sdkconfig.defaults`. This applies the custom partition table, enables LVGL configuration, increases flash size, and enables Bluetooth.

```bash
idf.py reconfigure
idf.py build
idf.py flash monitor
```