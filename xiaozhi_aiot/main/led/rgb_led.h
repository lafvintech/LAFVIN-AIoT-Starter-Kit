#ifndef _RGB_LED_H_
#define _RGB_LED_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <cmath>
#include "led.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// RGB LED控制类
class RgbLed : public Led {
public:
    // 构造函数，需要红绿蓝三个GPIO引脚
    // common_anode参数表示是否为共阳极LED，true为共阳极，false为共阴极
    // 使用PWM控制RGB LED的亮度，需要指定三个通道
    RgbLed(gpio_num_t red_gpio, gpio_num_t green_gpio, gpio_num_t blue_gpio, 
           bool common_anode = false, gpio_num_t common_gpio = GPIO_NUM_NC,
           ledc_channel_t red_channel = LEDC_CHANNEL_1,
           ledc_channel_t green_channel = LEDC_CHANNEL_2,
           ledc_channel_t blue_channel = LEDC_CHANNEL_3,
           ledc_timer_t timer_num = LEDC_TIMER_2);
    virtual ~RgbLed();

    // 从Led基类继承的方法
    void OnStateChanged() override;
    
    // RGB LED特有的控制方法
    void SetColor(uint8_t r, uint8_t g, uint8_t b);
    void SetRed();
    void SetGreen();
    void SetBlue();
    void SetYellow();
    void SetPurple();      // 洋红色 (128, 0, 128)
    void SetMagenta();     // 洋红色的别名 (255, 0, 255)
    void SetNavyBlue();    // 海军蓝 (0, 0, 128)
    void SetPink();        // 粉色 (255, 153, 255)
    void SetOrange();      // 橙色 (255, 165, 0)
    void SetCyan();
    void SetWhite();
    void TurnOff();
    
    // 呼吸灯相关方法
    void StartBreathing(uint8_t r, uint8_t g, uint8_t b, int breath_period_ms = 2000);
    void StartRainbowBreathing(int breath_period_ms = 5000);
    void StopBreathing();
    
    // 获取当前颜色
    void GetColor(uint8_t* r, uint8_t* g, uint8_t* b);
    bool IsOn() const { return is_on_; }
    
private:
    gpio_num_t red_gpio_;
    gpio_num_t green_gpio_;
    gpio_num_t blue_gpio_;
    gpio_num_t common_gpio_; // 共阳或共阴GPIO
    
    uint8_t r_ = 0;
    uint8_t g_ = 0;
    uint8_t b_ = 0;
    bool is_on_ = false;
    bool is_common_anode_ = false; // 设置为共阴极
    
    // 实际应用颜色到GPIO
    void ApplyColor();
    
    // PWM控制相关成员
    ledc_channel_t red_channel_;
    ledc_channel_t green_channel_;
    ledc_channel_t blue_channel_;
    bool ledc_initialized_ = false;
    
    // 呼吸灯相关成员
    TaskHandle_t breathing_task_ = nullptr;
    esp_timer_handle_t breath_timer_ = nullptr;
    bool is_breathing_ = false;
    bool is_rainbow_breathing_ = false;
    int breath_step_ = 0;
    int breath_period_ms_ = 2000;
    uint8_t target_r_ = 0;
    uint8_t target_g_ = 0;
    uint8_t target_b_ = 0;
    
    // 呼吸灯回调函数
    void BreathingCallback();
    static void BreathingTimerCallback(void* arg);
};

#endif // _RGB_LED_H_ 