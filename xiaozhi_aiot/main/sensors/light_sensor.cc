#include "light_sensor.h"

static const char* TAG = "LightSensor";

namespace xiaozhi {

// 全局传感器实例
LightSensor* g_light_sensor = nullptr;

// 初始化模拟光敏传感器
void InitAnalogLightSensor(adc_channel_t adc_channel) {
    if (g_light_sensor == nullptr) {
        g_light_sensor = new LightSensor(adc_channel);
        ESP_LOGI(TAG, "Analog light sensor initialized on ADC channel %d", 
                 adc_channel);
    } else {
        ESP_LOGW(TAG, "Light sensor already initialized");
    }
}

// 模拟模式构造函数
LightSensor::LightSensor(adc_channel_t adc_channel)
    : adc_channel_(adc_channel), adc_handle_(nullptr), is_initialized_(false) {
    
    // 初始化ADC单元
    init_config_.unit_id = ADC_UNIT_2;  // 使用ADC2，因为GPIO11是ADC2_CH0
    init_config_.ulp_mode = ADC_ULP_MODE_DISABLE;
    init_config_.clk_src = ADC_RTC_CLK_SRC_DEFAULT;
    
    esp_err_t ret = adc_oneshot_new_unit(&init_config_, &adc_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(ret));
        return;
    }
    
    // 配置ADC通道
    chan_config_.atten = ADC_ATTEN_DB_11;  // 最大输入电压范围
    chan_config_.bitwidth = ADC_BITWIDTH_12;  // 12位分辨率 (0-4095)
    
    ret = adc_oneshot_config_channel(adc_handle_, adc_channel_, &chan_config_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel %d: %s", 
                 adc_channel_, esp_err_to_name(ret));
        adc_oneshot_del_unit(adc_handle_);
        adc_handle_ = nullptr;
        return;
    }
    
    is_initialized_ = true;
    ESP_LOGI(TAG, "Analog light sensor initialized on ADC channel %d", adc_channel_);
}

LightSensor::~LightSensor() {
    if (adc_handle_ != nullptr) {
        adc_oneshot_del_unit(adc_handle_);
        adc_handle_ = nullptr;
    }
}

bool LightSensor::IsLightDetected() {
    if (!is_initialized_) {
        return false;
    }
    
    // 根据ADC值判断
    return GetAdcValue() > kDarkThreshold;
}

int LightSensor::GetAdcValue() {
    if (!is_initialized_) {
        return -1;
    }
    
    int adc_value = 0;
    esp_err_t ret = adc_oneshot_read(adc_handle_, adc_channel_, &adc_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ADC value: %s", esp_err_to_name(ret));
        return -1;
    }
    
    // 将ADC值取反（基于12位ADC，最大值为4095）
    return 4095 - adc_value;
}

LightLevel LightSensor::GetLightLevel() {
    if (!is_initialized_) {
        return NORMAL; // 默认返回正常
    }
    
    int adc_value = GetAdcValue();
    
    if (adc_value < 0) {
        return NORMAL; // ADC读取错误，返回正常
    } else if (adc_value <= kDarkThreshold) {
        return DARK;
    } else if (adc_value <= kNormalThreshold) {
        return NORMAL;
    } else {
        return BRIGHT;
    }
}

const char* LightSensor::LightLevelToString(LightLevel level) {
    switch (level) {
        case DARK:    return "偏暗";
        case NORMAL:  return "正常";
        case BRIGHT:  return "偏亮";
        default:      return "未知";
    }
}

} // namespace xiaozhi 