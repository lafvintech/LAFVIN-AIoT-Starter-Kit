#ifndef __DHT11_H__
#define __DHT11_H__

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

namespace xiaozhi {

class DHT11 {
 public:
  // 构造函数，需指定DHT11连接的GPIO引脚
  explicit DHT11(gpio_num_t pin);
  
  // 读取温度湿度，返回是否读取成功
  // 如果设置了retry_count > 0，会在失败时自动重试
  bool ReadData(uint8_t retry_count = 3);
  
  // 获取最近一次读取的湿度值（整数部分）
  uint8_t GetHumidity() const { return humidity_; }
  
  // 获取最近一次读取的温度值（整数部分）
  uint8_t GetTemperature() const { return temperature_; }
  
  // 获取读取成功次数
  uint32_t GetSuccessCount() const { return success_count_; }
  
  // 获取读取失败次数
  uint32_t GetFailCount() const { return fail_count_; }
  
  // 获取数据的新鲜度（自上次成功读取以来的毫秒数）
  uint32_t GetDataFreshness() const;
  
  // 数据是否新鲜（小于指定的毫秒数）
  bool IsDataFresh(uint32_t max_age_ms = 30000) const;
  
 private:
  // 初始化DHT11
  void Init();
  
  // 等待引脚状态变化，带超时检测
  esp_err_t WaitPinState(uint32_t timeout_us, int expected_pin_state);
  
  // 读取数据
  esp_err_t DataRead();
  
  // DHT11连接的GPIO引脚
  gpio_num_t pin_;
  
  // 存储的湿度值（整数部分）
  uint8_t humidity_ = 0;
  
  // 存储的温度值（整数部分）
  uint8_t temperature_ = 0;
  
  // 成功计数
  uint32_t success_count_ = 0;
  
  // 失败计数
  uint32_t fail_count_ = 0;
  
  // 读取的原始数据
  uint8_t buffer_[5] = {0};
  
  // 上次成功读取的时间（微秒）
  int64_t last_read_time_ = 0;
  
  // 最小读取间隔（微秒）- 4秒
  // 增加到4秒，给DHT11更多恢复时间
  static const int64_t MIN_READ_INTERVAL_US = 4000000;
};

}  // namespace xiaozhi

#endif  // __DHT11_H__
