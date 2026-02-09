#ifndef SPIBUS_H
#define SPIBUS_H

#pragma once

#include "driver/spi_master.h"

class SPIBus
{
public:
    SPIBus(spi_host_device_t host_id, int mosi, int miso, int sclk,  int max_transfer_sz);
    ~SPIBus();

    esp_err_t init();
    spi_host_device_t getHostId() const;

private:
    spi_host_device_t host_id;
    int mosi;
    int miso;
    int sclk;
    int max_transfer_sz;
    bool initialized{false};
};

#endif