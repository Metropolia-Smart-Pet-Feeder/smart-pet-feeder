#include "SPIBus.h"

SPIBus::SPIBus(spi_host_device_t host_id,
               int mosi, int miso, int sclk,
               int max_transfer_sz)
    : host_id(host_id),
      mosi(mosi),
      miso(miso),
      sclk(sclk),
      max_transfer_sz(max_transfer_sz)
{}

SPIBus::~SPIBus()
{
    if (initialized) {
        spi_bus_free(host_id);
    }
}

esp_err_t SPIBus::init()
{
    if (initialized) {
        return ESP_OK;
    }

    spi_bus_config_t buscfg = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .max_transfer_sz = max_transfer_sz,
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
        .intr_flags = 0
    };

    esp_err_t err = spi_bus_initialize(static_cast<spi_host_device_t>(host_id), &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        return err;
    } else {
        initialized = true;
    }

    return err;
}

spi_host_device_t SPIBus::getHostId() const
{
    return static_cast<spi_host_device_t>(host_id);
}