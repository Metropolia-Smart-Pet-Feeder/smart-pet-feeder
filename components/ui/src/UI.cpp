#include "UI.h"
#include "Events.h"
#include "Types.h"
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
    event_bus->subscribe(EVENT_FEED_COMPLETED, onFeedCompletedEvent, this);
    event_bus->subscribe(EVENT_FOOD_LEVEL_CHANGED, onFoodLevelChangedEvent, this);

    // Create main screen
    main_screen = lv_obj_create(NULL);
    buildMainScreen();

    // Load main screen
    lv_scr_load(main_screen);

    ESP_LOGI(TAG, "UI built successfully");
}

void UI::buildMainScreen()
{
    // Layout constants
    constexpr lv_coord_t SCREEN_W       = 320;
    constexpr lv_coord_t SCREEN_H       = 240;
    constexpr lv_coord_t MARGIN         = 8;
    constexpr lv_coord_t GAP            = 6;
    constexpr lv_coord_t HEADER_H       = 22;
    constexpr lv_coord_t FOOD_LEVEL_H   = 68;
    constexpr lv_coord_t LAST_FED_H     = 50;
    constexpr lv_coord_t RADIUS         = 8;
    constexpr lv_coord_t CONTENT_W      = SCREEN_W - 2 * MARGIN;

    // Row 3 (feed and connect buttons) fill remaining height
    constexpr lv_coord_t ROW3_Y = MARGIN + HEADER_H + GAP + FOOD_LEVEL_H + GAP + LAST_FED_H + GAP;
    constexpr lv_coord_t BTN_H  = SCREEN_H - ROW3_Y - MARGIN;
    // Feed button is wider (65% width), connect takes the rest
    constexpr lv_coord_t FEED_BTN_W    = (CONTENT_W * 65) / 100;
    constexpr lv_coord_t CONNECT_BTN_W = CONTENT_W - FEED_BTN_W - GAP;

    // Set screen background
    lv_obj_set_style_bg_color(main_screen, lv_color_hex(0xF5F5F5), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(main_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);

    // Row 0: Header bar
    lv_obj_t* header = lv_label_create(main_screen);
    lv_label_set_text_static(header, LV_SYMBOL_HOME " Pet Feeder");
    lv_obj_set_style_text_color(header, lv_color_hex(0x003D5B), LV_PART_MAIN);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, MARGIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_CLICKABLE);

    // WiFi icon
    wifi_icon = lv_label_create(main_screen);
    lv_label_set_text_static(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x999999), LV_PART_MAIN);
    lv_obj_align(wifi_icon, LV_ALIGN_TOP_RIGHT, -MARGIN, MARGIN);
    lv_obj_clear_flag(wifi_icon, LV_OBJ_FLAG_CLICKABLE);

    // Row 1: Food level
    constexpr lv_coord_t row1_y = MARGIN + HEADER_H + GAP;
    lv_obj_t* level_container = lv_obj_create(main_screen);
    lv_obj_set_size(level_container, CONTENT_W, FOOD_LEVEL_H);
    lv_obj_set_pos(level_container, MARGIN, row1_y);
    lv_obj_set_style_bg_color(level_container, lv_color_hex(0x30638E), LV_PART_MAIN);
    lv_obj_set_style_border_width(level_container, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(level_container, RADIUS, LV_PART_MAIN);
    lv_obj_set_style_pad_all(level_container, MARGIN, LV_PART_MAIN);
    lv_obj_clear_flag(level_container, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* level_title = lv_label_create(level_container);
    lv_label_set_text_static(level_title, "Food Level");
    lv_obj_set_style_text_color(level_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_pos(level_title, 0, 0);
    lv_obj_clear_flag(level_title, LV_OBJ_FLAG_CLICKABLE);

    food_level_bar = lv_bar_create(level_container);
    lv_obj_set_size(food_level_bar, CONTENT_W - 4 * MARGIN, 12);
    lv_obj_set_pos(food_level_bar, 0, 20);  // Fixed position
    lv_bar_set_value(food_level_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(food_level_bar, lv_color_hex(0xEDAE49), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(food_level_bar, lv_color_hex(0x003D5B), LV_PART_MAIN);
    lv_obj_clear_flag(food_level_bar, LV_OBJ_FLAG_CLICKABLE);

    food_level_label = lv_label_create(level_container);
    lv_label_set_text(food_level_label, "Unknown");
    lv_obj_set_style_text_color(food_level_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_pos(food_level_label, 0, 38);  // Fixed position
    lv_obj_clear_flag(food_level_label, LV_OBJ_FLAG_CLICKABLE);

    // Row 2: Last fed
    constexpr lv_coord_t row2_y = row1_y + FOOD_LEVEL_H + GAP;
    lv_obj_t* last_fed_container = lv_obj_create(main_screen);
    lv_obj_set_size(last_fed_container, CONTENT_W, LAST_FED_H);
    lv_obj_set_pos(last_fed_container, MARGIN, row2_y);
    lv_obj_set_style_bg_color(last_fed_container, lv_color_hex(0x00798C), LV_PART_MAIN);
    lv_obj_set_style_border_width(last_fed_container, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(last_fed_container, RADIUS, LV_PART_MAIN);
    lv_obj_set_style_pad_all(last_fed_container, MARGIN, LV_PART_MAIN);
    lv_obj_clear_flag(last_fed_container, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* last_fed_title = lv_label_create(last_fed_container);
    lv_label_set_text_static(last_fed_title, "Last Fed");
    lv_obj_set_style_text_color(last_fed_title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_pos(last_fed_title, 0, 0);  // Fixed position
    lv_obj_clear_flag(last_fed_title, LV_OBJ_FLAG_CLICKABLE);

    last_fed_label = lv_label_create(last_fed_container);
    lv_label_set_text(last_fed_label, "No feeding yet");
    lv_obj_set_style_text_color(last_fed_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_pos(last_fed_label, 0, 24);
    lv_obj_clear_flag(last_fed_label, LV_OBJ_FLAG_CLICKABLE);

    // Row 3: Buttons: Feed Now, Connect
    btn_feed_now = lv_btn_create(main_screen);
    lv_obj_set_size(btn_feed_now, FEED_BTN_W, BTN_H);
    lv_obj_set_pos(btn_feed_now, MARGIN, ROW3_Y);
    lv_obj_set_style_bg_color(btn_feed_now, lv_color_hex(0xD1495B), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_feed_now, RADIUS, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_feed_now, onFeedNowClicked, LV_EVENT_CLICKED, this);

    lv_obj_t* feed_label = lv_label_create(btn_feed_now);
    lv_label_set_text_static(feed_label, LV_SYMBOL_DOWNLOAD " FEED NOW");
    lv_obj_set_style_text_align(feed_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(feed_label);
    lv_obj_clear_flag(feed_label, LV_OBJ_FLAG_CLICKABLE);

    // connect to phone
    btn_connect = lv_btn_create(main_screen);
    lv_obj_set_size(btn_connect, CONNECT_BTN_W, BTN_H);
    lv_obj_set_pos(btn_connect, MARGIN + FEED_BTN_W + GAP, ROW3_Y);
    lv_obj_set_style_bg_color(btn_connect, lv_color_hex(0xEDAE49), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_connect, RADIUS, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_connect, onConnectClicked, LV_EVENT_CLICKED, this);

    lv_obj_t* connect_label = lv_label_create(btn_connect);
    lv_label_set_text_static(connect_label, LV_SYMBOL_WIFI "\nCONNECT");
    lv_obj_set_style_text_align(connect_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(connect_label);
    lv_obj_clear_flag(connect_label, LV_OBJ_FLAG_CLICKABLE);

    lv_timer_create(onLastFedTimer, 30000, this);
}

void UI::onFeedNowClicked(lv_event_t* event)
{
    ESP_LOGI(TAG, "Feed Now clicked");

    UI* ui = (UI*)lv_event_get_user_data(event);
    if (ui) {
        feed_request_t request = {};
        request.portions = 1;
        request.source   = FEED_SOURCE_MANUAL;
        ui->event_bus->publish(EVENT_FEED_REQUEST, request);
        ui->showFeedingScreen();
    }
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
        lv_async_call(updateWiFiUI, ui);
    }
}

void UI::onWiFiDisconnectedEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    UI* ui = (UI*)arg;
    if (ui) {
        ui->is_wifi_connected = false;
        ui->wifi_ssid[0] = '\0';
        lv_async_call(updateWiFiUI, ui);
    }
}

void UI::onProvisioningStartedEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    UI* ui = (UI*)arg;
    if (ui && data) {
        provisioning_event_data_t* prov_data = (provisioning_event_data_t*)data;
        strncpy(ui->pending_device_name, prov_data->device_name, sizeof(ui->pending_device_name) - 1);
        ui->pending_device_name[sizeof(ui->pending_device_name) - 1] = '\0';
        lv_async_call(updateProvisioningUI, ui);
    }
}

void UI::onFeedCompletedEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    UI* ui = (UI*)arg;
    if (ui && data) {
        feed_status_t* status  = (feed_status_t*)data;
        ui->last_feed_ms       = lv_tick_get();
        ui->last_feed_portions = status->portions;
        lv_async_call(updateFeedCompletedUI, ui);
    }
}

void UI::updateWiFiUI(void* arg)
{
    UI* ui = (UI*)arg;
    ui->updateWiFiIcon();
    if (ui->is_wifi_connected && lv_scr_act() == ui->provisioning_screen) {
        ui->showWiFiStatusScreen();
    }
}

void UI::updateProvisioningUI(void* arg)
{
    UI* ui = (UI*)arg;
    if (ui->prov_device_name_label) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Device Name:\n%s", ui->pending_device_name);
        lv_label_set_text(ui->prov_device_name_label, buf);
    }
}

void UI::updateFeedCompletedUI(void* arg)
{
    UI* ui = (UI*)arg;
    if (ui->last_fed_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Just now - %d portion%s",
                 ui->last_feed_portions, ui->last_feed_portions == 1 ? "" : "s");
        lv_label_set_text(ui->last_fed_label, buf);
    }
    if (lv_scr_act() == ui->feeding_screen) {
        ui->showMainScreen();
    }
}

void UI::onLastFedTimer(lv_timer_t* timer)
{
    UI* ui = (UI*)timer->user_data;
    if (!ui || !ui->last_fed_label || ui->last_feed_ms == 0) return;

    uint32_t elapsed_ms  = lv_tick_get() - ui->last_feed_ms;
    uint32_t elapsed_min = elapsed_ms / 60000;
    uint32_t elapsed_hr  = elapsed_min / 60;

    char buf[40];
    if (elapsed_min < 1) {
        snprintf(buf, sizeof(buf), "Just now - %d portion%s",
                 ui->last_feed_portions, ui->last_feed_portions == 1 ? "" : "s");
    } else if (elapsed_hr < 1) {
        snprintf(buf, sizeof(buf), "%lu min ago - %d portion%s",
                 elapsed_min, ui->last_feed_portions, ui->last_feed_portions == 1 ? "" : "s");
    } else {
        snprintf(buf, sizeof(buf), "%luh ago - %d portion%s",
                 elapsed_hr, ui->last_feed_portions, ui->last_feed_portions == 1 ? "" : "s");
    }
    lv_label_set_text(ui->last_fed_label, buf);
}

void UI::onFoodLevelChangedEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    UI* ui = (UI*)arg;
    if (ui && data) {
        ui->pending_food_level = *(int*)data;
        lv_async_call(updateFoodLevelUI, ui);
    }
}

void UI::updateFoodLevelUI(void* arg)
{
    UI* ui = (UI*)arg;
    const int bar_values[]     = { 0, 25, 50, 75, 100 };
    const char* level_labels[] = { "Empty", "Low", "Medium", "High", "Full" };
    int idx = (ui->pending_food_level >= 1 && ui->pending_food_level <= 4) ? ui->pending_food_level : 0;
    if (ui->food_level_bar) {
        lv_bar_set_value(ui->food_level_bar, bar_values[idx], LV_ANIM_ON);
    }
    if (ui->food_level_label) {
        lv_label_set_text(ui->food_level_label, level_labels[idx]);
    }
}
