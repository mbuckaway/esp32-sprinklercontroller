#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/queue.h"
#include <esp_event.h>
#include <esp_log.h>
#include "driver/gpio.h"
//#include <iot_button.h>
#include "sprinkler.h"
#include "app_main.h"
#include "homekit_states.h"


static const char *TAG = "GDGPIO";

static const long long unsigned int GPIO_OUTPUT_PIN_SEL = (1ULL<<CONFIG_GPIO_OUTPUT_IO_RELAY_ZONE1) | (1ULL<<CONFIG_GPIO_OUTPUT_IO_RELAY_ZONE2) | (1ULL<<CONFIG_GPIO_OUTPUT_IO_RELAY_MASTER);
//static const long long unsigned int GPIO_INPUT_PIN_SEL = ((1ULL<<CONFIG_GPIO_INPUT_IO_OPEN) | (1ULL<<CONFIG_GPIO_INPUT_IO_CLOSE));
//static const uint16_t ESP_INTR_FLAG_DEFAULT = 0;

/**
 * @brief Set the valve state (on/off)
 * 
 * @param valveno Valve number (ValveNo type)
 * @param inuse Valve in use (in use = active and flowing)
 */
void set_valve_state(u_int8_t valveno, u_int8_t inuse)
{
    ESP_LOGI(TAG, "Enabling relay %d...", valveno);
    // Check that the GPIO pin is a digital and not ADC!
    int gpio_port = CONFIG_GPIO_OUTPUT_IO_RELAY_ZONE1;
    switch (valveno)
    {
        case VALUE_ZONE1:
            gpio_port = CONFIG_GPIO_OUTPUT_IO_RELAY_ZONE1;
            break;
        case VALUE_ZONE2:
            gpio_port = CONFIG_GPIO_OUTPUT_IO_RELAY_ZONE2;
            break;
        case VALUE_MASTER:
            gpio_port = CONFIG_GPIO_OUTPUT_IO_RELAY_MASTER;
            break;
        default:
            gpio_port = CONFIG_GPIO_OUTPUT_IO_RELAY_ZONE1;
            break;
    }
    gpio_set_level(gpio_port, inuse);
}

/**
 * @brief Get the valve state 
 * 
 * @param valveno Valve number (ValveNo type)
 * @return uint8_t State of the valve (in use or not in use)
 */
u_int8_t get_valve_state(u_int8_t valveno)
{
    ESP_LOGI(TAG, "Enabling relay %d...", valveno);
    // Check that the GPIO pin is a digital and not ADC!
    int gpio_port = CONFIG_GPIO_OUTPUT_IO_RELAY_ZONE1;
    switch (valveno)
    {
        case VALUE_ZONE1:
            gpio_port = CONFIG_GPIO_OUTPUT_IO_RELAY_ZONE1;
            break;
        case VALUE_ZONE2:
            gpio_port = CONFIG_GPIO_OUTPUT_IO_RELAY_ZONE2;
            break;
        case VALUE_MASTER:
            gpio_port = CONFIG_GPIO_OUTPUT_IO_RELAY_MASTER;
            break;
        default:
            gpio_port = CONFIG_GPIO_OUTPUT_IO_RELAY_ZONE1;
            break;
    }
    return gpio_get_level(gpio_port)?INUSE_NOTINUSE:INUSE_INUSE;
}


/**
 * @brief Setup the GPIO, ISR, and event queue
 */

void sprinkler_setup(void)
{
    gpio_config_t io_out_conf = {
        .intr_type = GPIO_PIN_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = GPIO_OUTPUT_PIN_SEL,
        .pull_down_en = 0,
        .pull_up_en = 0
    };

    gpio_config(&io_out_conf);

    ESP_LOGI(TAG, "Sprinkler GPIO configured");
}
