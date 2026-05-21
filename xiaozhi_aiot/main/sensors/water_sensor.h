#ifndef _WATER_SENSOR_H_
#define _WATER_SENSOR_H_

#include <driver/gpio.h>
#include <esp_log.h>

namespace xiaozhi {

/**
 * @brief 水分传感器类型枚举
 */
enum WaterSensorType {
    RAIN_SENSOR,    // 雨滴传感器
    SOIL_SENSOR     // 土壤湿度传感器
};

/**
 * @brief Water Sensor Class
 * 
 * Detects moisture/water using digital sensor module
 * Can be used for both rain detection and soil moisture sensing
 */
class WaterSensor {
public:
    /**
     * @brief Construct a new Water Sensor object
     * 
     * @param gpio_pin GPIO pin connected to sensor's digital output
     * @param type Type of water sensor (rain or soil)
     * @param active_low true if sensor outputs LOW when water is detected (default)
     */
    WaterSensor(gpio_num_t gpio_pin, WaterSensorType type, bool active_low = true);
    
    /**
     * @brief Destroy the Water Sensor object
     */
    ~WaterSensor();
    
    /**
     * @brief Check if water/moisture is detected
     * 
     * @return true if water/moisture is detected, false otherwise
     */
    bool IsWaterDetected();
    
    /**
     * @brief Get the raw GPIO level
     * 
     * @return int GPIO level (0 or 1)
     */
    int GetRawLevel();
    
    /**
     * @brief Get the sensor type
     * 
     * @return WaterSensorType 传感器类型
     */
    WaterSensorType GetSensorType() const { return sensor_type_; }
    
    /**
     * @brief Get the sensor GPIO pin
     * 
     * @return gpio_num_t 传感器GPIO引脚
     */
    gpio_num_t GetGpioPin() const { return gpio_pin_; }

private:
    gpio_num_t gpio_pin_;
    bool active_low_;
    WaterSensorType sensor_type_;
};

// 全局变量，分别用于雨滴传感器和土壤湿度传感器
extern WaterSensor* g_rain_sensor;
extern WaterSensor* g_soil_sensor;

// 便捷函数，初始化雨滴传感器
void InitRainSensor(gpio_num_t gpio_pin, bool active_low = true);

// 便捷函数，初始化土壤湿度传感器
void InitSoilSensor(gpio_num_t gpio_pin, bool active_low = true);

} // namespace xiaozhi

#endif // _WATER_SENSOR_H_ 