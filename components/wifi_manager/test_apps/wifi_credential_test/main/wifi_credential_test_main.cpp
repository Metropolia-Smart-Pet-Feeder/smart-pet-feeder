#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include <cstring>

static const char* TAG = "wifi_cred_test";

// Do not edit Wi-Fi ssid and password here.
// Instead, run "idf.py menuconfig" to configure
// They will be saved to your own sdkconfig file, which won't be committed to git.

static const char* WIFI_SSID     = CONFIG_TEST_WIFI_SSID;
static const char* WIFI_PASSWORD = CONFIG_TEST_WIFI_PASSWORD;

extern "C" void app_main()
{
    ESP_LOGI(TAG, "WiFi Credential Storage Test");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Minimal WiFi init (needed to use esp_wifi_set_config)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Write credentials to NVS via esp_wifi API
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_LOGI(TAG, "Credentials saved to NVS:");
    ESP_LOGI(TAG, "SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG, "Password: %s", WIFI_PASSWORD);

    // Verify by reading back
    wifi_config_t verify_config = {};
    ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &verify_config));
    ESP_LOGI(TAG, "Verified from NVS: SSID = %s", (char*)verify_config.sta.ssid);

    ESP_LOGI(TAG, "Done. You can now flash wifi_connect_test to verify connection.");
}