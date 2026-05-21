#include "dht11.h"
#include "esp_log.h"
#include <string.h>

namespace xiaozhi {

static const char* TAG = "DHT11";

DHT11::DHT11(gpio_num_t pin) : pin_(pin) {
    Init();
}

void DHT11::Init() {
    // 配置为开漏输出模式，需要外部上拉电阻
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT_OD;  // 开漏输出模式
    io_conf.pin_bit_mask = (1ULL << pin_);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;   // 启用内部上拉电阻
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    
    // 设置为高电平
    gpio_set_level(pin_, 1);
    
    // 等待DHT11上电稳定
    vTaskDelay(1200 / portTICK_PERIOD_MS);
    
    // 发送一个"假"的起始信号，然后丢弃结果，确保DHT11处于稳定状态
    gpio_set_level(pin_, 0);
    vTaskDelay(25 / portTICK_PERIOD_MS);  // 保持低电平至少18ms
    gpio_set_level(pin_, 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);  // 给DHT11足够的恢复时间
    
    ESP_LOGI(TAG, "DHT11 initialized on GPIO %d with open-drain mode", pin_);
}

esp_err_t DHT11::WaitPinState(uint32_t timeout_us, int expected_pin_state) {
    int64_t start_time = esp_timer_get_time();
    while (esp_timer_get_time() - start_time <= timeout_us) {
        if (gpio_get_level(pin_) == expected_pin_state)
            return ESP_OK;
        esp_rom_delay_us(1);
    }
    return ESP_FAIL;
}

esp_err_t DHT11::DataRead() {
    esp_err_t result = ESP_FAIL;
    memset(buffer_, 0, sizeof(buffer_));
    
    // 完全重置GPIO状态
    // 先释放GPIO，让它回到默认状态
    gpio_reset_pin(pin_);
    vTaskDelay(20 / portTICK_PERIOD_MS);  // 增加到20ms，给更多稳定时间
    
    // 重新配置为开漏输出模式
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT_OD;
    io_conf.pin_bit_mask = (1ULL << pin_);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    
    // 设置为输出模式并保持高电平
    gpio_set_direction(pin_, GPIO_MODE_OUTPUT);
    gpio_set_level(pin_, 1);
    vTaskDelay(200 / portTICK_PERIOD_MS);  // 增加到200ms，给DHT11更多稳定时间
    
    // 1. 主机发送开始信号
    gpio_set_level(pin_, 0);
    vTaskDelay(25 / portTICK_PERIOD_MS);  // 保持低电平至少18ms，增加到25ms更稳定
    gpio_set_level(pin_, 1);
    esp_rom_delay_us(40);  // 增加到40us，给更多稳定时间
    
    // 2. 切换为输入模式，等待DHT11响应
    gpio_set_direction(pin_, GPIO_MODE_INPUT);
    
    // 3. 等待DHT11响应（低电平）
    result = WaitPinState(100, 0);  // 增加超时时间到100us
    if (result == ESP_FAIL) {
/*         ESP_LOGE(TAG, "Phase A Fail, DHT11 not responding with LOW"); */
        return ESP_FAIL;
    }
    
    // 4. 等待DHT11拉高（80us低电平响应结束）
    result = WaitPinState(100, 1);  // 增加超时时间到100us
    if (result == ESP_FAIL) {
/*         ESP_LOGE(TAG, "Phase B Fail, DHT11 not responding with HIGH"); */
        return ESP_FAIL;
    }
    
    // 5. 等待DHT11拉低（80us高电平准备结束）
    result = WaitPinState(100, 0);  // 增加超时时间到100us
    if (result == ESP_FAIL) {
/*         ESP_LOGE(TAG, "Phase C Fail, DHT11 not starting data transmission"); */
        return ESP_FAIL;
    }
    
    // 6. 读取40位数据（5字节）
    for (int i = 0; i < 5; i++) {
        uint8_t byte_value = 0;
        for (int j = 0; j < 8; j++) {
            // 等待数据位开始（高电平）
            result = WaitPinState(100, 1);  // 增加超时时间到100us
            if (result == ESP_FAIL) {
/*                 ESP_LOGE(TAG, "Bit %d.%d start timeout", i, j); */
                // 恢复引脚状态后再返回
                gpio_set_direction(pin_, GPIO_MODE_OUTPUT);
                gpio_set_level(pin_, 1);
                return ESP_FAIL;
            }
            
            // 延时30us，然后检查电平
            // 如果仍为高电平，则为1，否则为0
            esp_rom_delay_us(40);  // 增加到40us，给更多时间稳定
            if (gpio_get_level(pin_) == 1) {
                byte_value = (byte_value << 1) | 1;
            } else {
                byte_value = byte_value << 1;
            }
            
            // 等待本位结束（低电平）
            result = WaitPinState(100, 0);  // 增加超时时间到100us
            if (result == ESP_FAIL) {
/*                 ESP_LOGE(TAG, "Bit %d.%d end timeout", i, j); */
                // 恢复引脚状态后再返回
                gpio_set_direction(pin_, GPIO_MODE_OUTPUT);
                gpio_set_level(pin_, 1);
                return ESP_FAIL;
            }
        }
        buffer_[i] = byte_value;
    }
    
    // 7. 校验数据
    uint8_t checksum = buffer_[0] + buffer_[1] + buffer_[2] + buffer_[3];
    if (checksum != buffer_[4]) {
/*         ESP_LOGE(TAG, "Checksum error: calc=0x%02x, recv=0x%02x", checksum, buffer_[4]);
        ESP_LOGE(TAG, "Raw data: %02x %02x %02x %02x %02x", 
                buffer_[0], buffer_[1], buffer_[2], buffer_[3], buffer_[4]); */
        return ESP_FAIL;
    }
    
    // 8. 更新温湿度数据
    humidity_ = buffer_[0];     // 湿度整数部分
    temperature_ = buffer_[2];  // 温度整数部分
    
    // 9. 记录成功读取的时间
    last_read_time_ = esp_timer_get_time();
    success_count_++;
    
    // 10. 恢复引脚状态
    gpio_set_direction(pin_, GPIO_MODE_OUTPUT);
    gpio_set_level(pin_, 1);
    
    ESP_LOGI(TAG, "DHT11 read success: Temperature=%d°C, Humidity=%d%%", 
             temperature_, humidity_);
    
    return ESP_OK;
}

bool DHT11::ReadData(uint8_t retry_count) {
    ESP_LOGI(TAG, "Starting DHT11 reading with retry_count=%d...", retry_count);
    
    // 检查是否满足最小读取间隔
    int64_t current_time = esp_timer_get_time();
    if (last_read_time_ > 0 && (current_time - last_read_time_) < MIN_READ_INTERVAL_US) {
        int wait_ms = (MIN_READ_INTERVAL_US - (current_time - last_read_time_)) / 1000;
        ESP_LOGW(TAG, "Reading too frequent! Last read was %lld us ago, waiting %d ms...", 
                 (current_time - last_read_time_), wait_ms);
        
        // 等待直到满足最小间隔
        vTaskDelay((wait_ms + 100) / portTICK_PERIOD_MS); // 额外增加100ms作为安全边界
    }
    
    // 在读取前先等待一段时间，确保DHT11处于稳定状态
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    // 尝试读取，如果失败则重试
    bool success = false;
    for (uint8_t i = 0; i <= retry_count; i++) {
        if (i > 0) {
            ESP_LOGI(TAG, "Retry %d/%d after delay...", i, retry_count);
            // 每次重试增加等待时间，给DHT11更多恢复时间
            vTaskDelay((1000 + i * 500) / portTICK_PERIOD_MS); 
        }
        
        esp_err_t result = DataRead();
        if (result == ESP_OK) {
            ESP_LOGI(TAG, "Reading successful on %s attempt: Temperature=%d°C, Humidity=%d%%", 
                    i == 0 ? "first" : "retry", temperature_, humidity_);
            success = true;
            break;
        }
        
        // 每次失败后完全复位引脚状态
        gpio_reset_pin(pin_);
        vTaskDelay(50 / portTICK_PERIOD_MS);  // 增加到50ms
    }
    
    if (!success) {
/*         ESP_LOGE(TAG, "Failed to read DHT11 after %d retries", retry_count); */
        fail_count_++;
    }
    
    return success;
}

uint32_t DHT11::GetDataFreshness() const {
    if (last_read_time_ == 0) {
        return UINT32_MAX; // 表示从未成功读取过数据
    }
    
    int64_t current_time = esp_timer_get_time();
    int64_t elapsed_us = current_time - last_read_time_;
    
    // 转换为毫秒并确保不溢出
    uint32_t elapsed_ms = (elapsed_us / 1000);
    if (elapsed_ms > UINT32_MAX) {
        return UINT32_MAX;
    }
    
    return elapsed_ms;
}

bool DHT11::IsDataFresh(uint32_t max_age_ms) const {
    return GetDataFreshness() <= max_age_ms;
}

}  // namespace xiaozhi
