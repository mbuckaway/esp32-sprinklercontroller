#pragma once

#include <stdlib.h>

/**
 * TargetDoorState enum - matches homekit status for same
    Characteristic.TargetDoorState.OPEN = 0;
    Characteristic.TargetDoorState.CLOSED = 1;
 */
enum TargetDoorState {
    TARGET_STATE_OPEN = 0,
    TARGET_STATE_CLOSED = 1
};

/**
 * CurrentDoorState enum - matches homekit status for same
    Characteristic.CurrentDoorState.OPEN = 0;
    Characteristic.CurrentDoorState.CLOSED = 1;
    Characteristic.CurrentDoorState.OPENING = 2;
    Characteristic.CurrentDoorState.CLOSING = 3;
    Characteristic.CurrentDoorState.STOPPED = 4;
 */
enum CurrentDoorState {
    CURRENT_STATE_OPEN = 0,
    CURRENT_STATE_CLOSED = 1,
    CURRENT_STATE_OPENING = 2,
    CURRENT_STATE_CLOSING = 3,
    CURRENT_STATE_STOPPED = 4
};

/**
 * ContactState enum - matches the homekit status for same
 * Characteristic.ContactSensorState.CONTACT_DETECTED = 0;
 * Characteristic.ContactSensorState.CONTACT_NOT_DETECTED = 1;
 */
enum ContactState {
    CONTACT_DETECTED = 0,
    CONTACT_NOT_DETECTED = 1
};

/**
 * In Use.
 *
 * This characteristic describes if the service is in use. The service must be "Active" before the value of this
 * characteristic can be set to in use.  A valve is set to "In Use" when there are
 * fluid flowing through the valve.
 *
 * This characteristic requires iOS 11.2 or later.
 */

enum InUseState {
    INUSE_NOTINUSE = 0,
    INUSE_INUSE = 1
};

/**
 * Valve Type.
 *
 * This characteristic describes the type of valve.
 */
enum ValveType {
    VALVETYPE_GENERIC = 0,
    VALVETYPE_IRRIGRATION = 1,
    VALVETYPE_SHOWERHEAD = 2,
    VALVETYPE_WATERFAUCET = 3,
};


/**
 * Active.
 *
 * This characteristic indicates whether the service is currently active.
 */
enum ActiveType {
    ACTIVETYPE_INACTIVE = 0,
    ACTIVETYPE_ACTIVE = 1
};

char *valve_current_state_string(uint8_t state);
