#ifndef UI_H
#define UI_H

#pragma once

#include "lvgl.h"
#include "LVGLManager.h"
#include "EventBus.h"
#include <memory>

//my question: how do we update the ui? does the ui keep a copy of all the data or does ui activly get the data somewhere?
class UI
{
public:
    explicit UI(std::shared_ptr<LVGLManager> lvgl_manager, std::shared_ptr<EventBus> event_bus);
    ~UI();
    void build();
    
    // ui update interfaces
    // void updateFoodLevel(int percentage, float weight_kg);
    // void updateLastFedTime(const char* time_str);

private:
    std::shared_ptr<LVGLManager> lvgl_manager;
    std::shared_ptr<EventBus> event_bus;
    
    // WiFi state
    bool is_wifi_connected{false};
    char wifi_ssid[32]{0};
    
    // Screens
    lv_obj_t* main_screen{nullptr};
    lv_obj_t* schedule_screen{nullptr};
    lv_obj_t* feeding_screen{nullptr};
    lv_obj_t* provisioning_screen{nullptr};
    lv_obj_t* wifi_status_screen{nullptr};
    
    // UI elements (main screen)
    lv_obj_t* food_level_bar{nullptr};
    lv_obj_t* food_level_label{nullptr};
    lv_obj_t* last_fed_label{nullptr};
    lv_obj_t* btn_feed_now{nullptr};
    lv_obj_t* btn_schedule{nullptr};
    lv_obj_t* btn_history{nullptr};
    lv_obj_t* btn_connect{nullptr};
    lv_obj_t* wifi_icon{nullptr};
    
    // UI elements (provisioning screen)
    lv_obj_t* prov_device_name_label{nullptr};
    
    // UI elements (status screen)
    lv_obj_t* status_ssid_label{nullptr};
    
    // Helper methods
    void buildMainScreen();
    void buildScheduleScreen();
    void buildFeedingScreen();
    void buildProvisioningScreen();
    void buildWiFiStatusScreen();
    void showMainScreen();
    void showScheduleScreen();
    void showFeedingScreen();
    void showProvisioningScreen();
    void showWiFiStatusScreen();
    void updateWiFiIcon();
    
    // Event callbacks (LVGL button clicks)
    static void onFeedNowClicked(lv_event_t* event);
    static void onScheduleClicked(lv_event_t* event);
    static void onHistoryClicked(lv_event_t* event);
    static void onConnectClicked(lv_event_t* event);
    static void onBackClicked(lv_event_t* event);
    static void onResetWiFiClicked(lv_event_t* event);
    static void onCancelProvisioningClicked(lv_event_t* event);
    
    // Event callbacks (EventBus subscriptions)
    static void onWiFiConnectedEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
    static void onWiFiDisconnectedEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
    static void onProvisioningStartedEvent(void* arg, esp_event_base_t base, int32_t id, void* data);


};

#endif