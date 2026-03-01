#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H
#include <memory>
#include <string>
#include "esp_wifi.h"
#include "esp_event.h"
#include "wifi_provisioning/manager.h"
#include "EventBus.h"

class WiFiManager
{
public:
    WiFiManager(std::shared_ptr<EventBus> event_bus);
    ~WiFiManager();
    
    // initialize WiFi and check if the credentials already exist
    bool init();
    
    // start BLE provisioning; called when EVENT_START_PROVISIONING is received
    void startProvisioning();
    void stopProvisioning();
    
    bool isConnected() const {return is_connected;};
    std::string getSSID() const {return std::string(current_ssid);};
    std::string getDeviceId() const {return std::string(device_id);};
    void resetCredentials();

private:
    std::shared_ptr<EventBus> event_bus;
    
    bool is_connected;
    bool is_provisioning;
    char current_ssid[33];
    char device_id[20]{0};
    
    esp_event_handler_instance_t wifi_event_handler;
    esp_event_handler_instance_t ip_event_handler;
    
    static void wifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void ipEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    
    // these are called when events are published to EventBus
    static void onStartProvisioningEvent(void* arg, esp_event_base_t base, 
                                        int32_t id, void* data);
    static void onStopProvisioningEvent(void* arg, esp_event_base_t base,
                                       int32_t id, void* data);
    static void onResetCredentialsEvent(void* arg, esp_event_base_t base,
                                       int32_t id, void* data);

    void connectToWiFi();
    bool isProvisioned();
    
    static const char* TAG;
};

#endif