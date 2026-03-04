#include "Display.h"


Display::Display(std::shared_ptr<SPIBus> spi_bus, const Config& config)
    : spi_bus(spi_bus), config(config) {}

Display::~Display()
{
    if (initialized) {
        esp_lcd_panel_del(panel_handle);
        esp_lcd_panel_io_del(panel_io_handle);
        initialized = false;
    }
}

esp_err_t Display::init()
{
    if (initialized) {
        return ESP_OK;
    }
    ESP_ERROR_CHECK(initPanelIO());
    ESP_ERROR_CHECK(initPanel());
    ESP_ERROR_CHECK(initBacklight());
    initialized = true;
    return ESP_OK;
}

//spi panel io interface initialization
esp_err_t Display::initPanelIO()
{
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = config.cs,
        .dc_gpio_num = config.dc,
        .spi_mode = 0,
        .pclk_hz = config.pixel_clock_hz,
        .trans_queue_depth = config.queue_depth,
        .on_color_trans_done = nullptr, //set later via setFlushCompleteCallback()
        .user_ctx = this,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags = {
            .dc_low_on_data = 0,
            .octal_mode = 0,
            .sio_mode = 0,
            .lsb_first = 0,
            .cs_high_active = 0
        }
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)spi_bus->getHostId(), &io_config, &panel_io_handle));
    return ESP_OK;
}

//display driver ili9341 lcd controller initialization
esp_err_t Display::initPanel()
{
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = config.rst,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    
    //must invert the colors for ili9341
    if (config.invert_colors) {
        esp_lcd_panel_io_tx_param(panel_io_handle, 0x21, nullptr, 0); 
    }
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, config.swap_xy));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, config.mirror_x, config.mirror_y));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    return ESP_OK;
}

esp_err_t Display::initBacklight()
{
    gpio_config_t bk_gpio_config = {
        .pin_bit_mask = 1ULL << config.backlight,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(config.backlight, 1); 
    return ESP_OK;
}


/*
lvgl manager will call this to set lv_disp_flush_ready as the callback after panel io init
so that lvgl will get notified when dma trasfer done
once notified, lvgl can prepare the next buffer
*/
void Display::setFlushCompleteCallback(std::function<void()> cb)
{
    flush_done_cb = std::move(cb);
    if (panel_io_handle) {
        esp_lcd_panel_io_callbacks_t cbs = {
            .on_color_trans_done = onColorTransDone
        };
        ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(panel_io_handle, &cbs, this));
    }
}

//invoke by spi dma interrupt when data transfer done
bool Display::onColorTransDone(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t* evdata, void* user_ctx)
{
    auto* self = static_cast<Display*>(user_ctx);
    if (self && self->flush_done_cb) {
        self->flush_done_cb();
    }
    return false;
}

/*
called by lvgl manager in its flush cb
this transfers the data. data transfered via spi dma.
isr triggers when transfer done. onColorTransDone called.
*/
void Display::drawBitmap(int16_t x1, int16_t y1, int16_t x2, int16_t y2, const void* pixel_data)
{
    if (!initialized || !panel_handle) {
        return;
    }
    
    //end point exclusive
    esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, pixel_data);
}