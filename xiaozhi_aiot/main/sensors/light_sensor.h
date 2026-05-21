#ifndef _LIGHT_SENSOR_H_
#define _LIGHT_SENSOR_H_

#include <driver/gpio.h>
#include <driver/adc.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>

namespace xiaozhi {

/**
 * @brief 光照等级枚举 - 简化为三个等级
 */
enum LightLevel {
    DARK,       // 偏暗
    NORMAL,     // 正常
    BRIGHT      // 偏亮
};

/**
 * @brief Light Sensor Class
 * 
 * Detects light intensity using photoresistor module
 * Provides both digital detection and analog value reading
 */
class LightSensor {
public:
    /**
     * @brief Construct a new Light Sensor object with analog input
     * 
     * @param adc_channel ADC channel for reading analog values
     */
    LightSensor(adc_channel_t adc_channel);
    
    /**
     * @brief Destroy the Light Sensor object
     */
    ~LightSensor();
    
    /**
     * @brief Check if light sensor is properly initialized
     * 
     * @return true if initialized
     */
    bool IsInitialized() const { return is_initialized_; }
    
    /**
     * @brief Check if light is detected (brightness above threshold)
     * 
     * @return true if light is detected, false if dark
     */
    bool IsLightDetected();
    
    /**
     * @brief Get the raw ADC value (analog mode)
     * 
     * @return int ADC value (0-4095)
     */
    int GetAdcValue();
    
    /**
     * @brief Get the current light level based on ADC value
     * 
     * @return LightLevel The current light level
     */
    LightLevel GetLightLevel();
    
    /**
     * @brief Get string representation of light level
     * 
     * @param level The light level to convert
     * @return const char* String representation
     */
    static const char* LightLevelToString(LightLevel level);

private:
    // Analog mode variables
    adc_channel_t adc_channel_;
    adc_oneshot_unit_handle_t adc_handle_;
    adc_oneshot_unit_init_cfg_t init_config_;
    adc_oneshot_chan_cfg_t chan_config_;
    
    // Light level thresholds (can be adjusted based on calibration)
    static const int kDarkThreshold = 1000;    // 0-1000: Dark
    static const int kNormalThreshold = 3000;  // 1001-3000: Normal
                                               // 3001-4095: Bright
                                               
    bool is_initialized_;
};

// 全局变量，用于光敏传感器
extern LightSensor* g_light_sensor;

// 便捷函数，初始化模拟光敏传感器
void InitAnalogLightSensor(adc_channel_t adc_channel);

} // namespace xiaozhi

#endif // _LIGHT_SENSOR_H_ 