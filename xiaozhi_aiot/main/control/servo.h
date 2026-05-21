#ifndef _SERVO_H_
#define _SERVO_H_

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <cmath>
#include <vector>
#include <esp_heap_caps.h>

#define SERVO_TAG "SERVO"

class Servo {
public:
    // 构造函数，指定GPIO引脚和LEDC通道
    Servo(gpio_num_t pin, ledc_channel_t channel = LEDC_CHANNEL_4, 
          ledc_timer_t timer = LEDC_TIMER_1);
    virtual ~Servo();
    
    // 设置角度(0-180度)
    bool SetAngle(float angle);
    
    // 获取当前角度
    float GetAngle() const { return current_angle_; }
    
    // 设置脉冲宽度(微秒)，典型值500-2500
    bool SetPulseWidth(uint32_t pulse_width_us);
    
    // 校准函数，可设置最小/最大脉宽对应的角度
    void Calibrate(uint32_t min_pulse_width_us = 500, uint32_t max_pulse_width_us = 2500,
                  float min_angle = 0.0f, float max_angle = 180.0f);

    // 静态方法，获取或创建PSRAM中的单例Servo对象
    static Servo* GetInstance(gpio_num_t pin = GPIO_NUM_16, 
                              ledc_channel_t channel = LEDC_CHANNEL_4,
                              ledc_timer_t timer = LEDC_TIMER_1);

    // 静态方法，执行扫描动作
    static bool Sweep(Servo* servo, int delay_ms = 50);

private:
    gpio_num_t pin_;
    ledc_channel_t channel_;
    ledc_timer_t timer_;
    bool initialized_ = false;
    
    // 当前角度
    float current_angle_ = 90.0f;
    
    // 校准参数
    uint32_t min_pulse_width_us_ = 500;   // 0度对应的脉宽(微秒)
    uint32_t max_pulse_width_us_ = 2500;  // 180度对应的脉宽(微秒)
    float min_angle_ = 0.0f;
    float max_angle_ = 180.0f;
    
    // 初始化LEDC
    bool Initialize();
    
    // 将角度转换为脉宽
    uint32_t AngleToPulseWidth(float angle) const;

    // 静态单例实例指针
    static Servo* instance_;
};

// 删除ServoManager类，改为简化的静态方法方式

#endif // _SERVO_H_