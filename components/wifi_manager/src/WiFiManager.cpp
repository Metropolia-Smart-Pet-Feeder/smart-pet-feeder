#include "WiFiManager.h"
#include "Events.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "wifi_provisioning/scheme_ble.h"
#include <cstring>
#include "Types.h"

const char* WiFiManager::TAG = "WiFiManager";

WiFiManager::WiFiManager(std::shared_ptr<EventBus> event_bus)
    : event_bus(event_bus)
    , is_connected(false)
    , is_provisioning(false)
    , wifi_event_handler(nullptr)
    , ip_event_handler(nullptr)
{
    memset(current_ssid, 0, sizeof(current_ssid));
    ESP_LOGI(TAG, "WiFiManager created");
}

WiFiManager::~WiFiManager()
{
    if (wifi_event_handler) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler);
    }
    if (ip_event_handler) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler);
    }
    
    esp_wifi_stop();
    esp_wifi_deinit();
    
    ESP_LOGI(TAG, "WiFiManager destroyed");
}

bool WiFiManager::init()
{
    ESP_LOGI(TAG, "Initializing WiFi...");
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,           // Event base
        ESP_EVENT_ANY_ID,     // Listen to ALL WiFi events
        &wifiEventHandler,    // callback. called by event loop wifi event occurs
        this,                
        &wifi_event_handler   // Store handler instance for cleanup
    ));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &ipEventHandler,
        this,
        &ip_event_handler
    ));
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    //subscribe to custom events
    event_bus->subscribe(EVENT_START_PROVISIONING, onStartProvisioningEvent, this);
    event_bus->subscribe(EVENT_RESET_CREDENTIALS, onResetCredentialsEvent, this);
    
    if (isProvisioned()) {
        ESP_LOGI(TAG, "Already provisioned, connecting...");
        connectToWiFi();
    } else {
        ESP_LOGI(TAG, "Not provisioned yet");
        event_bus->publish(EVENT_WIFI_DISCONNECTED);
    }
    
    return true;
}

bool WiFiManager::isProvisioned()
{
    wifi_config_t wifi_cfg;
    // look for wifi congis in nvs flash memory
    if (esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) == ESP_OK) {
        if (strlen((const char*)wifi_cfg.sta.ssid) > 0) {
            ESP_LOGI(TAG, "Found credentials for SSID: %s", wifi_cfg.sta.ssid);
            return true;
        }
    }
    return false;
}

void WiFiManager::connectToWiFi()
{
    ESP_LOGI(TAG, "Connecting to WiFi...");
    esp_wifi_connect();
}


//after provisioning, esp wifi manager will automatically stores wifi credentials and connect wifi
void WiFiManager::startProvisioning()
{
    // If have credentials but not connected, clear them and re-provision
    if (!is_connected && isProvisioned() && !is_provisioning) {
        ESP_LOGW(TAG, "Have credentials but not connected - resetting for re-provisioning");
        resetCredentials();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Check if already connected
    if (is_connected) {
        ESP_LOGI(TAG, "Already connected to WiFi: %s", current_ssid);
        return;
    }
    
    // Check if already provisioning
    if (is_provisioning) {
        ESP_LOGW(TAG, "Already in provisioning mode");
        return;
    }
    
    ESP_LOGI(TAG, "Starting BLE provisioning...");
    
    // config and init provision manager
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        //free both classic bluetooth and bluetooth LE/BTDM memory
        //since we are not using bluetooth for anything else
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,  
        .app_event_handler = WIFI_PROV_EVENT_HANDLER_NONE
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
    
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char service_name[32];
    snprintf(service_name, sizeof(service_name), 
             "PROV_PETFEEDER_%02X%02X%02X", 
             mac[3], mac[4], mac[5]);
    
    ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
        WIFI_PROV_SECURITY_1,  // 1 = encrypted
        NULL,                  // no password
        service_name,          // BLE device name
        NULL                   // not used in BLE
    ));
    
    is_provisioning = true;
    ESP_LOGI(TAG, "BLE provisioning started with name: %s", service_name);
    
    // publish provisioning started event to customized event bus with device name
    provisioning_event_data_t prov_data;
    strncpy(prov_data.device_name, service_name, sizeof(prov_data.device_name) - 1);
    prov_data.device_name[sizeof(prov_data.device_name) - 1] = '\0';
    event_bus->publish(EVENT_PROVISIONING_STARTED, prov_data);
}

void WiFiManager::stopProvisioning()
{
    if (!is_provisioning) {
        return;
    }
    ESP_LOGI(TAG, "Stopping provisioning...");
    wifi_prov_mgr_deinit();
    // Give BLE stack time to clean up
    vTaskDelay(pdMS_TO_TICKS(500));
    is_provisioning = false;
    ESP_LOGI(TAG, "Provisioning stopped");
}

void WiFiManager::resetCredentials()
{
    ESP_LOGI(TAG, "Resetting credentials...");
    
    // Stop provisioning first if active
    if (is_provisioning) {
        stopProvisioning();
    }
    
    // Erase WiFi credentials from NVS
    esp_wifi_restore();
    
    // Disconnect if connected
    if (is_connected) {
        esp_wifi_disconnect();
        is_connected = false;
    }
    
    memset(current_ssid, 0, sizeof(current_ssid));
    event_bus->publish(EVENT_WIFI_DISCONNECTED);
    ESP_LOGI(TAG, "Credentials reset");
}

void WiFiManager::wifiEventHandler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data)
{
    // Cast arg back to WiFiManager* to access our object
    WiFiManager* manager = (WiFiManager*)arg;
    
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi started");
            break;
            
        case WIFI_EVENT_STA_CONNECTED: {
            ESP_LOGI(TAG, "WiFi connected");
            
            // Extract SSID from event data
            wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*)event_data;
            memcpy(manager->current_ssid, event->ssid, sizeof(event->ssid));
            manager->current_ssid[32] = '\0';  
            break;
        }
            
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "WiFi disconnected");
            manager->is_connected = false;
            memset(manager->current_ssid, 0, sizeof(manager->current_ssid));
            
            // Retry connection if not provisioning
            if (!manager->is_provisioning) {
                ESP_LOGI(TAG, "Retrying connection...");
                esp_wifi_connect();
            }
            
            manager->event_bus->publish(EVENT_WIFI_DISCONNECTED);
            break;
            
        default:
            break;
    }
}

void WiFiManager::ipEventHandler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    WiFiManager* manager = (WiFiManager*)arg;
    
    if (event_id == IP_EVENT_STA_GOT_IP) {
        // Extract IP address from event data
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        
        manager->is_connected = true;
        
        // If we were provisioning, stop it now
        if (manager->is_provisioning) {
            manager->stopProvisioning();
        }
        
        // Publish WiFi connected event to custom event bus with data
        wifi_event_data_t wifi_data;
        strncpy(wifi_data.ssid, manager->current_ssid, sizeof(wifi_data.ssid));
        manager->event_bus->publish(EVENT_WIFI_CONNECTED, wifi_data);
    }
}


//wrappers
void WiFiManager::onStartProvisioningEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    WiFiManager* manager = (WiFiManager*)arg;
    manager->startProvisioning();
}

void WiFiManager::onStopProvisioningEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    WiFiManager* manager = (WiFiManager*)arg;
    manager->stopProvisioning();
}

void WiFiManager::onResetCredentialsEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    WiFiManager* manager = (WiFiManager*)arg;
    manager->resetCredentials();
}
