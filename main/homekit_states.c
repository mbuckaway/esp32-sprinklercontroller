#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "homekit_states.h"

static char *valve_current_states[] = {
    "in use",
    "not in use"
};

/**
 * @brief Change the Garage Door current status to a string
 *
 * @param(status) - current status
 * 
 * @return string to the name of the status
 */

char *valve_current_state_string(uint8_t state)
{
    if (state>INUSE_NOTINUSE)
    {
        state = INUSE_NOTINUSE;
    }
    return (valve_current_states[state]);
}
