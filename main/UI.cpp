#include "UI.h"
#include "Events.h"
#include "esp_log.h"

static const char* TAG = "UI";

UI::UI(std::shared_ptr<LVGLManager> lvgl_manager, std::shared_ptr<EventBus> event_bus)
    : lvgl_manager(lvgl_manager), event_bus(event_bus)
{
}

UI::~UI()
{
}

void UI::build()
{
    ESP_LOGI(TAG, "Building UI...");
    
    // Subscribe to WiFi events for UI updates
    event_bus->subscribe(EVENT_WIFI_CONNECTED, onWiFiConnectedEvent, this);
    event_bus->subscribe(EVENT_WIFI_DISCONNECTED, onWiFiDisconnectedEvent, this);
    event_bus->subscribe(EVENT_PROVISIONING_STARTED, onProvisioningStartedEvent, this);
    
    // Create main screen
    main_screen = lv_obj_create(NULL);
    buildMainScreen();
    
    // Load main screen
    lv_scr_load(main_screen);
    
    ESP_LOGI(TAG, "UI built successfully");
}

void UI::buildMainScreen()
{
    // Set screen background
    lv_obj_set_style_bg_color(main_screen, lv_color_hex(0xF5F5F5), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(main_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // header - use static text and fixed position
    lv_obj_t* header = lv_label_create(main_screen);
    lv_label_set_text_static(header, LV_SYMBOL_HOME " Pet Feeder");
    lv_obj_set_style_text_color(header, lv_color_hex(0x003D5B), LV_PART_MAIN);
    lv_obj_set_pos(header, 65, 8);  // Fixed position instead of align
    lv_obj_clear_flag(header, LV_OBJ_FLAG_CLICKABLE);
    
    // WiFi icon
    wifi_icon = lv_label_create(main_screen);
    lv_label_set_text_static(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x999999), LV_PART_MAIN);
    lv_obj_set_pos(wifi_icon, 210, 8);
    lv_obj_clear_flag(wifi_icon, LV_OBJ_FLAG_CLICKABLE);
    
    // food level
    lv_obj_t* level_container = lv_obj_create(main_screen);
    lv_obj_set_size(level_container, 220, 68);
    lv_obj_set_pos(level_container, 10, 33);
    lv_obj_set_style_bg_color(level_container, lv_color_hex(0x30638E), LV_PART_MAIN);
    lv_obj_set_style_border_width(level_container, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(level_container, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(level_container, 8, LV_PART_MAIN);
    lv_obj_clear_flag(level_container, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    
    lv_obj_t* level_title = lv_label_create(level_container);
    lv_label_set_text_static(level_title, "Food Level");
    lv_obj_set_style_text_color(level_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_pos(level_title, 0, 0);  // Fixed position
    lv_obj_clear_flag(level_title, LV_OBJ_FLAG_CLICKABLE);
    
    food_level_bar = lv_bar_create(level_container);
    lv_obj_set_size(food_level_bar, 200, 12);
    lv_obj_set_pos(food_level_bar, 0, 20);  // Fixed position
    lv_bar_set_value(food_level_bar, 75, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(food_level_bar, lv_color_hex(0xEDAE49), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(food_level_bar, lv_color_hex(0x003D5B), LV_PART_MAIN);
    lv_obj_clear_flag(food_level_bar, LV_OBJ_FLAG_CLICKABLE);
    
    food_level_label = lv_label_create(level_container);
    lv_label_set_text(food_level_label, "75% (1.5kg)");
    lv_obj_set_style_text_color(food_level_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_pos(food_level_label, 0, 38);  // Fixed position
    lv_obj_clear_flag(food_level_label, LV_OBJ_FLAG_CLICKABLE);
    
    // last feeding data
    lv_obj_t* last_fed_container = lv_obj_create(main_screen);
    lv_obj_set_size(last_fed_container, 220, 50);
    lv_obj_set_pos(last_fed_container, 10, 108);
    lv_obj_set_style_bg_color(last_fed_container, lv_color_hex(0x00798C), LV_PART_MAIN);
    lv_obj_set_style_border_width(last_fed_container, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(last_fed_container, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(last_fed_container, 8, LV_PART_MAIN);
    lv_obj_clear_flag(last_fed_container, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    
    lv_obj_t* last_fed_title = lv_label_create(last_fed_container);
    lv_label_set_text_static(last_fed_title, "Last Fed");
    lv_obj_set_style_text_color(last_fed_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_pos(last_fed_title, 0, 0);  // Fixed position
    lv_obj_clear_flag(last_fed_title, LV_OBJ_FLAG_CLICKABLE);
    
    last_fed_label = lv_label_create(last_fed_container);
    lv_label_set_text(last_fed_label, "Jan 4, 2:30 PM (2h ago)");
    lv_obj_set_style_text_color(last_fed_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_pos(last_fed_label, 0, 24);  // Fixed position
    lv_obj_clear_flag(last_fed_label, LV_OBJ_FLAG_CLICKABLE);
    
    // menu buttons
    lv_obj_t* grid_container = lv_obj_create(main_screen);
    lv_obj_set_size(grid_container, 220, 138);
    lv_obj_set_pos(grid_container, 10, 168);
    lv_obj_set_style_bg_opa(grid_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(grid_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(grid_container, 0, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(grid_container, true, LV_PART_MAIN);
    
    // 2*2 grid
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(grid_container, col_dsc, row_dsc);
    lv_obj_set_style_pad_column(grid_container, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_row(grid_container, 10, LV_PART_MAIN);
    lv_obj_set_layout(grid_container, LV_LAYOUT_GRID);
    
    // manual feed button
    btn_feed_now = lv_btn_create(grid_container);
    lv_obj_set_grid_cell(btn_feed_now, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_bg_color(btn_feed_now, lv_color_hex(0xD1495B), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_feed_now, 8, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_feed_now, onFeedNowClicked, LV_EVENT_CLICKED, this);
    
    lv_obj_t* feed_label = lv_label_create(btn_feed_now);
    lv_label_set_text_static(feed_label, LV_SYMBOL_DOWNLOAD "\nFEED NOW");
    lv_obj_set_style_text_align(feed_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(feed_label);
    lv_obj_clear_flag(feed_label, LV_OBJ_FLAG_CLICKABLE);
    
    // schedule feeding time button
    btn_schedule = lv_btn_create(grid_container);
    lv_obj_set_grid_cell(btn_schedule, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_bg_color(btn_schedule, lv_color_hex(0x30638E), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_schedule, 8, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_schedule, onScheduleClicked, LV_EVENT_CLICKED, this);
    
    lv_obj_t* schedule_label = lv_label_create(btn_schedule);
    lv_label_set_text_static(schedule_label, LV_SYMBOL_BELL "\nSCHEDULE");
    lv_obj_set_style_text_align(schedule_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(schedule_label);
    lv_obj_clear_flag(schedule_label, LV_OBJ_FLAG_CLICKABLE);
    
    // check history
    btn_history = lv_btn_create(grid_container);
    lv_obj_set_grid_cell(btn_history, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_style_bg_color(btn_history, lv_color_hex(0x00798C), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_history, 8, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_history, onHistoryClicked, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* history_label = lv_label_create(btn_history);
    lv_label_set_text_static(history_label, LV_SYMBOL_LIST "\nHISTORY");
    lv_obj_set_style_text_align(history_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(history_label);
    lv_obj_clear_flag(history_label, LV_OBJ_FLAG_CLICKABLE);
    
    // connect to phone
    btn_connect = lv_btn_create(grid_container);
    lv_obj_set_grid_cell(btn_connect, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_style_bg_color(btn_connect, lv_color_hex(0xEDAE49), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_connect, 8, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_connect, onConnectClicked, LV_EVENT_CLICKED, this);
    
    lv_obj_t* connect_label = lv_label_create(btn_connect);
    lv_label_set_text_static(connect_label, LV_SYMBOL_WIFI "\nCONNECT");
    lv_obj_set_style_text_align(connect_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(connect_label);
    lv_obj_clear_flag(connect_label, LV_OBJ_FLAG_CLICKABLE);
}

void UI::onFeedNowClicked(lv_event_t* event)
{
    ESP_LOGI(TAG, "Feed Now clicked");
    
    UI* ui = (UI*)lv_event_get_user_data(event);
    if (ui) {
        ui->event_bus->publish(EVENT_FEED_NOW_PRESSED);
        ui->showFeedingScreen();
    }
}

void UI::onScheduleClicked(lv_event_t* event)
{
    ESP_LOGI(TAG, "Schedule clicked");
    UI* ui = (UI*)lv_event_get_user_data(event);
    if (ui) {
        ui->showScheduleScreen();
    }
}

void UI::onHistoryClicked(lv_event_t* event)
{
    ESP_LOGI(TAG, "History clicked");
    
}

void UI::onConnectClicked(lv_event_t* event)
{
    ESP_LOGI(TAG, "Connect clicked");
    
    UI* ui = (UI*)lv_event_get_user_data(event);
    if (ui) {
        if (ui->is_wifi_connected) {
            ui->showWiFiStatusScreen();
        } else {
            ui->event_bus->publish(EVENT_START_PROVISIONING);
            ui->showProvisioningScreen();
        }
    }
}

void UI::onBackClicked(lv_event_t* event)
{
    ESP_LOGI(TAG, "Back clicked");
    UI* ui = (UI*)lv_event_get_user_data(event);
    if (ui) {
        ui->showMainScreen();
    }
}

void UI::onResetWiFiClicked(lv_event_t* event)
{
    ESP_LOGI(TAG, "Reset WiFi clicked");
    UI* ui = (UI*)lv_event_get_user_data(event);
    if (ui) {
        ui->event_bus->publish(EVENT_RESET_CREDENTIALS);
        ui->showMainScreen();
    }
}

void UI::onCancelProvisioningClicked(lv_event_t* event)
{
    ESP_LOGI(TAG, "Cancel Provisioning clicked");
    UI* ui = (UI*)lv_event_get_user_data(event);
    if (ui) {
        ui->showMainScreen();
    }
}

// Screen navigation
void UI::showMainScreen()
{
    lv_scr_load_anim(main_screen, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}

void UI::showScheduleScreen()
{
    // Lazy-build schedule screen on first access
    if (!schedule_screen) {
        schedule_screen = lv_obj_create(NULL);
        buildScheduleScreen();
    }
    lv_scr_load_anim(schedule_screen, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}

void UI::showFeedingScreen()
{
    // Lazy-build feeding screen on first access
    if (!feeding_screen) {
        feeding_screen = lv_obj_create(NULL);
        buildFeedingScreen();
    }
    lv_scr_load_anim(feeding_screen, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}

void UI::showProvisioningScreen()
{
    // Lazy-build provisioning screen on first access
    if (!provisioning_screen) {
        provisioning_screen = lv_obj_create(NULL);
        buildProvisioningScreen();
    }
    lv_scr_load_anim(provisioning_screen, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}

void UI::showWiFiStatusScreen()
{
    // Lazy-build status screen on first access
    if (!wifi_status_screen) {
        wifi_status_screen = lv_obj_create(NULL);
        buildWiFiStatusScreen();
    } else {
        // Update SSID label
        if (status_ssid_label) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Connected to:\n%s", wifi_ssid);
            lv_label_set_text(status_ssid_label, buf);
        }
    }
    lv_scr_load_anim(wifi_status_screen, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}

// Build schedule screen
void UI::buildScheduleScreen()
{
    // Set background
    lv_obj_set_style_bg_color(schedule_screen, lv_color_hex(0xF5F5F5), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(schedule_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(schedule_screen, 0, LV_PART_MAIN);
    lv_obj_clear_flag(schedule_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    
    // Back button
    lv_obj_t* back_btn = lv_btn_create(schedule_screen);
    lv_obj_set_size(back_btn, 60, 30);
    lv_obj_set_pos(back_btn, 5, 10);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x00798C), LV_PART_MAIN);
    lv_obj_add_event_cb(back_btn, onBackClicked, LV_EVENT_CLICKED, this);
    
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_label);
    
    // Content placeholder
    lv_obj_t* content = lv_label_create(schedule_screen);
    lv_label_set_text(content, "Schedule configuration\ncoming soon...");
    lv_obj_set_style_text_color(content, lv_color_hex(0x30638E), LV_PART_MAIN);
    lv_obj_set_style_text_align(content, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(content, LV_ALIGN_CENTER, 0, 0);
}

// Build provisioning screen
void UI::buildProvisioningScreen()
{
    lv_obj_set_style_bg_color(provisioning_screen, lv_color_hex(0xF5F5F5), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(provisioning_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(provisioning_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Instructions
    lv_obj_t* instruction = lv_label_create(provisioning_screen);
    lv_label_set_text_static(instruction, "Go to PetFeeder APP\nPress \"Connect Device\"");
    lv_obj_set_style_text_color(instruction, lv_color_hex(0x30638E), LV_PART_MAIN);
    lv_obj_set_style_text_align(instruction, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(instruction, LV_ALIGN_CENTER, 0, -30);
    
    // Device name label
    prov_device_name_label = lv_label_create(provisioning_screen);
    lv_label_set_text(prov_device_name_label, "Device Name:\nPROV_PETFEEDER_...");
    lv_obj_set_style_text_color(prov_device_name_label, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_text_align(prov_device_name_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(prov_device_name_label, LV_ALIGN_CENTER, 0, 20);
    
    // Cancel button
    lv_obj_t* cancel_btn = lv_btn_create(provisioning_screen);
    lv_obj_set_size(cancel_btn, 100, 40);
    lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0xD1495B), LV_PART_MAIN);
    lv_obj_add_event_cb(cancel_btn, onCancelProvisioningClicked, LV_EVENT_CLICKED, this);
    
    lv_obj_t* cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text_static(cancel_label, "Cancel");
    lv_obj_center(cancel_label);
}

// Build WiFi status screen
void UI::buildWiFiStatusScreen()
{
    lv_obj_set_style_bg_color(wifi_status_screen, lv_color_hex(0xF5F5F5), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(wifi_status_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(wifi_status_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // SSID label
    status_ssid_label = lv_label_create(wifi_status_screen);
    char buf[64];
    snprintf(buf, sizeof(buf), "Connected to:\n%s", wifi_ssid[0] ? wifi_ssid : "(unknown)");
    lv_label_set_text(status_ssid_label, buf);
    lv_obj_set_style_text_color(status_ssid_label, lv_color_hex(0x30638E), LV_PART_MAIN);
    lv_obj_set_style_text_align(status_ssid_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(status_ssid_label, LV_ALIGN_CENTER, 0, -20);
    
    // Reset button
    lv_obj_t* reset_btn = lv_btn_create(wifi_status_screen);
    lv_obj_set_size(reset_btn, 140, 45);
    lv_obj_align(reset_btn, LV_ALIGN_BOTTOM_MID, 0, -70);
    lv_obj_set_style_bg_color(reset_btn, lv_color_hex(0xD1495B), LV_PART_MAIN);
    lv_obj_add_event_cb(reset_btn, onResetWiFiClicked, LV_EVENT_CLICKED, this);
    
    lv_obj_t* reset_label = lv_label_create(reset_btn);
    lv_label_set_text_static(reset_label, "Reset WiFi");
    lv_obj_center(reset_label);
    
    // Back button
    lv_obj_t* back_btn = lv_btn_create(wifi_status_screen);
    lv_obj_set_size(back_btn, 60, 30);
    lv_obj_set_pos(back_btn, 5, 10);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x00798C), LV_PART_MAIN);
    lv_obj_add_event_cb(back_btn, onBackClicked, LV_EVENT_CLICKED, this);
    
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_label);
}

// Build feeding screen
void UI::buildFeedingScreen()
{
    lv_obj_set_style_bg_color(feeding_screen, lv_color_hex(0xF5F5F5), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(feeding_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(feeding_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // placeholder message
    lv_obj_t* feeding_label = lv_label_create(feeding_screen);
    lv_label_set_text(feeding_label, LV_SYMBOL_DOWNLOAD "\nFeeding...");
    lv_obj_set_style_text_color(feeding_label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_align(feeding_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(feeding_label, LV_ALIGN_CENTER, 0, 0);
    
    // Back button
    lv_obj_t* back_btn = lv_btn_create(feeding_screen);
    lv_obj_set_size(back_btn, 60, 30);
    lv_obj_set_pos(back_btn, 5, 10);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x00798C), LV_PART_MAIN);
    lv_obj_add_event_cb(back_btn, onBackClicked, LV_EVENT_CLICKED, this);
    
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_center(back_label);
}

// WiFi icon update
void UI::updateWiFiIcon()
{
    if (wifi_icon) {
        if (is_wifi_connected) {
            lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x28A745), LV_PART_MAIN);
        } else {
            lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x999999), LV_PART_MAIN);
        }
    }
}

// WiFi event handlers (static callbacks for EventBus)
void UI::onWiFiConnectedEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    UI* ui = (UI*)arg;
    if (ui && data) {
        wifi_event_data_t* wifi_data = (wifi_event_data_t*)data;
        
        ui->is_wifi_connected = true;
        strncpy(ui->wifi_ssid, wifi_data->ssid, sizeof(ui->wifi_ssid) - 1);
        ui->wifi_ssid[sizeof(ui->wifi_ssid) - 1] = '\0';
        ui->updateWiFiIcon();
        
        // If we're on the provisioning screen, switch to status screen
        if (lv_scr_act() == ui->provisioning_screen) {
            ui->showWiFiStatusScreen();
        }
    }
}

void UI::onWiFiDisconnectedEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    UI* ui = (UI*)arg;
    if (ui) {
        ui->is_wifi_connected = false;
        ui->wifi_ssid[0] = '\0';
        ui->updateWiFiIcon();
    }
}

void UI::onProvisioningStartedEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    UI* ui = (UI*)arg;
    if (ui && data) {
        provisioning_event_data_t* prov_data = (provisioning_event_data_t*)data;
        // Update device name label if provisioning screen exists
        if (ui->prov_device_name_label) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Device Name:\n%s", prov_data->device_name);
            lv_label_set_text(ui->prov_device_name_label, buf);
        }
    }
}
