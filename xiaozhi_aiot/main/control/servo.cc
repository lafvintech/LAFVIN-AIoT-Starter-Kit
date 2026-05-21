#include "control/servo.h"
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

// 使用13位分辨率代替16位，ESP32支持的范围是1-14位
#define LEDC_TIMER_RESOLUTION LEDC_TIMER_13_BIT
#define LEDC_BASE_FREQ       50  // 50Hz - 标准舵机PWM频率

// 初始化静态成员变量
Servo* Servo::instance_ = nullptr;

Servo::Servo(gpio_num_t pin, ledc_channel_t channel, ledc_timer_t timer)
    : pin_(pin), channel_(channel), timer_(timer) {
    Initialize();
}

Servo::~Servo() {
    // 停止输出，使用LEDC_LOW_SPEED_MODE
    ledc_stop(LEDC_LOW_SPEED_MODE, channel_, 0);
}

// 静态方法获取单例实例，使用PSRAM分配
Servo* Servo::GetInstance(gpio_num_t pin, ledc_channel_t channel, ledc_timer_t timer) {
    if (!instance_) {
        // 尝试从PSRAM分配内存
        void* mem = heap_caps_malloc(sizeof(Servo), MALLOC_CAP_SPIRAM);
        if (!mem) {
            // 如果PSRAM分配失败，从普通堆分配
            ESP_LOGE(SERVO_TAG, "Failed to allocate servo from PSRAM, using heap");
            mem = heap_caps_malloc(sizeof(Servo), MALLOC_CAP_8BIT);
            if (!mem) {
                ESP_LOGE(SERVO_TAG, "Failed to allocate servo from heap");
                return nullptr;
            }
        } else {
            ESP_LOGI(SERVO_TAG, "Allocated servo in PSRAM");
        }
        
        // 使用placement new在分配的内存上构造对象
        instance_ = new(mem) Servo(pin, channel, timer);
    }
    return instance_;
}

// 静态扫描方法实现
bool Servo::Sweep(Servo* servo, int delay_ms) {
    if (!servo) return false;
    
    // 从0到180度
    for (int i = 0; i <= 180; i += 5) {
        servo->SetAngle(i);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    
    // 从180到0度
    for (int i = 180; i >= 0; i -= 5) {
        servo->SetAngle(i);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    
    // 回到中间位置
    servo->SetAngle(90);
    
    return true;
}

bool Servo::Initialize() {
    if (initialized_) {
        return true;
    }
    
    // 配置LEDC定时器，使用LEDC_LOW_SPEED_MODE
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_RESOLUTION,
        .timer_num = timer_,
        .freq_hz = LEDC_BASE_FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    
    // 配置LEDC通道，使用LEDC_LOW_SPEED_MODE
    ledc_channel_config_t ledc_channel = {
        .gpio_num = pin_,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = channel_,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = timer_,
        .duty = 0,
        .hpoint = 0,
        .flags = {}
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    
    initialized_ = true;
    
    // 设置到中间位置
    SetAngle(90.0f);
    
    return true;
}

void Servo::Calibrate(uint32_t min_pulse_width_us, uint32_t max_pulse_width_us,
                     float min_angle, float max_angle) {
    min_pulse_width_us_ = min_pulse_width_us;
    max_pulse_width_us_ = max_pulse_width_us;
    min_angle_ = min_angle;
    max_angle_ = max_angle;
    
    // 重新设置当前角度以应用新的校准参数
    SetAngle(current_angle_);
}

bool Servo::SetAngle(float angle) {
    // 限制角度范围
    if (angle < min_angle_) angle = min_angle_;
    if (angle > max_angle_) angle = max_angle_;
    
    current_angle_ = angle;
    
    // 转换为脉宽并设置
    uint32_t pulse_width_us = AngleToPulseWidth(angle);
    return SetPulseWidth(pulse_width_us);
}

bool Servo::SetPulseWidth(uint32_t pulse_width_us) {
    if (!initialized_) {
        return false;
    }
    
    // 限制脉宽范围
    if (pulse_width_us < min_pulse_width_us_) pulse_width_us = min_pulse_width_us_;
    if (pulse_width_us > max_pulse_width_us_) pulse_width_us = max_pulse_width_us_;
    
    // 将脉宽(微秒)转换为占空比
    // LEDC_BASE_FREQ = 50Hz，周期为20ms(20000微秒)
    // 对于13位分辨率，最大值为8191
    uint32_t duty = (pulse_width_us * 8191) / 20000;
    
    // 使用LEDC_LOW_SPEED_MODE
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel_, duty);
    if (err != ESP_OK) {
        ESP_LOGE(SERVO_TAG, "Failed to set duty: %d", err);
        return false;
    }
    
    err = ledc_update_duty(LEDC_LOW_SPEED_MODE, channel_);
    if (err != ESP_OK) {
        ESP_LOGE(SERVO_TAG, "Failed to update duty: %d", err);
        return false;
    }
    
    return true;
}

uint32_t Servo::AngleToPulseWidth(float angle) const {
    // 线性映射角度到脉宽
    float angle_range = max_angle_ - min_angle_;
    float pulse_range = max_pulse_width_us_ - min_pulse_width_us_;
    
    return min_pulse_width_us_ + (angle - min_angle_) * pulse_range / angle_range;
}