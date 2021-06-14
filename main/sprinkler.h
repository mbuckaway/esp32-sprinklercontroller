#pragma once

#include <stdlib.h>
#include <stdio.h>

enum ValveNo {
    VALUE_ZONE1,
    VALUE_ZONE2,
    VALUE_MASTER
};

void sprinkler_setup(void);
void start_sprinkler(void);

/**
 * @brief Set the value state (on/off)
 * 
 * @param valueno Valve number (ValveNo type)
 * @param inuse Valve in use (in use = active and flowing)
 */
void set_valve_state(u_int8_t valueno, u_int8_t inuse);

/**
 * @brief Get the value state 
 * 
 * @param valueno Valve number (ValveNo type)
 * @return uint8_t State of the valve (in use or not in use)
 */
uint8_t get_valve_state(u_int8_t valueno);
