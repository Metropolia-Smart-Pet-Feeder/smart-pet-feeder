#include "MQTTManager.h"
#include "Events.h"
#include "esp_log.h"
#include <cstring>
#include "Types.h"
#include "cJSON.h"

const char* MQTTManager::TAG = "MQTTManager";

MQTTManager::MQTTManager(std::shared_ptr<EventBus> event_bus, const Config& config)
    : event_bus(event_bus)
    , config(config)
    , topic_event("petfeeder/" + config.device_id + "/event")
    , topic_command("petfeeder/" + config.device_id + "/command")
    , mqtt_client(nullptr)
    , is_connected(false)
{
    ESP_LOGI(TAG, "Event topic:   %s", topic_event.c_str());
    ESP_LOGI(TAG, "Command topic: %s", topic_command.c_str());
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
    event_bus->subscribe(EVENT_FEED_COMPLETED, onFeedCompltedEvent, this);
    event_bus->subscribe(EVENT_FOOD_LEVEL_CHANGED, onFoodLevelChangedEvent, this);
    event_bus->subscribe(EVENT_CAT_APPROACHED, onCatApproachedEvent, this);
    event_bus->subscribe(EVENT_CAT_LEFT, onCatLeftEvent, this);
    event_bus->subscribe(EVENT_RFID_DETECTED, onRfidDetectedEvent, this);
    event_bus->subscribe(EVENT_FOOD_EATEN, onFoodEatenEvent, this);
    
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

void MQTTManager::mqttEventHandler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
{
    MQTTManager* manager = static_cast<MQTTManager*>(handler_args);
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
    
    switch (static_cast<esp_mqtt_event_id_t>(event_id)) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            manager->is_connected = true;
            esp_mqtt_client_subscribe(manager->mqtt_client, manager->topic_command.c_str(), 1);
            ESP_LOGI(TAG, "Subscribed to command topic: %s", manager->topic_command.c_str());
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

            // null-terminate data for parsing
            char data_buf[256] = {};
            int data_len = event->data_len < (int)sizeof(data_buf) - 1 ? event->data_len : (int)sizeof(data_buf) - 1;
            memcpy(data_buf, event->data, data_len);

            cJSON* root = cJSON_Parse(data_buf);
            if (!root) {
                ESP_LOGW(TAG, "Failed to parse command JSON");
                break;
            }

            const cJSON* type_item = cJSON_GetObjectItem(root, "type");
            if (!cJSON_IsString(type_item)) {
                ESP_LOGW(TAG, "Command missing 'type' field");
                cJSON_Delete(root);
                break;
            }

            const char* type = type_item->valuestring;

            if (strcmp(type, "feed") == 0) {
                const cJSON* amount_item = cJSON_GetObjectItem(root, "amount");
                int amount = cJSON_IsNumber(amount_item) ? amount_item->valueint : 1;
                feed_request_t request = {};
                request.portions = static_cast<uint8_t>(amount);
                request.source   = FEED_SOURCE_REMOTE;
                ESP_LOGI(TAG, "Feed command: %d portions", amount);
                manager->event_bus->publish(EVENT_FEED_REQUEST, request);

            } else if (strcmp(type, "schedule") == 0) {
                const cJSON* arr = cJSON_GetObjectItem(root, "schedules");
                if (cJSON_IsArray(arr)) {
                    schedule_list_t list = {};
                    int count = cJSON_GetArraySize(arr);
                    list.count = static_cast<uint8_t>(count > MAX_SCHEDULES ? MAX_SCHEDULES : count);
                    for (int i = 0; i < list.count; i++) {
                        const cJSON* entry   = cJSON_GetArrayItem(arr, i);
                        list.schedules[i].hour     = static_cast<uint8_t>(cJSON_GetObjectItem(entry, "hour")->valueint);
                        list.schedules[i].minute   = static_cast<uint8_t>(cJSON_GetObjectItem(entry, "minute")->valueint);
                        list.schedules[i].portions = static_cast<uint8_t>(cJSON_GetObjectItem(entry, "amount")->valueint);
                    }
                    const cJSON* tz = cJSON_GetObjectItem(root, "utc_offset_minutes");
                    list.utc_offset_minutes = tz ? static_cast<int16_t>(tz->valueint) : 0;
                    ESP_LOGI(TAG, "Schedule command: %d entries, UTC offset: %d min", list.count, list.utc_offset_minutes);
                    manager->event_bus->publish(EVENT_SCHEDULE_UPDATE, list);
                }

            } else {
                ESP_LOGW(TAG, "Unknown command type: %s", type);
            }

            cJSON_Delete(root);
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

void MQTTManager::onFeedCompltedEvent(void* arg, esp_event_base_t base,
                                       int32_t id, void* data)
{
    MQTTManager* manager = static_cast<MQTTManager*>(arg);
    const feed_status_t* feed_status = static_cast<feed_status_t*>(data);
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"type\":\"dispense\",\"amount\":%d}", feed_status->portions);
    manager->publish(manager->topic_event, payload);    
}

void MQTTManager::onRfidDetectedEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    MQTTManager* manager = static_cast<MQTTManager*>(arg);
    const char* rfid_id = static_cast<const char*>(data);
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"type\":\"cat_identified\",\"rfid\":\"%s\"}", rfid_id);
    manager->publish(manager->topic_event, payload);
}

void MQTTManager::onFoodLevelChangedEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    MQTTManager* manager = static_cast<MQTTManager*>(arg);
    int level = *static_cast<int*>(data);
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"type\":\"tank_level\",\"level\":%d}", level);
    manager->publish(manager->topic_event, payload);
}

void MQTTManager::onCatApproachedEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    MQTTManager* manager = static_cast<MQTTManager*>(arg);
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"type\":\"cat_came\"}");
    manager->publish(manager->topic_event, payload);
}

void MQTTManager::onCatLeftEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    MQTTManager* manager = static_cast<MQTTManager*>(arg);
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"type\":\"cat_leave\"}");
    manager->publish(manager->topic_event, payload);
}

void MQTTManager::onFoodEatenEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    MQTTManager* manager = static_cast<MQTTManager*>(arg);
    float food_eaten = *static_cast<float*>(data);
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"type\":\"food_eaten\",\"amount\":%f}", food_eaten);
    manager->publish(manager->topic_event, payload);    
}
