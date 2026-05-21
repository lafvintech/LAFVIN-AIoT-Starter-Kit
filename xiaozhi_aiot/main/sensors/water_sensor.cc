#include "water_sensor.h"

static const char* TAG = "WaterSensor";

namespace xiaozhi {

// 全局传感器实例
WaterSensor* g_rain_sensor = nullptr;
WaterSensor* g_soil_sensor = nullptr;

// 初始化雨滴传感器
void InitRainSensor(gpio_num_t gpio_pin, bool active_low) {
    if (g_rain_sensor == nullptr) {
        g_rain_sensor = new WaterSensor(gpio_pin, RAIN_SENSOR, active_low);
        ESP_LOGI(TAG, "Rain sensor initialized on GPIO %d (active %s)", 
                 gpio_pin, active_low ? "LOW" : "HIGH");
    } else {
        ESP_LOGW(TAG, "Rain sensor already initialized");
    }
}

// 初始化土壤湿度传感器
void InitSoilSensor(gpio_num_t gpio_pin, bool active_low) {
    if (g_soil_sensor == nullptr) {
        g_soil_sensor = new WaterSensor(gpio_pin, SOIL_SENSOR, active_low);
        ESP_LOGI(TAG, "Soil moisture sensor initialized on GPIO %d (active %s)", 
                 gpio_pin, active_low ? "LOW" : "HIGH");
    } else {
        ESP_LOGW(TAG, "Soil moisture sensor already initialized");
    }
}

WaterSensor::WaterSensor(gpio_num_t gpio_pin, WaterSensorType type, bool active_low)
    : gpio_pin_(gpio_pin), active_low_(active_low), sensor_type_(type) {
    
    // Configure GPIO pin for water sensor input
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;  // Disable interrupt
    io_conf.mode = GPIO_MODE_INPUT;          // Input mode
    io_conf.pin_bit_mask = (1ULL << gpio_pin_);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;  // Enable pull-up resistor
    gpio_config(&io_conf);
    
    const char* type_str = (sensor_type_ == RAIN_SENSOR) ? "Rain" : "Soil";
    ESP_LOGI(TAG, "%s water sensor initialized on GPIO %d (active %s)", 
             type_str, gpio_pin_, active_low_ ? "LOW" : "HIGH");
}

WaterSensor::~WaterSensor() {
    // No resources to clean up
}

bool WaterSensor::IsWaterDetected() {
    int level = GetRawLevel();
    
    // If active_low_ is true, a LOW signal indicates water
    // If active_low_ is false, a HIGH signal indicates water
    bool is_water_detected = (active_low_) ? (level == 0) : (level == 1);
    
    return is_water_detected;
}

int WaterSensor::GetRawLevel() {
    return gpio_get_level(gpio_pin_);
}

} // namespace xiaozhi 