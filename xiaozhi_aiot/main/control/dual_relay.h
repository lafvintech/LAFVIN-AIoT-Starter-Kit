#ifndef _DUAL_RELAY_H_
#define _DUAL_RELAY_H_

#include <driver/gpio.h>
#include <esp_log.h>

/**
 * @brief Dual Relay Controller Class
 * 
 * Controls two relay channels using GPIO pins
 */
class DualRelay {
public:
    /**
     * @brief Construct a new Dual Relay object
     * 
     * @param gpio_1 GPIO pin for first relay channel
     * @param gpio_2 GPIO pin for second relay channel
     * @param active_high true if relay is activated on high level (default), false for active low
     */
    DualRelay(gpio_num_t gpio_1, gpio_num_t gpio_2, bool active_high = true);
    
    /**
     * @brief Destroy the Dual Relay object
     */
    ~DualRelay();
    
    /**
     * @brief Set the state of a specific relay channel
     * 
     * @param channel Channel number (1 or 2)
     * @param state true to turn on, false to turn off
     * @return true if successful, false otherwise
     */
    bool SetChannel(int channel, bool state);
    
    /**
     * @brief Set the state of both relay channels
     * 
     * @param state_1 State for channel 1
     * @param state_2 State for channel 2
     */
    void SetBoth(bool state_1, bool state_2);
    
    /**
     * @brief Get the state of a specific relay channel
     * 
     * @param channel Channel number (1 or 2)
     * @return true if relay is on, false if off
     */
    bool GetChannelState(int channel) const;
    
    /**
     * @brief Turn on both relay channels
     */
    void TurnOnAll();
    
    /**
     * @brief Turn off both relay channels
     */
    void TurnOffAll();
    
    /**
     * @brief Get the singleton instance
     * 
     * @return DualRelay* Pointer to the singleton instance
     */
    static DualRelay* GetInstance() { return instance_; }
    
    /**
     * @brief Initialize the singleton instance
     * 
     * @param gpio_1 GPIO pin for first relay channel
     * @param gpio_2 GPIO pin for second relay channel
     * @param active_high true if relay is activated on high level, false for active low
     */
    static void Init(gpio_num_t gpio_1, gpio_num_t gpio_2, bool active_high = true);
    
private:
    gpio_num_t gpio_1_;
    gpio_num_t gpio_2_;
    bool active_high_;
    bool state_1_;
    bool state_2_;
    
    // Apply GPIO level based on state and active_high_ setting
    void ApplyGpioLevel(gpio_num_t gpio, bool state);
    
    // Singleton instance
    static DualRelay* instance_;
};

#endif // _DUAL_RELAY_H_ 