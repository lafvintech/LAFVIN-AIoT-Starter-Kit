/*
 * MCP Server Implementation
 * Reference: https://modelcontextprotocol.io/specification/2024-11-05
 */

#include "mcp_server.h"
#include <esp_log.h>
#include <esp_app_desc.h>
#include <algorithm>
#include <cstring>
#include <esp_pthread.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"
#include "display.h"
#include "board.h"
#include "led/rgb_led.h"
#include "sensors/dht11.h"
#include "sensors/water_sensor.h"
#include "sensors/light_sensor.h"
#include "control/servo.h"
#include "control/dual_relay.h"
#include "led/circular_strip.h"

#define TAG "MCP"

#define DEFAULT_TOOLCALL_STACK_SIZE 6144

// 添加一个静态变量来存储警车模式的定时器句柄（如果不方便修改CircularStrip类）
static esp_timer_handle_t g_police_timer = nullptr;

// 添加一个辅助函数，用于停止所有LED特效定时器
void StopAllLedEffects(CircularStrip* strip) {
    // 停止CircularStrip内部的效果
    strip->StopEffects();  // 使用新添加的方法停止strip_timer_
    
    // 停止警车效果定时器
    if (g_police_timer != nullptr) {
        esp_timer_stop(g_police_timer);
        esp_timer_delete(g_police_timer);
        g_police_timer = nullptr;
        ESP_LOGI(TAG, "Stopped police effect timer");
    }
}

McpServer::McpServer() {
}

McpServer::~McpServer() {
    for (auto tool : tools_) {
        delete tool;
    }
    tools_.clear();
}

void McpServer::AddCommonTools() {
    // To speed up the response time, we add the common tools to the beginning of
    // the tools list to utilize the prompt cache.
    // Backup the original tools list and restore it after adding the common tools.
    auto original_tools = std::move(tools_);
    auto& board = Board::GetInstance();

    AddTool("self.get_device_status",
        "Provides the real-time information of the device, including the current status of the audio speaker, screen, battery, network, etc.\n"
        "Use this tool for: \n"
        "1. Answering questions about current condition (e.g. what is the current volume of the audio speaker?)\n"
        "2. As the first step to control the device (e.g. turn up / down the volume of the audio speaker, etc.)",
        PropertyList(),
        [&board](const PropertyList& properties) -> ReturnValue {
            return board.GetDeviceStatusJson();
        });

    // 添加DHT11温湿度传感器支持
    // 初始化DHT11传感器，连接到GPIO11引脚
    // 使用重构后的DHT11驱动，采用开漏输出模式和精确时序控制
    static xiaozhi::DHT11* dht11_sensor = new xiaozhi::DHT11(GPIO_NUM_11);
    
    // 添加读取温湿度的工具
    AddTool("self.sensor.dht11.read",
        "Read temperature and humidity data from DHT11 sensor",
        PropertyList(),
        [dht11_sensor](const PropertyList& properties) -> ReturnValue {
            bool success = dht11_sensor->ReadData(3); // 使用3次重试
            if (success) {
                std::string result = "Temperature: " + std::to_string(dht11_sensor->GetTemperature()) + 
                                    "°C, Humidity: " + std::to_string(dht11_sensor->GetHumidity()) + "%";
                return result;
            } else {
                // 检查是否有较新的数据可用（30秒内的数据）
                if (dht11_sensor->IsDataFresh(30000)) {
                    std::string result = "Last valid reading (from " + 
                                        std::to_string(dht11_sensor->GetDataFreshness() / 1000) + 
                                        " seconds ago): Temperature: " + 
                                        std::to_string(dht11_sensor->GetTemperature()) + 
                                        "°C, Humidity: " + 
                                        std::to_string(dht11_sensor->GetHumidity()) + "%";
                    return result;
                } else {
                    return "Failed to read DHT11 sensor data";
                }
            }
        });
    
    // // 添加只读取温度的工具
    // AddTool("self.sensor.dht11.get_temperature",
    //     "Get temperature data from DHT11 sensor",
    //     PropertyList(),
    //     [dht11_sensor](const PropertyList& properties) -> ReturnValue {
    //         bool success = dht11_sensor->ReadData();
    //         if (success) {
    //             return dht11_sensor->GetTemperature();
    //         } else {
    //             return -1; // 返回-1表示读取失败
    //         }
    //     });
    
    // // 添加只读取湿度的工具
    // AddTool("self.sensor.dht11.get_humidity",
    //     "Get humidity data from DHT11 sensor",
    //     PropertyList(),
    //     [dht11_sensor](const PropertyList& properties) -> ReturnValue {
    //         bool success = dht11_sensor->ReadData();
    //         if (success) {
    //             return dht11_sensor->GetHumidity();
    //         } else {
    //             return -1; // 返回-1表示读取失败
    //         }
    //     });

    // // 添加测试DHT11传感器是否正常工作的工具
    // AddTool("self.sensor.dht11.test",
    //     "Test if the DHT11 sensor is working properly",
    //     PropertyList(),
    //     [dht11_sensor](const PropertyList& properties) -> ReturnValue {
    //         ESP_LOGI(TAG, "Testing DHT11 sensor...");
    //         bool success = dht11_sensor->ReadData();
    //         if (success) {
    //             int temp = dht11_sensor->GetTemperature();
    //             int humidity = dht11_sensor->GetHumidity();
    //             ESP_LOGI(TAG, "DHT11 test success! Temp: %d°C, Humidity: %d%%", temp, humidity);
    //             return "DHT11 sensor is working properly. Temperature: " + 
    //                     std::to_string(temp) + "°C, Humidity: " + 
    //                     std::to_string(humidity) + "%";
    //         } else {
    //             ESP_LOGE(TAG, "DHT11 test failed!");
    //             return "DHT11 sensor test failed. Please check the connection and GPIO pin configuration.";
    //         }
    //     });
        
    // 添加雨滴传感器和土壤湿度传感器支持
    // 初始化两个独立的传感器：雨滴传感器连接到GPIO15，土壤湿度传感器连接到GPIO17
    // 两者都设置为低电平触发模式
    xiaozhi::InitRainSensor(GPIO_NUM_15, true);
    xiaozhi::InitSoilSensor(GPIO_NUM_17, true);
    
    // // 初始化光敏电阻传感器，使用ADC模式
    // xiaozhi::InitAnalogLightSensor(ADC_CHANNEL_0); // 使用ADC2_CH0，对应GPIO11
    
    // 添加检测下雨状态的工具
    AddTool("self.sensor.rain.is_detected",
        "Check if rain/water is detected by the LOCAL physical rain sensor connected to GPIO15.\n"
        "Use this tool to detect water/rain with the hardware sensor on the device, NOT for checking weather forecast.\n"
        "Returns true if water/moisture is detected on the sensor, false otherwise.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            if (!xiaozhi::g_rain_sensor) return "Rain sensor not initialized";
            
            bool is_water = xiaozhi::g_rain_sensor->IsWaterDetected();
            ESP_LOGI(TAG, "Rain detection: %s", is_water ? "Yes" : "No");
            return is_water;
        });
    
    // 添加测试雨滴传感器工具
    AddTool("self.sensor.rain.test",
        "Test if the local hardware rain sensor connected to GPIO15 is working properly.\n"
        "This tool runs a diagnostic on the PHYSICAL SENSOR attached to the device.\n"
        "NOT related to weather forecast - this checks the actual hardware sensor.\n"
        "Returns detailed status of the sensor including its current readings.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            if (!xiaozhi::g_rain_sensor) return "Rain sensor not initialized";
            
            int level = xiaozhi::g_rain_sensor->GetRawLevel();
            bool is_water = xiaozhi::g_rain_sensor->IsWaterDetected();
            
            ESP_LOGI(TAG, "Rain sensor test: Raw level=%d, Rain detected=%s", 
                    level, is_water ? "Yes" : "No");
            
            return "Hardware rain sensor is working properly. Current state: " + 
                   std::string(is_water ? "Rain/water detected" : "No rain/water detected") + 
                   " (Raw GPIO level: " + std::to_string(level) + ")";
        });
    
    // 添加检测土壤湿度的工具
    AddTool("self.sensor.soil.is_moist",
        "Check if soil is moist using the LOCAL physical soil moisture sensor connected to GPIO17.\n"
        "Use this tool to detect soil moisture with the hardware sensor, NOT for checking weather or irrigation status.\n"
        "Returns true if soil moisture is detected, false if soil is dry.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            if (!xiaozhi::g_soil_sensor) return "Soil moisture sensor not initialized";
            
            bool is_moist = xiaozhi::g_soil_sensor->IsWaterDetected();
            ESP_LOGI(TAG, "Soil moisture detection: %s", is_moist ? "Moist" : "Dry");
            return is_moist;
        });
    
    // 添加测试土壤湿度传感器工具
    AddTool("self.sensor.soil.test",
        "Test if the local hardware soil moisture sensor connected to GPIO17 is working properly.\n"
        "This tool runs a diagnostic on the PHYSICAL SENSOR attached to the device.\n"
        "Returns detailed status of the soil moisture sensor including its current readings.",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            if (!xiaozhi::g_soil_sensor) return "Soil moisture sensor not initialized";
            
            int level = xiaozhi::g_soil_sensor->GetRawLevel();
            bool is_moist = xiaozhi::g_soil_sensor->IsWaterDetected();
            
            ESP_LOGI(TAG, "Soil sensor test: Raw level=%d, Soil moist=%s", 
                    level, is_moist ? "Yes" : "No");
            
            return "Hardware soil moisture sensor is working properly. Current state: " + 
                   std::string(is_moist ? "Soil is moist" : "Soil is dry") + 
                   " (Raw GPIO level: " + std::to_string(level) + ")";
        });

    // // 添加检测光线的工具
    // AddTool("self.sensor.light.is_detected",
    //     "Check if light is detected by the LOCAL physical light sensor (photoresistor) connected to GPIO11.\n"
    //     "Use this tool to detect light/brightness with the hardware sensor on the device.\n"
    //     "Returns true if light is detected, false if dark.",
    //     PropertyList(),
    //     [](const PropertyList& properties) -> ReturnValue {
    //         if (!xiaozhi::g_light_sensor) return "Light sensor not initialized";
            
    //         bool is_light = xiaozhi::g_light_sensor->IsLightDetected();
    //         ESP_LOGI(TAG, "Light detection: %s", is_light ? "Bright" : "Dark");
    //         return is_light;
    //     });
    
    // // 添加测试光敏电阻工具
    // AddTool("self.sensor.light.test",
    //     "Test if the local hardware light sensor is working properly.\n"
    //     "This tool runs a diagnostic on the PHYSICAL SENSOR attached to the device.\n"
    //     "Returns detailed status of the sensor including its current readings.",
    //     PropertyList(),
    //     [](const PropertyList& properties) -> ReturnValue {
    //         if (!xiaozhi::g_light_sensor) return "Light sensor not initialized";
            
    //         std::string result = "光敏电阻传感器工作正常。\n";
            
    //         // 添加模拟模式信息
    //         int adc_value = xiaozhi::g_light_sensor->GetAdcValue();
    //         xiaozhi::LightLevel light_level = xiaozhi::g_light_sensor->GetLightLevel();
            
    //         result += "ADC值 = " + std::to_string(adc_value) + 
    //                   " (光照等级: " + xiaozhi::LightSensor::LightLevelToString(light_level) + ")";
            
    //         return result;
    //     });

    // // 添加获取ADC值的工具
    // AddTool("self.sensor.light.get_adc",
    //     "Get the raw ADC value from the light sensor.\n"
    //     "This tool returns the analog reading from the photoresistor sensor.\n"
    //     "Returns ADC value (0-4095) or -1 if not available.",
    //     PropertyList(),
    //     [](const PropertyList& properties) -> ReturnValue {
    //         if (!xiaozhi::g_light_sensor) return "Light sensor not initialized";
            
    //         int adc_value = xiaozhi::g_light_sensor->GetAdcValue();
    //         return adc_value;
    //     });
    
    // // 添加获取当前光照等级的工具
    // AddTool("self.sensor.light.get_level",
    //     "Get the current light level category.\n"
    //     "This tool returns the categorized light level from the sensor.\n"
    //     "Returns one of: '偏暗', '正常', '偏亮'",
    //     PropertyList(),
    //     [](const PropertyList& properties) -> ReturnValue {
    //         if (!xiaozhi::g_light_sensor) return "Light sensor not initialized";
            
    //         xiaozhi::LightLevel level = xiaozhi::g_light_sensor->GetLightLevel();
    //         const char* level_str = xiaozhi::LightSensor::LightLevelToString(level);
            
    //         ESP_LOGI(TAG, "Current light level: %s", level_str);
    //         return level_str;
    //     });

    // 添加RGB LED控制工具
    // 初始化RGB LED，使用GPIO4(红), GPIO5(绿), GPIO6(蓝)
    // 设置为共阴极模式(common_anode=false)
    // 使用PWM通道1,2,3分别控制RGB三色
    // 使用TIMER_2避免与背光(TIMER_0)和舵机(TIMER_1)冲突
    static RgbLed* rgb_led = new RgbLed(
        GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6, 
        false, GPIO_NUM_NC,
        LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3,
        LEDC_TIMER_2
    );
    
    // 添加设置颜色的工具
    AddTool("self.led.set_color",
        "Set the RGB LED color with RGB values (0-255 for each channel)",
        PropertyList({
            Property("r", kPropertyTypeInteger, 0, 255),
            Property("g", kPropertyTypeInteger, 0, 255),
            Property("b", kPropertyTypeInteger, 0, 255)
        }), 
        [rgb_led](const PropertyList& properties) -> ReturnValue {
            uint8_t r = properties["r"].value<int>();
            uint8_t g = properties["g"].value<int>();
            uint8_t b = properties["b"].value<int>();
            rgb_led->SetColor(r, g, b);
            return true;
        });
    
    // 添加预设颜色的工具（合并多个颜色设置为一个工具）
    AddTool("self.led.set_preset_color",
        "Set the RGB LED to a preset color\n"
        "Args:\n"
        "  `color`: One of ['red', 'green', 'blue', 'yellow', 'purple', 'magenta', 'navy_blue', 'pink', 'orange', 'cyan', 'white']",
        PropertyList({
            Property("color", kPropertyTypeString)
        }),
        [rgb_led](const PropertyList& properties) -> ReturnValue {
            std::string color = properties["color"].value<std::string>();
            
            if (color == "red") {
                rgb_led->SetRed();
            } else if (color == "green") {
                rgb_led->SetGreen();
            } else if (color == "blue") {
                rgb_led->SetBlue();
            } else if (color == "yellow") {
                rgb_led->SetYellow();
            } else if (color == "purple") {
                rgb_led->SetPurple();
            } else if (color == "magenta") {
                rgb_led->SetMagenta();
            } else if (color == "navy_blue") {
                rgb_led->SetNavyBlue();
            } else if (color == "pink") {
                rgb_led->SetPink();
            } else if (color == "orange") {
                rgb_led->SetOrange();
            } else if (color == "cyan") {
                rgb_led->SetCyan();
            } else if (color == "white") {
                rgb_led->SetWhite();
            } else {
                return false;
            }
            
            return true;
        });
    
    // 添加关闭LED的工具
    AddTool("self.led.turn_off",
        "Turn off the RGB LED",
        PropertyList(), 
        [rgb_led](const PropertyList& properties) -> ReturnValue {
            rgb_led->TurnOff();
            return true;
        });
    
    // 添加呼吸灯效果工具
    AddTool("self.led.start_breathing",
        "Start a breathing effect with specified color and period",
        PropertyList({
            Property("r", kPropertyTypeInteger, 0, 255),
            Property("g", kPropertyTypeInteger, 0, 255),
            Property("b", kPropertyTypeInteger, 0, 255),
            Property("period_ms", kPropertyTypeInteger, 500, 10000)
        }), 
        [rgb_led](const PropertyList& properties) -> ReturnValue {
            uint8_t r = properties["r"].value<int>();
            uint8_t g = properties["g"].value<int>();
            uint8_t b = properties["b"].value<int>();
            int period_ms = properties["period_ms"].value<int>();
            rgb_led->StartBreathing(r, g, b, period_ms);
            return true;
        });
    
    AddTool("self.led.start_rainbow",
        "Start a rainbow breathing effect with specified period",
        PropertyList({
            Property("period_ms", kPropertyTypeInteger, 1000, 20000)
        }), 
        [rgb_led](const PropertyList& properties) -> ReturnValue {
            int period_ms = properties["period_ms"].value<int>();
            rgb_led->StartRainbowBreathing(period_ms);
            return true;
        });
    
    AddTool("self.led.stop_breathing",
        "Stop any breathing effect",
        PropertyList(), 
        [rgb_led](const PropertyList& properties) -> ReturnValue {
            rgb_led->StopBreathing();
            return true;
        });
    
    // 添加获取LED状态的工具
    AddTool("self.led.get_status",
        "Get the current status and color of the RGB LED",
        PropertyList(), 
        [rgb_led](const PropertyList& properties) -> ReturnValue {
            uint8_t r, g, b;
            rgb_led->GetColor(&r, &g, &b);
            bool is_on = rgb_led->IsOn();
            
            char status[100];
            snprintf(status, sizeof(status), 
                     "{\"power\": %s, \"color\": {\"r\": %d, \"g\": %d, \"b\": %d}}", 
                     is_on ? "true" : "false", r, g, b);
            
            ESP_LOGI(TAG, "RGB LED status: %s, Color: (%d, %d, %d)", 
                     is_on ? "ON" : "OFF", r, g, b);
            
            return std::string(status);
        });

    AddTool("self.audio_speaker.set_volume", 
        "Set the volume of the audio speaker. If the current volume is unknown, you must call `self.get_device_status` tool first and then call this tool.",
        PropertyList({
            Property("volume", kPropertyTypeInteger, 0, 100)
        }), 
        [&board](const PropertyList& properties) -> ReturnValue {
            auto codec = board.GetAudioCodec();
            codec->SetOutputVolume(properties["volume"].value<int>());
            return true;
        });
    
    auto backlight = board.GetBacklight();
    if (backlight) {
        AddTool("self.screen.set_brightness",
            "Set the brightness of the screen.",
            PropertyList({
                Property("brightness", kPropertyTypeInteger, 0, 100)
            }),
            [backlight](const PropertyList& properties) -> ReturnValue {
                uint8_t brightness = static_cast<uint8_t>(properties["brightness"].value<int>());
                backlight->SetBrightness(brightness, true);
                return true;
            });
    }

    auto display = board.GetDisplay();
    if (display && !display->GetTheme().empty()) {
        AddTool("self.screen.set_theme",
            "Set the theme of the screen. The theme can be `light` or `dark`.",
            PropertyList({
                Property("theme", kPropertyTypeString)
            }),
            [display](const PropertyList& properties) -> ReturnValue {
                display->SetTheme(properties["theme"].value<std::string>().c_str());
                return true;
            });
    }

    auto camera = board.GetCamera();
    if (camera) {
        AddTool("self.camera.take_photo",
            "Take a photo and explain it. Use this tool after the user asks you to see something.\n"
            "Args:\n"
            "  `question`: The question that you want to ask about the photo.\n"
            "Return:\n"
            "  A JSON object that provides the photo information.",
            PropertyList({
                Property("question", kPropertyTypeString)
            }),
            [camera](const PropertyList& properties) -> ReturnValue {
                if (!camera->Capture()) {
                    return "{\"success\": false, \"message\": \"Failed to capture photo\"}";
                }
                auto question = properties["question"].value<std::string>();
                return camera->Explain(question);
            });
    }

    // ------------------------------------------------------------
    // Servo舵机控制工具
    // ------------------------------------------------------------
    // 创建统一的属性列表
    static PropertyList angle_property({
        Property("angle", kPropertyTypeInteger, 90, 0, 180)
    });

    // 添加设置舵机角度的工具
    AddTool("self.servo.set_angle",
        "Set servo angle",
        angle_property,
        [](const PropertyList& properties) -> ReturnValue {
            Servo* servo = Servo::GetInstance();
            if (!servo) return false;
            
            int angle = properties["angle"].value<int>();
            return servo->SetAngle(angle);
        });

    // 添加校准舵机的工具
    static PropertyList calibrate_properties({
        Property("min_pulse", kPropertyTypeInteger, 500, 500, 1500),
        Property("max_pulse", kPropertyTypeInteger, 2500, 1500, 2500),
        Property("min_angle", kPropertyTypeInteger, 0, 0, 90),
        Property("max_angle", kPropertyTypeInteger, 180, 90, 270)
    });
    
    AddTool("self.servo.calibrate",
        "Calibrate servo",
        calibrate_properties,
        [](const PropertyList& properties) -> ReturnValue {
            Servo* servo = Servo::GetInstance();
            if (!servo) return false;
            
            int min_pulse = properties["min_pulse"].value<int>();
            int max_pulse = properties["max_pulse"].value<int>();
            int min_angle = properties["min_angle"].value<int>();
            int max_angle = properties["max_angle"].value<int>();
            
            servo->Calibrate(min_pulse, max_pulse, min_angle, max_angle);
            return true;
        });

    // 添加获取舵机当前角度的工具
    AddTool("self.servo.get_angle",
        "Get servo angle",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            Servo* servo = Servo::GetInstance();
            if (!servo) return -1;
            
            return (int)servo->GetAngle();
        });

    // 添加舵机扫描工具
    static PropertyList sweep_properties({
        Property("delay_ms", kPropertyTypeInteger, 50, 10, 1000)
    });
    
    AddTool("self.servo.sweep",
        "Sweep servo",
        sweep_properties,
        [](const PropertyList& properties) -> ReturnValue {
            Servo* servo = Servo::GetInstance();
            if (!servo) return false;
            
            int delay_ms = properties["delay_ms"].value<int>();
            
            // 创建低优先级任务，使用较小的堆栈大小
            // 不捕获servo，而是在任务内部获取
            xTaskCreate([](void* arg) {
                int delay = *((int*)arg);
                delete (int*)arg; // 释放参数内存
                
                Servo* s = Servo::GetInstance();
                if (s) {
                    Servo::Sweep(s, delay);
                }
                vTaskDelete(NULL);
            }, "servo_sweep", 2048, new int(delay_ms), tskIDLE_PRIORITY, NULL);
            
            return true;
        });

    // ------------------------------------------------------------
    // WS2812 LED灯带控制工具
    // ------------------------------------------------------------
    // 初始化CircularStrip，连接到GPIO_NUM_7，LED数量为8
    static CircularStrip* strip_led = new CircularStrip(GPIO_NUM_7, 8);

    // 修改set_all_color工具实现
    AddTool("self.strip.set_all_color",
        "Set all LEDs on the WS2812 LED strip to the same color. Use this when you want the entire strip to display a uniform color.",
        PropertyList({
            Property("r", kPropertyTypeInteger, 0, 255),
            Property("g", kPropertyTypeInteger, 0, 255),
            Property("b", kPropertyTypeInteger, 0, 255)
        }),
        [strip_led](const PropertyList& properties) -> ReturnValue {
            // 首先停止所有LED效果
            StopAllLedEffects(strip_led);
            
            // 然后设置新颜色
            uint8_t r = properties["r"].value<int>();
            uint8_t g = properties["g"].value<int>();
            uint8_t b = properties["b"].value<int>();
            StripColor color = {r, g, b};
            strip_led->SetAllColor(color);
            ESP_LOGI(TAG, "Set all LEDs to color (%d, %d, %d)", r, g, b);
            return true;
        });

        // 设置灯带单个LED灯颜色
    AddTool("self.strip.set_single_color",
        "Set color for a single LED on the WS2812 strip. Index ranges from 0 to 7.",
        PropertyList({
            Property("index", kPropertyTypeInteger, 0, 0, 7),
            Property("r", kPropertyTypeInteger, 0, 255),
            Property("g", kPropertyTypeInteger, 0, 255),
            Property("b", kPropertyTypeInteger, 0, 255)
        }),
        [strip_led](const PropertyList& properties) -> ReturnValue {
            // 首先停止所有LED效果
            StopAllLedEffects(strip_led);
            
            uint8_t index = properties["index"].value<int>();
            uint8_t r = properties["r"].value<int>();
            uint8_t g = properties["g"].value<int>();
            uint8_t b = properties["b"].value<int>();
            StripColor color = {r, g, b};
            strip_led->SetSingleColor(index, color);
            return true;
        });

        // 添加彩虹追逐效果工具
    AddTool("self.strip.rainbow_chase",
        "Start rainbow chase effect on WS2812 LED strip - creates a moving rainbow pattern.\n"
        "Use this tool specifically when user asks for 'rainbow chase', 'rainbow effect', or 'colorful moving lights'.\n"
        "DO NOT use this for police lights, emergency lights, or red-blue flashing effects.",
        PropertyList({
            Property("interval_ms", kPropertyTypeInteger, 100, 10, 1000)
        }),
        [strip_led](const PropertyList& properties) -> ReturnValue {
            // 首先停止所有LED效果
            StopAllLedEffects(strip_led);
            
            int interval = properties["interval_ms"].value<int>();
            strip_led->RainbowChase(interval);
            ESP_LOGI(TAG, "Started rainbow chase effect with interval %d ms", interval);
            return true;
        });

    // 修改set_preset_mode中的police模式实现
    AddTool("self.strip.set_preset_mode",
        "Set preset effect mode for WS2812 LED strip.\n"
        "Available modes:\n"
        "- 'police': Red and blue alternating flash (for police/emergency effect)\n"
        "- 'rainbow_chase': Moving rainbow colors (same as rainbow_chase tool)\n"
        "- 'off': Turn off all LEDs\n"
        "Use 'police' mode specifically for police lights, emergency lights, or red-blue flashing patterns.",
        PropertyList({
            Property("mode", kPropertyTypeString)
        }),
        [strip_led](const PropertyList& properties) -> ReturnValue {
            std::string mode = properties["mode"].value<std::string>();
           if (mode == "rainbow_chase") {
                // 彩虹追逐模式：彩虹色循环移动
                // 首先停止所有LED效果
                StopAllLedEffects(strip_led);
                
                // 启动彩虹追逐效果，每100ms更新一次
                strip_led->RainbowChase(100);
                ESP_LOGI(TAG, "Started rainbow chase effect");
                return true;
            }
            else if (mode == "police") {
                // 警车效果：红蓝交替闪烁
                // 先停止可能存在的老定时器
                if (g_police_timer != nullptr) {
                    esp_timer_stop(g_police_timer);
                    esp_timer_delete(g_police_timer);
                    g_police_timer = nullptr;
                }
                
                // 我们使用两个不同的颜色状态
                static bool is_first_pattern = true;
                
                // 设置初始状态
                if (is_first_pattern) {
                    // 前半部分红色，后半部分蓝色
                    for (int i = 0; i < 4; i++) {
                        strip_led->SetSingleColor(i, {255, 0, 0}); // 红色
                    }
                    for (int i = 4; i < 8; i++) {
                        strip_led->SetSingleColor(i, {0, 0, 255}); // 蓝色
                    }
                }
                
                // 创建一个简单的定时器来交替颜色
                esp_timer_create_args_t timer_args = {
                    .callback = [](void* arg) {
                        static bool is_first_pattern = true;
                        auto strip = static_cast<CircularStrip*>(arg);
                        
                        if (is_first_pattern) {
                            // 前半部分红色，后半部分蓝色
                            for (int i = 0; i < 4; i++) {
                                strip->SetSingleColor(i, {255, 0, 0}); // 红色
                            }
                            for (int i = 4; i < 8; i++) {
                                strip->SetSingleColor(i, {0, 0, 255}); // 蓝色
                            }
                        } else {
                            // 前半部分蓝色，后半部分红色
                            for (int i = 0; i < 4; i++) {
                                strip->SetSingleColor(i, {0, 0, 255}); // 蓝色
                            }
                            for (int i = 4; i < 8; i++) {
                                strip->SetSingleColor(i, {255, 0, 0}); // 红色
                            }
                        }
                        
                        is_first_pattern = !is_first_pattern;
                    },
                    .arg = strip_led,
                    .dispatch_method = ESP_TIMER_TASK,
                    .name = "police_timer",
                    .skip_unhandled_events = false,
                };
                
                ESP_ERROR_CHECK(esp_timer_create(&timer_args, &g_police_timer));
                ESP_ERROR_CHECK(esp_timer_start_periodic(g_police_timer, 500 * 1000)); // 500ms
                ESP_LOGI(TAG, "Started police effect");
                
                return true;
            }
            else if (mode == "off") {
                // 关闭所有灯 - 需要停止所有效果
                StopAllLedEffects(strip_led);
                
                // 设置所有LED为黑色
                StripColor off = {0, 0, 0};
                strip_led->SetAllColor(off);
                ESP_LOGI(TAG, "All LED effects stopped and LEDs turned off");
                return true;
            }
            else {
                return false;
            }
        });

    // // 添加专门的关闭所有LED效果工具
    // AddTool("self.strip.stop_all_effects",
    //     "Stop all LED strip effects and turn off all LEDs.\n"
    //     "Use this tool when user wants to 'turn off LED strip', 'stop LED effects', 'close lights', or '关闭LED灯带'.\n"
    //     "This will stop any running effects (police, rainbow chase, etc.) and turn off all LEDs.",
    //     PropertyList(),
    //     [strip_led](const PropertyList& properties) -> ReturnValue {
    //         // 停止所有LED效果
    //         StopAllLedEffects(strip_led);
            
    //         // 设置所有LED为黑色
    //         StripColor off = {0, 0, 0};
    //         strip_led->SetAllColor(off);
    //         ESP_LOGI(TAG, "All LED effects stopped and strip turned off");
    //         return true;
    //     });

    // 呼吸灯效果
    AddTool("self.strip.breathe",
        "Create a breathing effect on the WS2812 strip between two colors",
        PropertyList({
            Property("low_r", kPropertyTypeInteger, 0, 0, 255),
            Property("low_g", kPropertyTypeInteger, 0, 0, 255),
            Property("low_b", kPropertyTypeInteger, 0, 0, 255),
            Property("high_r", kPropertyTypeInteger, 255, 0, 255),
            Property("high_g", kPropertyTypeInteger, 255, 0, 255),
            Property("high_b", kPropertyTypeInteger, 255, 0, 255),
            Property("interval_ms", kPropertyTypeInteger, 50, 10, 1000)
        }),
        [strip_led](const PropertyList& properties) -> ReturnValue {
            StripColor low = {
                (uint8_t)properties["low_r"].value<int>(),
                (uint8_t)properties["low_g"].value<int>(),
                (uint8_t)properties["low_b"].value<int>()
            };
            StripColor high = {
                (uint8_t)properties["high_r"].value<int>(),
                (uint8_t)properties["high_g"].value<int>(),
                (uint8_t)properties["high_b"].value<int>()
            };
            int interval = properties["interval_ms"].value<int>();
            strip_led->Breathe(low, high, interval);
            return true;
        });

        // 修改fade_out工具
    AddTool("self.strip.fade_out",
        "Gradually fade out the current colors on the WS2812 strip.",
        PropertyList({
            Property("interval_ms", kPropertyTypeInteger, 50, 10, 1000)
        }),
        [strip_led](const PropertyList& properties) -> ReturnValue {
            // 首先停止所有LED效果
            StopAllLedEffects(strip_led);
            
            int interval = properties["interval_ms"].value<int>();
            strip_led->FadeOut(interval);
            ESP_LOGI(TAG, "Starting fade out effect with interval %d ms", interval);
            return true;
        });

        // 修改turn_off工具，确保它能真正关闭所有LED
    AddTool("self.strip.turn_off",
        "Turn off all LEDs on the WS2812 strip and stop all effects.\n"
        "Use this tool when user wants to turn off, close, or stop the LED strip.\n"
        "This will stop any running effects (police, rainbow chase, etc.) and turn off all LEDs.",
        PropertyList(), 
        [strip_led](const PropertyList& properties) -> ReturnValue {
            // 首先停止所有LED效果
            StopAllLedEffects(strip_led);
            
            // 设置所有LED为黑色
            StripColor off = {0, 0, 0};
            strip_led->SetAllColor(off);
            ESP_LOGI(TAG, "Turned off all LEDs");
            return true;
        });

    // 添加GPIO风扇控制工具
    // 使用GPIO_NUM_10作为风扇控制引脚，可以根据实际情况修改
    const gpio_num_t fan_gpio = GPIO_NUM_10;
    
    // 初始化GPIO引脚
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << fan_gpio);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    
    // 默认关闭风扇
    gpio_set_level(fan_gpio, 0);
    ESP_LOGI(TAG, "Fan control GPIO initialized on pin %d", fan_gpio);
    
    // 添加设置风扇状态的工具
    AddTool("self.fan.set_state",
        "Turn the fan on or off by controlling a GPIO pin",
        PropertyList({
            Property("state", kPropertyTypeBoolean, false)
        }),
        [fan_gpio](const PropertyList& properties) -> ReturnValue {
            bool state = properties["state"].value<bool>();
            gpio_set_level(fan_gpio, state ? 1 : 0);
            ESP_LOGI(TAG, "Fan state set to: %s", state ? "ON" : "OFF");
            return true;
        });
    
    // 添加获取风扇状态的工具
    AddTool("self.fan.get_state",
        "Get the current state of the fan (on/off)",
        PropertyList(),
        [fan_gpio](const PropertyList& properties) -> ReturnValue {
            int level = gpio_get_level(fan_gpio);
            ESP_LOGI(TAG, "Current fan state: %s", level ? "ON" : "OFF");
            return (bool)(level == 1);
        });
    
    // ------------------------------------------------------------
    // 双路继电器控制工具
    // ------------------------------------------------------------
    // 初始化继电器，使用GPIO8和GPIO9控制两路继电器
    // 注意：GPIO引脚可以根据实际硬件连接进行调整
    DualRelay::Init(GPIO_NUM_8, GPIO_NUM_9, false);  // true表示高电平触发
    
    // 添加设置单路继电器状态的工具
    AddTool("self.relay.set_channel",
        "Set the state of a specific relay channel",
        PropertyList({
            Property("channel", kPropertyTypeInteger, 1, 1, 2),
            Property("state", kPropertyTypeBoolean, false)
        }),
        [](const PropertyList& properties) -> ReturnValue {
            DualRelay* relay = DualRelay::GetInstance();
            if (!relay) return false;
            
            int channel = properties["channel"].value<int>();
            bool state = properties["state"].value<bool>();
            return relay->SetChannel(channel, state);
        });
    
    // 添加设置两路继电器状态的工具
    AddTool("self.relay.set_both",
        "Set the state of both relay channels",
        PropertyList({
            Property("state_1", kPropertyTypeBoolean, false),
            Property("state_2", kPropertyTypeBoolean, false)
        }),
        [](const PropertyList& properties) -> ReturnValue {
            DualRelay* relay = DualRelay::GetInstance();
            if (!relay) return false;
            
            bool state_1 = properties["state_1"].value<bool>();
            bool state_2 = properties["state_2"].value<bool>();
            relay->SetBoth(state_1, state_2);
            return true;
        });
    
    // 添加获取继电器状态的工具
    AddTool("self.relay.get_status",
        "Get the current status of both relay channels",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            DualRelay* relay = DualRelay::GetInstance();
            if (!relay) return "{}";
            
            bool state_1 = relay->GetChannelState(1);
            bool state_2 = relay->GetChannelState(2);
            
            char status[100];
            snprintf(status, sizeof(status), 
                    "{\"channel_1\": %s, \"channel_2\": %s}", 
                    state_1 ? "true" : "false", state_2 ? "true" : "false");
            
            return std::string(status);
        });
    
    // 注意：移除了set_mode工具，使用set_both工具替代
    // 全开：self.relay.set_both(state_1=true, state_2=true)
    // 全关：self.relay.set_both(state_1=false, state_2=false)

    // Restore the original tools list to the end of the tools list
    tools_.insert(tools_.end(), original_tools.begin(), original_tools.end());
}

void McpServer::AddTool(McpTool* tool) {
    // Prevent adding duplicate tools
    if (std::find_if(tools_.begin(), tools_.end(), [tool](const McpTool* t) { return t->name() == tool->name(); }) != tools_.end()) {
        ESP_LOGW(TAG, "Tool %s already added", tool->name().c_str());
        return;
    }

    ESP_LOGI(TAG, "Add tool: %s", tool->name().c_str());
    tools_.push_back(tool);
}

void McpServer::AddTool(const std::string& name, const std::string& description, const PropertyList& properties, std::function<ReturnValue(const PropertyList&)> callback) {
    AddTool(new McpTool(name, description, properties, callback));
}

void McpServer::ParseMessage(const std::string& message) {
    cJSON* json = cJSON_Parse(message.c_str());
    if (json == nullptr) {
        ESP_LOGE(TAG, "Failed to parse MCP message: %s", message.c_str());
        return;
    }
    ParseMessage(json);
    cJSON_Delete(json);
}

void McpServer::ParseCapabilities(const cJSON* capabilities) {
    auto vision = cJSON_GetObjectItem(capabilities, "vision");
    if (cJSON_IsObject(vision)) {
        auto url = cJSON_GetObjectItem(vision, "url");
        auto token = cJSON_GetObjectItem(vision, "token");
        if (cJSON_IsString(url)) {
            auto camera = Board::GetInstance().GetCamera();
            if (camera) {
                std::string url_str = std::string(url->valuestring);
                std::string token_str;
                if (cJSON_IsString(token)) {
                    token_str = std::string(token->valuestring);
                }
                camera->SetExplainUrl(url_str, token_str);
            }
        }
    }
}

void McpServer::ParseMessage(const cJSON* json) {
    // Check JSONRPC version
    auto version = cJSON_GetObjectItem(json, "jsonrpc");
    if (version == nullptr || !cJSON_IsString(version) || strcmp(version->valuestring, "2.0") != 0) {
        ESP_LOGE(TAG, "Invalid JSONRPC version: %s", version ? version->valuestring : "null");
        return;
    }
    
    // Check method
    auto method = cJSON_GetObjectItem(json, "method");
    if (method == nullptr || !cJSON_IsString(method)) {
        ESP_LOGE(TAG, "Missing method");
        return;
    }
    
    auto method_str = std::string(method->valuestring);
    if (method_str.find("notifications") == 0) {
        return;
    }
    
    // Check params
    auto params = cJSON_GetObjectItem(json, "params");
    if (params != nullptr && !cJSON_IsObject(params)) {
        ESP_LOGE(TAG, "Invalid params for method: %s", method_str.c_str());
        return;
    }

    auto id = cJSON_GetObjectItem(json, "id");
    if (id == nullptr || !cJSON_IsNumber(id)) {
        ESP_LOGE(TAG, "Invalid id for method: %s", method_str.c_str());
        return;
    }
    auto id_int = id->valueint;
    
    if (method_str == "initialize") {
        if (cJSON_IsObject(params)) {
            auto capabilities = cJSON_GetObjectItem(params, "capabilities");
            if (cJSON_IsObject(capabilities)) {
                ParseCapabilities(capabilities);
            }
        }
        auto app_desc = esp_app_get_description();
        std::string message = "{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"" BOARD_NAME "\",\"version\":\"";
        message += app_desc->version;
        message += "\"}}";
        ReplyResult(id_int, message);
    } else if (method_str == "tools/list") {
        std::string cursor_str = "";
        if (params != nullptr) {
            auto cursor = cJSON_GetObjectItem(params, "cursor");
            if (cJSON_IsString(cursor)) {
                cursor_str = std::string(cursor->valuestring);
            }
        }
        GetToolsList(id_int, cursor_str);
    } else if (method_str == "tools/call") {
        if (!cJSON_IsObject(params)) {
            ESP_LOGE(TAG, "tools/call: Missing params");
            ReplyError(id_int, "Missing params");
            return;
        }
        auto tool_name = cJSON_GetObjectItem(params, "name");
        if (!cJSON_IsString(tool_name)) {
            ESP_LOGE(TAG, "tools/call: Missing name");
            ReplyError(id_int, "Missing name");
            return;
        }
        auto tool_arguments = cJSON_GetObjectItem(params, "arguments");
        if (tool_arguments != nullptr && !cJSON_IsObject(tool_arguments)) {
            ESP_LOGE(TAG, "tools/call: Invalid arguments");
            ReplyError(id_int, "Invalid arguments");
            return;
        }
        auto stack_size = cJSON_GetObjectItem(params, "stackSize");
        if (stack_size != nullptr && !cJSON_IsNumber(stack_size)) {
            ESP_LOGE(TAG, "tools/call: Invalid stackSize");
            ReplyError(id_int, "Invalid stackSize");
            return;
        }
        DoToolCall(id_int, std::string(tool_name->valuestring), tool_arguments, stack_size ? stack_size->valueint : DEFAULT_TOOLCALL_STACK_SIZE);
    } else {
        ESP_LOGE(TAG, "Method not implemented: %s", method_str.c_str());
        ReplyError(id_int, "Method not implemented: " + method_str);
    }
}

void McpServer::ReplyResult(int id, const std::string& result) {
    std::string payload = "{\"jsonrpc\":\"2.0\",\"id\":";
    payload += std::to_string(id) + ",\"result\":";
    payload += result;
    payload += "}";
    Application::GetInstance().SendMcpMessage(payload);
}

void McpServer::ReplyError(int id, const std::string& message) {
    std::string payload = "{\"jsonrpc\":\"2.0\",\"id\":";
    payload += std::to_string(id);
    payload += ",\"error\":{\"message\":\"";
    payload += message;
    payload += "\"}}";
    Application::GetInstance().SendMcpMessage(payload);
}

void McpServer::GetToolsList(int id, const std::string& cursor) {
    const int max_payload_size = 8000;
    std::string json = "{\"tools\":[";
    
    bool found_cursor = cursor.empty();
    auto it = tools_.begin();
    std::string next_cursor = "";
    
    while (it != tools_.end()) {
        // 如果我们还没有找到起始位置，继续搜索
        if (!found_cursor) {
            if ((*it)->name() == cursor) {
                found_cursor = true;
            } else {
                ++it;
                continue;
            }
        }
        
        // 添加tool前检查大小
        std::string tool_json = (*it)->to_json() + ",";
        if (json.length() + tool_json.length() + 30 > max_payload_size) {
            // 如果添加这个tool会超出大小限制，设置next_cursor并退出循环
            next_cursor = (*it)->name();
            break;
        }
        
        json += tool_json;
        ++it;
    }
    
    if (json.back() == ',') {
        json.pop_back();
    }
    
    if (json.back() == '[' && !tools_.empty()) {
        // 如果没有添加任何tool，返回错误
        ESP_LOGE(TAG, "tools/list: Failed to add tool %s because of payload size limit", next_cursor.c_str());
        ReplyError(id, "Failed to add tool " + next_cursor + " because of payload size limit");
        return;
    }

    if (next_cursor.empty()) {
        json += "]}";
    } else {
        json += "],\"nextCursor\":\"" + next_cursor + "\"}";
    }
    
    ReplyResult(id, json);
}

void McpServer::DoToolCall(int id, const std::string& tool_name, const cJSON* tool_arguments, int stack_size) {
    auto tool_iter = std::find_if(tools_.begin(), tools_.end(), 
                                 [&tool_name](const McpTool* tool) { 
                                     return tool->name() == tool_name; 
                                 });
    
    if (tool_iter == tools_.end()) {
        ESP_LOGE(TAG, "tools/call: Unknown tool: %s", tool_name.c_str());
        ReplyError(id, "Unknown tool: " + tool_name);
        return;
    }

    PropertyList arguments = (*tool_iter)->properties();
    try {
        for (auto& argument : arguments) {
            bool found = false;
            if (cJSON_IsObject(tool_arguments)) {
                auto value = cJSON_GetObjectItem(tool_arguments, argument.name().c_str());
                if (argument.type() == kPropertyTypeBoolean && cJSON_IsBool(value)) {
                    argument.set_value<bool>(value->valueint == 1);
                    found = true;
                } else if (argument.type() == kPropertyTypeInteger && cJSON_IsNumber(value)) {
                    argument.set_value<int>(value->valueint);
                    found = true;
                } else if (argument.type() == kPropertyTypeString && cJSON_IsString(value)) {
                    argument.set_value<std::string>(value->valuestring);
                    found = true;
                }
            }

            if (!argument.has_default_value() && !found) {
                ESP_LOGE(TAG, "tools/call: Missing valid argument: %s", argument.name().c_str());
                ReplyError(id, "Missing valid argument: " + argument.name());
                return;
            }
        }
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "tools/call: %s", e.what());
        ReplyError(id, e.what());
        return;
    }

    // Start a task to receive data with stack size
    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.thread_name = "tool_call";
    cfg.stack_size = stack_size;
    cfg.prio = 1;
    esp_pthread_set_cfg(&cfg);

    // Use a thread to call the tool to avoid blocking the main thread
    tool_call_thread_ = std::thread([this, id, tool_iter, arguments = std::move(arguments)]() {
        try {
            ReplyResult(id, (*tool_iter)->Call(arguments));
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "tools/call: %s", e.what());
            ReplyError(id, e.what());
        }
    });
    tool_call_thread_.detach();
}