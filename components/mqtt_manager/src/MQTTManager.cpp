#include "MQTTManager.h"
#include "Events.h"
#include "esp_log.h"
#include <cstring>

const char* MQTTManager::TAG = "MQTTManager";

MQTTManager::MQTTManager(std::shared_ptr<EventBus> event_bus, const Config& config)
    : event_bus(event_bus)
    , config(config)
    , mqtt_client(nullptr)
    , is_connected(false)
{
}

MQTTManager::~MQTTManager()
{
    if (mqtt_client) {
        esp_mqtt_client_destroy(mqtt_client);
    }
}

bool MQTTManager::init()
{
    //configure client
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = config.broker_uri.c_str();
    mqtt_cfg.credentials.client_id = config.client_id.c_str();
    mqtt_cfg.credentials.username = config.username.c_str();
    mqtt_cfg.credentials.authentication.password = config.password.c_str();
    
    // certificate
    if (!config.cert_pem.empty()) {
        mqtt_cfg.broker.verification.certificate = config.cert_pem.c_str();
    }
    
    // create cleint
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return false;
    }
    
    //register event handler
    esp_mqtt_client_register_event(mqtt_client, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), mqttEventHandler, this);
    
    //subscribe to wifi events. auto connect mqtt if wifi connected
    event_bus->subscribe(EVENT_WIFI_CONNECTED, onWiFiConnectedEvent, this);
    event_bus->subscribe(EVENT_WIFI_DISCONNECTED, onWiFiDisconnectedEvent, this);
    
    ESP_LOGI(TAG, "MQTT Manager initialized");
    return true;
}

void MQTTManager::connect()
{
    if (!mqtt_client) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return;
    }
    esp_mqtt_client_start(mqtt_client);
}

void MQTTManager::disconnect()
{
    if (mqtt_client && is_connected) {
        esp_mqtt_client_stop(mqtt_client);
        is_connected = false;
    }
}

bool MQTTManager::publish(const std::string& topic, const std::string& data, int qos)
{
    if (!is_connected) {
        ESP_LOGW(TAG, "Cannot publish, not connected to MQTT broker");
        return false;
    }
    
    int msg = esp_mqtt_client_publish(mqtt_client, topic.c_str(), data.c_str(), data.length(), qos, 0);
    
    if (msg == -1) {
        ESP_LOGE(TAG, "Failed to publish to topic: %s", topic.c_str());
        return false;
    }
    return true;
}

bool MQTTManager::subscribe(const std::string& topic, int qos)
{
    if (!is_connected) {
        ESP_LOGW(TAG, "Cannot subscribe, not connected to MQTT broker");
        return false;
    }
    
    int msg = esp_mqtt_client_subscribe(mqtt_client, topic.c_str(), qos);
    
    if (msg == -1) {
        ESP_LOGE(TAG, "Failed to subscribe to topic: %s", topic.c_str());
        return false;
    }
    
    return true;
}

bool MQTTManager::unsubscribe(const std::string& topic)
{
    if (!is_connected) {
        ESP_LOGW(TAG, "Cannot unsubscribe, not connected to MQTT broker");
        return false;
    }
    
    int msg_id = esp_mqtt_client_unsubscribe(mqtt_client, topic.c_str());
    
    if (msg_id == -1) {
        ESP_LOGE(TAG, "Failed to unsubscribe from topic: %s", topic.c_str());
        return false;
    }
    
    return true;
}

void MQTTManager::mqttEventHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
{
    MQTTManager* manager = static_cast<MQTTManager*>(handler_args);
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
    
    switch (static_cast<esp_mqtt_event_id_t>(event_id)) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            manager->is_connected = true;
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            manager->is_connected = false;
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_DATA:
        {
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            ESP_LOGI(TAG, "Topic: %.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "Data: %.*s", event->data_len, event->data);
            
            // Publish received MQTT message to EventBus
            mqtt_message_data_t msg_data = {};
            
            // Copy topic
            int topic_len = event->topic_len < sizeof(msg_data.topic) - 1 ? 
                           event->topic_len : sizeof(msg_data.topic) - 1;
            memcpy(msg_data.topic, event->topic, topic_len);
            msg_data.topic[topic_len] = '\0';
            
            // Copy data
            int data_len = event->data_len < sizeof(msg_data.data) - 1 ? 
                          event->data_len : sizeof(msg_data.data) - 1;
            memcpy(msg_data.data, event->data, data_len);
            msg_data.data[data_len] = '\0';
            
            manager->event_bus->publish(EVENT_MQTT_MESSAGE_RECEIVED, &msg_data, sizeof(msg_data));
            break;
        }
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Last error code reported from esp-tls: 0x%x", 
                        event->error_handle->esp_tls_last_esp_err);
                ESP_LOGE(TAG, "Last tls stack error number: 0x%x", 
                        event->error_handle->esp_tls_stack_err);
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "Connection refused error: 0x%x", 
                        event->error_handle->connect_return_code);
            }
            break;
        
        case MQTT_EVENT_BEFORE_CONNECT:
        case MQTT_EVENT_DELETED:
        case MQTT_EVENT_ANY:
        case MQTT_USER_EVENT:
            //ignore these now
            break;
            
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

void MQTTManager::onWiFiConnectedEvent(void* arg, esp_event_base_t base,
                                       int32_t id, void* data)
{
    MQTTManager* manager = static_cast<MQTTManager*>(arg);
    manager->connect();
}

void MQTTManager::onWiFiDisconnectedEvent(void* arg, esp_event_base_t base,
                                          int32_t id, void* data)
{
    MQTTManager* manager = static_cast<MQTTManager*>(arg);
    manager->disconnect();
}
