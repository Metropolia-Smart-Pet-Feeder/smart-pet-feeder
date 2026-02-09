#ifndef MQTTMANAGER_H
#define MQTTMANAGER_H

#include <memory>
#include <string>
#include "mqtt_client.h"
#include "EventBus.h"

class MQTTManager
{
public:
    struct Config {
        std::string broker_uri;      
        std::string client_id;       
        std::string username;        
        std::string password;        
        std::string cert_pem;       
    };

    MQTTManager(std::shared_ptr<EventBus> event_bus, const Config& config);
    ~MQTTManager();
    
    bool init();
    void connect();
    void disconnect();
    bool publish(const std::string& topic, const std::string& data, int qos = 0);
    bool subscribe(const std::string& topic, int qos = 0);
    bool unsubscribe(const std::string& topic);
    bool isConnected() const { return is_connected; }

private:
    std::shared_ptr<EventBus> event_bus;
    Config config;
    
    esp_mqtt_client_handle_t mqtt_client;
    bool is_connected;
    
    // mqtt event handler
    static void mqttEventHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
    
    //auto start and stop mqtt on wifi events
    static void onWiFiConnectedEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
    static void onWiFiDisconnectedEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
    
    static const char* TAG;
};

#endif
