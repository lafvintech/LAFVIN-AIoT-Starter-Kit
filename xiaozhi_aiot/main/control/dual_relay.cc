#include "dual_relay.h"

static const char* TAG = "DualRelay";

// Initialize static instance pointer
DualRelay* DualRelay::instance_ = nullptr;

void DualRelay::Init(gpio_num_t gpio_1, gpio_num_t gpio_2, bool active_high) {
    // Create singleton instance if not exists
    if (!instance_) {
        instance_ = new DualRelay(gpio_1, gpio_2, active_high);
        ESP_LOGI(TAG, "DualRelay singleton initialized with GPIO %d and %d", 
                 gpio_1, gpio_2);
    }
}

DualRelay::DualRelay(gpio_num_t gpio_1, gpio_num_t gpio_2, bool active_high)
    : gpio_1_(gpio_1), gpio_2_(gpio_2), active_high_(active_high), 
      state_1_(false), state_2_(false) {
    
    // Configure GPIO pins for relay control
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << gpio_1_) | (1ULL << gpio_2_);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    // Initialize both relays to off state
    TurnOffAll();
    
    ESP_LOGI(TAG, "DualRelay initialized on GPIO %d and %d (active %s)", 
             gpio_1_, gpio_2_, active_high_ ? "HIGH" : "LOW");
}

DualRelay::~DualRelay() {
    // Turn off both relays before destruction
    TurnOffAll();
}

void DualRelay::ApplyGpioLevel(gpio_num_t gpio, bool state) {
    // Convert logical state to physical level based on active_high setting
    int level = (state == active_high_) ? 1 : 0;
    gpio_set_level(gpio, level);
}

bool DualRelay::SetChannel(int channel, bool state) {
    if (channel == 1) {
        state_1_ = state;
        ApplyGpioLevel(gpio_1_, state);
        ESP_LOGI(TAG, "Relay channel 1 set to: %s", state ? "ON" : "OFF");
        return true;
    } else if (channel == 2) {
        state_2_ = state;
        ApplyGpioLevel(gpio_2_, state);
        ESP_LOGI(TAG, "Relay channel 2 set to: %s", state ? "ON" : "OFF");
        return true;
    }
    
    ESP_LOGE(TAG, "Invalid relay channel: %d", channel);
    return false;
}

void DualRelay::SetBoth(bool state_1, bool state_2) {
    state_1_ = state_1;
    state_2_ = state_2;
    ApplyGpioLevel(gpio_1_, state_1);
    ApplyGpioLevel(gpio_2_, state_2);
    ESP_LOGI(TAG, "Relay channels set to: CH1=%s, CH2=%s", 
            state_1 ? "ON" : "OFF", state_2 ? "ON" : "OFF");
}

bool DualRelay::GetChannelState(int channel) const {
    if (channel == 1) {
        return state_1_;
    } else if (channel == 2) {
        return state_2_;
    }
    
    ESP_LOGW(TAG, "Invalid channel %d requested, returning false", channel);
    return false;
}

void DualRelay::TurnOnAll() {
    SetBoth(true, true);
}

void DualRelay::TurnOffAll() {
    SetBoth(false, false);
} 