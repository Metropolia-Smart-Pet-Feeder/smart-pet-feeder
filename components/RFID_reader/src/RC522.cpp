#include "RC522.h"
#include "esp_log.h"
#include <cstring>
#include <cstdio>

#define RC522_SPI_CLOCK 250000

//Commands
#define PCD_IDLE        0x00
#define PCD_TRANSCEIVE  0x0C
#define PCD_RESETPHASE  0x0F
//Registers
#define COMMAND_REG     0x01
#define COMM_IRQ_REG    0x04
#define FIFO_DATA_REG   0x09
#define FIFO_LEVEL_REG  0x0A
#define BIT_FRAMING_REG 0x0D
#define MODE_REG        0x11
#define TX_CONTROL_REG  0x14

RC522::RC522(std::shared_ptr<SPIBus> spi_bus, int cs_pin, int rst_pin, std::shared_ptr<EventBus> event_bus) : spi_bus(spi_bus), cs_pin(cs_pin), rst_pin(rst_pin), event_bus(event_bus){}

RC522::~RC522() {
    if(spi) {
        spi_bus_remove_device(spi);
    }
}

esp_err_t RC522::init() {
    if(initialized) {
        return ESP_OK;
    }
    //Reset pin setup
    gpio_set_direction((gpio_num_t)rst_pin, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)rst_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level((gpio_num_t)rst_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Add the RC522 device to the SPI bus
    spi_device_interface_config_t dev_config = {};
    dev_config.clock_speed_hz = RC522_SPI_CLOCK;
    dev_config.mode = 0;
    dev_config.spics_io_num = cs_pin;
    dev_config.queue_size = 1;

    ESP_ERROR_CHECK(spi_bus_add_device(spi_bus->getHostId(), &dev_config, &spi));

    //Soft reset
    write_register(COMMAND_REG, PCD_RESETPHASE);
    vTaskDelay(pdMS_TO_TICKS(50));

    //Initializes the internal timers used during card detection.
    write_register(0x2A, 0x8D); // TModeReg: Timer control register
    write_register(0x2B, 0x3E); //TPrescalerReg: Timer prescaler
    write_register(0x2D, 30); // TReloadRegL: Timer reload value low
    write_register(0x2C, 0x00); // TReloadRegH: Timer reload value high
    write_register(0x15, 0x40); //TxASKReg controls modulation used for sending signals
    write_register(MODE_REG, 0x3D); //ModeReg Defines CRC preset, data framing and protocol

    //Antenna ON
    uint8_t val = read_register(TX_CONTROL_REG);
    write_register(TX_CONTROL_REG,val | 0x03);

    initialized = true;
    return ESP_OK;
}

void RC522::startTask() {
    xTaskCreatePinnedToCore(rfid_task, "rc522_task", 4096, this, 10, &task_handle,tskNO_AFFINITY);
}

esp_err_t RC522::transceive(uint8_t *tx, uint8_t *rx, size_t len) {
    spi_transaction_t t{};
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    return spi_device_transmit(spi, &t);
}

esp_err_t RC522::write_register(uint8_t reg, uint8_t value){
    uint8_t tx[2] = {(uint8_t)((reg << 1) & 0x7E), value};
    uint8_t rx[2];
    return transceive(tx, rx, 2);
}

uint8_t RC522::read_register(uint8_t reg) {
    uint8_t tx[2]={(uint8_t)(((reg << 1) & 0x7E) | 0x80), 0x00}; // MSB=1 for read
    uint8_t rx[2];
    transceive(tx, rx, 2);
    return rx[1];
}

bool RC522::detect_card() {
    // RC522 card detection/REQA sequence
    write_register(COMMAND_REG, PCD_IDLE); // Set RC522 to idle
    write_register(COMM_IRQ_REG, 0x7F); // Clear all interrupt flags
    write_register(FIFO_LEVEL_REG, 0x80); //Flush FIFO buffer
    write_register(FIFO_DATA_REG, 0x26); // Load REQA command into FIFO
    write_register(COMMAND_REG, PCD_TRANSCEIVE); // start transceive command
    write_register(BIT_FRAMING_REG, 0x87); // set bit framing

    for (int i = 0; i < 50; i++) {
        uint8_t irq = read_register(COMM_IRQ_REG);
        if (irq & 0x30)
            return true;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return false;
}

bool RC522::read_UID(uint8_t *uid, uint8_t &uid_len) {
    write_register(COMMAND_REG, PCD_IDLE); // Set RC522 to idle
    write_register(COMM_IRQ_REG, 0x7F); // Clear all interrupt flags
    write_register(FIFO_LEVEL_REG, 0x80); //Flush FIFO buffer
    write_register(FIFO_DATA_REG, 0x93); // Load anti-collision CL1 command into FIFO
    write_register(FIFO_DATA_REG, 0x20); 
    write_register(COMMAND_REG, PCD_TRANSCEIVE); // start transceive command
    write_register(BIT_FRAMING_REG, 0x80); // set bit framing

    for (int i = 0; i < 50; i++) {
        uint8_t irq = read_register(COMM_IRQ_REG);
        if (irq & 0x30) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    uint8_t level = read_register(FIFO_LEVEL_REG);
    if (level < 5) return false;

    for (int i = 0; i < 5; i++){
        uid[i] = read_register(FIFO_DATA_REG);
    }
    uid_len = 4;
    return true;
}

void RC522::rfid_task(void *arg){
    RC522* self = (RC522*)arg;
    uint8_t uid[10];
    uint8_t uid_len;

    while(true) {
        if(self->detect_card()){
            if(self->read_UID(uid, uid_len)){
                char uid_str[32] = {0};
                char *ptr = uid_str;
                for (int i = 0; i < uid_len; i++) {
                    ptr += sprintf(ptr, "%02X", uid[i]);
                }
                self->event_bus->publish(EVENT_RFID_DETECTED, uid_str);
                ESP_LOGI("RC522", "Card detected with UID: %s", uid_str);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
