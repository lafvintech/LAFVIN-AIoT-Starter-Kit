#include "rgb_led.h"
#include <esp_log.h>
#include <cmath> // 使用C++的数学函数库

static const char* TAG = "RgbLed";

RgbLed::RgbLed(gpio_num_t red_gpio, gpio_num_t green_gpio, gpio_num_t blue_gpio, 
                 bool common_anode, gpio_num_t common_gpio,
                 ledc_channel_t red_channel, ledc_channel_t green_channel, ledc_channel_t blue_channel,
                 ledc_timer_t timer_num)
    : red_gpio_(red_gpio), green_gpio_(green_gpio), blue_gpio_(blue_gpio), 
      common_gpio_(common_gpio), is_common_anode_(common_anode),
      red_channel_(red_channel), green_channel_(green_channel), blue_channel_(blue_channel) {
    
    // 初始化LEDC定时器，用于PWM控制
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT, // 8位分辨率，对应0-255的亮度值
        .timer_num = timer_num,
        .freq_hz = 5000, // 5KHz PWM频率
        .clk_cfg = LEDC_AUTO_CLK
    };
    
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    
    // 配置红色LED通道
    ledc_channel_config_t red_config = {
        .gpio_num = red_gpio_,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = red_channel_,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = timer_num,
        .duty = 0,
        .hpoint = 0
    };
    
    // 配置绿色LED通道
    ledc_channel_config_t green_config = {
        .gpio_num = green_gpio_,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = green_channel_,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = timer_num,
        .duty = 0,
        .hpoint = 0
    };
    
    // 配置蓝色LED通道
    ledc_channel_config_t blue_config = {
        .gpio_num = blue_gpio_,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = blue_channel_,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = timer_num,
        .duty = 0,
        .hpoint = 0
    };
    
    ESP_ERROR_CHECK(ledc_channel_config(&red_config));
    ESP_ERROR_CHECK(ledc_channel_config(&green_config));
    ESP_ERROR_CHECK(ledc_channel_config(&blue_config));
    
    ledc_initialized_ = true;
    
    // 如果提供了共阳/共阴引脚，也将其配置为输出
    if (common_gpio_ != GPIO_NUM_NC) {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << common_gpio_);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
        
        // 对于共阴LED，设置共阴引脚为低电平
        // 对于共阳LED，设置共阳引脚为高电平
        gpio_set_level(common_gpio_, is_common_anode_ ? 1 : 0);
        
        ESP_LOGI(TAG, "RGB LED mode: %s", is_common_anode_ ? "Common Anode" : "Common Cathode");
    }
    
    // 初始状态：关闭所有LED
    TurnOff();
    
    ESP_LOGI(TAG, "RGB LED initialized on GPIO R:%d G:%d B:%d Common:%d", 
             red_gpio_, green_gpio_, blue_gpio_, 
             common_gpio_ == GPIO_NUM_NC ? -1 : common_gpio_);
}

RgbLed::~RgbLed() {
    // 停止呼吸灯任务
    StopBreathing();
    
    // 关闭所有LED
            TurnOff();
}

void RgbLed::OnStateChanged() {
    // 根据设备状态更新LED，可以根据需求实现
    // 例如：根据设备状态设置不同颜色
}

void RgbLed::SetColor(uint8_t r, uint8_t g, uint8_t b) {
    r_ = r;
    g_ = g;
    b_ = b;
    is_on_ = (r > 0 || g > 0 || b > 0);
    ApplyColor();
    
    ESP_LOGI(TAG, "RGB LED color set to (%d, %d, %d)", r, g, b);
}

void RgbLed::SetRed() {
    SetColor(255, 0, 0);
}

void RgbLed::SetGreen() {
    SetColor(0, 255, 0);
}

void RgbLed::SetBlue() {
    SetColor(0, 0, 255);
}

void RgbLed::SetYellow() {
    SetColor(255, 255, 0);
}

// 紫色
void RgbLed::SetPurple() {
    SetColor(128, 0, 128);
}

// 洋红色的别名
void RgbLed::SetMagenta() {
    SetColor(255, 0, 255);
}

// 海军蓝
void RgbLed::SetNavyBlue() {
    SetColor(0, 0, 128);
}

// 粉色
void RgbLed::SetPink() {
    SetColor(255, 153, 255);
}

// 橙色
void RgbLed::SetOrange() {
    SetColor(255, 165, 0);
}

void RgbLed::SetCyan() {
    SetColor(0, 255, 255);
}

void RgbLed::SetWhite() {
    SetColor(255, 255, 255);
}

void RgbLed::TurnOff() {
    SetColor(0, 0, 0);
}

void RgbLed::GetColor(uint8_t* r, uint8_t* g, uint8_t* b) {
    if (r) *r = r_;
    if (g) *g = g_;
    if (b) *b = b_;
}

void RgbLed::ApplyColor() {
    if (!ledc_initialized_) {
        return;
    }
    
    // 对于共阳极LED，PWM值需要反转（255-value）
    // 对于共阴极LED，PWM值直接使用
    uint32_t r_duty = is_common_anode_ ? (255 - r_) : r_;
    uint32_t g_duty = is_common_anode_ ? (255 - g_) : g_;
    uint32_t b_duty = is_common_anode_ ? (255 - b_) : b_;
    
    // 将0-255的亮度值转换为LEDC的占空比值
    r_duty = r_duty * 256 / 255;  // LEDC使用0-256作为8位分辨率的范围
    g_duty = g_duty * 256 / 255;
    b_duty = b_duty * 256 / 255;
    
    ESP_LOGI(TAG, "Setting RGB PWM duties: R=%d, G=%d, B=%d (mode: %s)", 
             r_, g_, b_, is_common_anode_ ? "Common Anode" : "Common Cathode");
    
    // 设置每个通道的PWM占空比
    ledc_set_duty(LEDC_LOW_SPEED_MODE, red_channel_, r_duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, red_channel_);
    
    ledc_set_duty(LEDC_LOW_SPEED_MODE, green_channel_, g_duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, green_channel_);
    
    ledc_set_duty(LEDC_LOW_SPEED_MODE, blue_channel_, b_duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, blue_channel_);
}

// 启动单色呼吸灯效果
void RgbLed::StartBreathing(uint8_t r, uint8_t g, uint8_t b, int breath_period_ms) {
    // 先停止可能正在运行的呼吸灯
    StopBreathing();
    
    // 保存目标颜色和周期
    target_r_ = r;
    target_g_ = g;
    target_b_ = b;
    breath_period_ms_ = breath_period_ms;
    is_rainbow_breathing_ = false;
    
    ESP_LOGI(TAG, "Starting breathing effect with color (%d, %d, %d), period: %d ms", 
             r, g, b, breath_period_ms);
    
    // 创建定时器
    esp_timer_create_args_t timer_args = {
        .callback = &RgbLed::BreathingTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "rgb_breath_timer"
    };
    
    if (esp_timer_create(&timer_args, &breath_timer_) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create breathing timer");
        return;
    }
    
    // 启动定时器，每20毫秒更新一次（50Hz）
    if (esp_timer_start_periodic(breath_timer_, 20 * 1000) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start breathing timer");
        esp_timer_delete(breath_timer_);
        breath_timer_ = nullptr;
        return;
    }
    
    is_breathing_ = true;
    breath_step_ = 0;
}

// 启动彩虹呼吸灯效果
void RgbLed::StartRainbowBreathing(int breath_period_ms) {
    // 先停止可能正在运行的呼吸灯
    StopBreathing();
    
    breath_period_ms_ = breath_period_ms;
    is_rainbow_breathing_ = true;
    
    ESP_LOGI(TAG, "Starting rainbow breathing effect, period: %d ms", breath_period_ms);
    
    // 创建定时器
    esp_timer_create_args_t timer_args = {
        .callback = &RgbLed::BreathingTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "rgb_breath_timer"
    };
    
    if (esp_timer_create(&timer_args, &breath_timer_) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create breathing timer");
        return;
    }
    
    // 启动定时器，每20毫秒更新一次（50Hz）
    if (esp_timer_start_periodic(breath_timer_, 20 * 1000) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start breathing timer");
        esp_timer_delete(breath_timer_);
        breath_timer_ = nullptr;
        return;
    }
    
    is_breathing_ = true;
    breath_step_ = 0;
}

// 停止呼吸灯效果
void RgbLed::StopBreathing() {
    if (!is_breathing_) {
        return;
    }
    
    ESP_LOGI(TAG, "Stopping breathing effect");
    
    if (breath_timer_ != nullptr) {
        esp_timer_stop(breath_timer_);
        esp_timer_delete(breath_timer_);
        breath_timer_ = nullptr;
    }
    
    is_breathing_ = false;
    is_rainbow_breathing_ = false;
    
    // 恢复到之前的颜色
    ApplyColor();
}

// 呼吸灯定时器回调函数
void RgbLed::BreathingTimerCallback(void* arg) {
    RgbLed* led = static_cast<RgbLed*>(arg);
    led->BreathingCallback();
}

// 呼吸灯回调实现
void RgbLed::BreathingCallback() {
    if (!is_breathing_) {
        return;
    }
    
    // 计算呼吸灯效果的亮度
    breath_step_ = (breath_step_ + 1) % breath_period_ms_;
    float brightness = 0.0f;
    
    // 使用正弦波模拟呼吸效果
    float phase = 2.0f * M_PI * static_cast<float>(breath_step_) / static_cast<float>(breath_period_ms_);
    brightness = (std::sin(phase) + 1.0f) / 2.0f;  // 将-1~1转换为0~1
    
    if (is_rainbow_breathing_) {
        // 彩虹模式，使用HSV颜色空间
        float hue = static_cast<float>(breath_step_) / static_cast<float>(breath_period_ms_) * 360.0f;
        
        // HSV to RGB转换
        float h = hue / 60.0f;
        float s = 1.0f;
        float v = brightness;
        
        float c = v * s;
        float x = c * (1.0f - std::abs(std::fmod(h, 2.0f) - 1.0f));
        float m = v - c;
        
        float r = 0, g = 0, b = 0;
        
        if (h >= 0 && h < 1) {
            r = c; g = x; b = 0;
        } else if (h >= 1 && h < 2) {
            r = x; g = c; b = 0;
        } else if (h >= 2 && h < 3) {
            r = 0; g = c; b = x;
        } else if (h >= 3 && h < 4) {
            r = 0; g = x; b = c;
        } else if (h >= 4 && h < 5) {
            r = x; g = 0; b = c;
        } else {
            r = c; g = 0; b = x;
        }
        
        r_ = static_cast<uint8_t>((r + m) * 255.0f);
        g_ = static_cast<uint8_t>((g + m) * 255.0f);
        b_ = static_cast<uint8_t>((b + m) * 255.0f);
    } else {
        // 单色呼吸模式
        r_ = static_cast<uint8_t>(target_r_ * brightness);
        g_ = static_cast<uint8_t>(target_g_ * brightness);
        b_ = static_cast<uint8_t>(target_b_ * brightness);
    }
    
    // 应用颜色
    ApplyColor();
}