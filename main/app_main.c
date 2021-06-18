/*
 * Copyright (c) 2020 <Mark Buckaway> MIT License
 * 
 * HomeKit GarageDoor Project
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/queue.h"
#include <esp_event.h>
#include <esp_log.h>

#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>

//#include <hap_fw_upgrade.h>
#include <iot_button.h>

#include "wifi.h"
#include <app_hap_setup_payload.h>

#include "sprinkler.h"
#include "homekit_states.h"
#include "led.h"

/*  Required for server verification during OTA, PEM format as string  */
char server_cert[] = {};

static const char *TAG = "HAP";

static const uint16_t SPRINKLER_TASK_PRIORITY = 5;
static const uint16_t SPRINKLER_TASK_STACKSIZE = 4 * 1024;
static const char *SPRINKLER_TASK_NAME = "hap_sprinkler";

/* Reset network credentials if button is pressed for more than 3 seconds and then released */
//static const uint16_t RESET_NETWORK_BUTTON_TIMEOUT = 3;

/* Reset to factory if button is pressed and held for more than 10 seconds */
static const uint16_t RESET_TO_FACTORY_BUTTON_TIMEOUT = 10;

/* The button "Boot" will be used as the Reset button for the example */
static const uint16_t RESET_GPIO = GPIO_NUM_0;

/* Char definitions for our switches */
static hap_char_t *zone1_valve_inuse_char = 0;
static hap_char_t *zone2_valve_inuse_char = 0;
static hap_char_t *master_valve_inuse_char = 0;
static bool reset_requested = false;

/**
 * @brief The factory reset button callback handler.
 */
void reset_to_factory_handler(void)
{
    // Only allow the routine to be called once. It reboots the device so we start over.
    if (!reset_requested)
    {
        hap_reset_to_factory();
        reset_requested = true;
    }
}

/**
 * The Reset button  GPIO initialisation function.
 * Same button will be used for resetting Wi-Fi network as well as for reset to factory based on
 * the time for which the button is pressed. Reset WIFI is not used for hard coded WIFI setup.
 */
static void reset_key_init(uint32_t key_gpio_pin)
{
    button_handle_t handle = iot_button_create(key_gpio_pin, BUTTON_ACTIVE_LOW);
//    iot_button_add_on_release_cb(handle, RESET_NETWORK_BUTTON_TIMEOUT, reset_network_handler, NULL);
    iot_button_add_on_press_cb(handle, RESET_TO_FACTORY_BUTTON_TIMEOUT, (button_cb)reset_to_factory_handler, NULL);
}

/**
 * Accessory Identity routine. Does nothing other than long the event because the device has no
 * LED to let the user know who we are.
 */
static int sprinkler_identify(hap_acc_t *ha)
{
    ESP_LOGI(TAG, "Sprinkler Controller Accessory identified");
    return HAP_SUCCESS;
}

/**
 * Event handler to report what HAP is doing. Useful for debugging.
 */
static void sprinkler_hap_event_handler(void* arg, esp_event_base_t event_base, int event, void *data)
{
    switch(event) {
        case HAP_EVENT_PAIRING_STARTED :
            ESP_LOGI(TAG, "Pairing Started");
            break;
        case HAP_EVENT_PAIRING_ABORTED :
            ESP_LOGI(TAG, "Pairing Aborted");
            break;
        case HAP_EVENT_CTRL_PAIRED :
            ESP_LOGI(TAG, "Controller %s Paired. Controller count: %d",
                        (char *)data, hap_get_paired_controller_count());
            break;
        case HAP_EVENT_CTRL_UNPAIRED :
            ESP_LOGI(TAG, "Controller %s Removed. Controller count: %d",
                        (char *)data, hap_get_paired_controller_count());
            break;
        case HAP_EVENT_CTRL_CONNECTED :
            ESP_LOGI(TAG, "Controller %s Connected", (char *)data);
            break;
        case HAP_EVENT_CTRL_DISCONNECTED :
            ESP_LOGI(TAG, "Controller %s Disconnected", (char *)data);
            break;
        case HAP_EVENT_ACC_REBOOTING : {
            char *reason = (char *)data;
            ESP_LOGI(TAG, "Accessory Rebooting (Reason: %s)",  reason ? reason : "null");
            break;
        }
        default:
            /* Silently ignore unknown events */
            break;
    }
}

void zone1_valve_update(uint8_t state)
{
        hap_val_t new_val;
        new_val.i = state;
        hap_char_update_val(zone1_valve_inuse_char, &new_val);
}

void zone2_valve_update(uint8_t state)
{
        hap_val_t new_val;
        new_val.i = state;
        hap_char_update_val(zone2_valve_inuse_char, &new_val);
}

void master_valve_update(uint8_t state)
{
        hap_val_t new_val;
        new_val.i = state;
        hap_char_update_val(master_valve_inuse_char, &new_val);
}

/* 
 * @brief Check the current status of the zone 1 valve and return it to homekit
 */
static int zone1_valve_read(hap_char_t *hc, hap_status_t *status_code, void *serv_priv, void *read_priv)
{
    if (hap_req_get_ctrl_id(read_priv)) {
        ESP_LOGI(TAG, "sprinkler received read from %s", hap_req_get_ctrl_id(read_priv));
    }
    if (!strcmp(hap_char_get_type_uuid(hc), HAP_CHAR_UUID_IN_USE)) 
    {
        led2_on();
        hap_val_t new_val;
        new_val.i = get_valve_state(VALUE_ZONE1);
        hap_char_update_val(hc, &new_val);
        zone1_valve_update(new_val.i);
        *status_code = HAP_STATUS_SUCCESS;
        ESP_LOGI(TAG,"sprinkler status read as %s", valve_current_state_string(new_val.i));
        led_off();
    }
    return HAP_SUCCESS;
}

/**
 * @brief Activate the Zone1 valve when homekit asks for it.
 */
static int zone1_valve_write(hap_write_data_t write_data[], int count,
        void *serv_priv, void *write_priv)
{
    if (hap_req_get_ctrl_id(write_priv)) {
        ESP_LOGI(TAG, "sprinkler received write from %s", hap_req_get_ctrl_id(write_priv));
    }
    ESP_LOGI(TAG, "sprinkler write called with %d chars", count);
    int i, ret = HAP_SUCCESS;
    led1_on();
    hap_write_data_t *write;
    for (i = 0; i < count; i++) {
        write = &write_data[i];
        if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ACTIVE)) {
            ESP_LOGI(TAG, "sprinkler received write In Use: %s", valve_current_state_string(write->val.i));
            set_valve_state(VALUE_ZONE1, write->val.i);
            hap_char_update_val(write->hc, &(write->val));
            zone1_valve_update(write->val.i);
            *(write->status) = HAP_STATUS_SUCCESS;
        } else {
            *(write->status) = HAP_STATUS_RES_ABSENT;
        }
    }
    led_off();
    return ret;
}

/* 
 * @brief Check the current status of the zone2 valve and return it to homekit
 */
static int zone2_valve_read(hap_char_t *hc, hap_status_t *status_code, void *serv_priv, void *read_priv)
{
    if (hap_req_get_ctrl_id(read_priv)) {
        ESP_LOGI(TAG, "sprinkler received read from %s", hap_req_get_ctrl_id(read_priv));
    }
    if (!strcmp(hap_char_get_type_uuid(hc), HAP_CHAR_UUID_ACTIVE)) 
    {
        led2_on();
        hap_val_t new_val;
        new_val.i = get_valve_state(VALUE_ZONE2);
        hap_char_update_val(hc, &new_val);
        zone2_valve_update(new_val.i);
        *status_code = HAP_STATUS_SUCCESS;
        ESP_LOGI(TAG,"sprinkler status read as %s", valve_current_state_string(new_val.i));
        led_off();
    }
    return HAP_SUCCESS;
}

/**
 * @brief Activate the zone2 valve when homekit asks for it.
 */
static int zone2_valve_write(hap_write_data_t write_data[], int count,
        void *serv_priv, void *write_priv)
{
    if (hap_req_get_ctrl_id(write_priv)) {
        ESP_LOGI(TAG, "sprinkler received write from %s", hap_req_get_ctrl_id(write_priv));
    }
    ESP_LOGI(TAG, "sprinkler write called with %d chars", count);
    int i, ret = HAP_SUCCESS;
    hap_write_data_t *write;
    led1_on();
    for (i = 0; i < count; i++) {
        write = &write_data[i];
        if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ACTIVE)) {
            ESP_LOGI(TAG, "sprinkler received write In Use: %s", valve_current_state_string(write->val.i));
            set_valve_state(VALUE_ZONE2, write->val.i);
            hap_char_update_val(write->hc, &(write->val));
            zone2_valve_update(write->val.i);
            *(write->status) = HAP_STATUS_SUCCESS;
        } else {
            *(write->status) = HAP_STATUS_RES_ABSENT;
        }
    }
    led_off();
    return ret;
}


/* 
 * @brief Check the current status of the Master Valve and return it to homekit
 */
static int master_valve_read(hap_char_t *hc, hap_status_t *status_code, void *serv_priv, void *read_priv)
{
    if (hap_req_get_ctrl_id(read_priv)) {
        ESP_LOGI(TAG, "sprinkler received read from %s", hap_req_get_ctrl_id(read_priv));
    }
    if (!strcmp(hap_char_get_type_uuid(hc), HAP_CHAR_UUID_ACTIVE)) 
    {
        led2_on();
        hap_val_t new_val;
        new_val.i = get_valve_state(VALUE_MASTER);
        hap_char_update_val(hc, &new_val);
        master_valve_update(new_val.i);
        *status_code = HAP_STATUS_SUCCESS;
        ESP_LOGI(TAG,"sprinkler status read as %s", valve_current_state_string(new_val.i));
        led_off();
    }
    return HAP_SUCCESS;
}

/**
 * @brief Open the Master valve when homekit asks for it.
 */
static int master_valve_write(hap_write_data_t write_data[], int count,
        void *serv_priv, void *write_priv)
{
    if (hap_req_get_ctrl_id(write_priv)) {
        ESP_LOGI(TAG, "sprinkler received write from %s", hap_req_get_ctrl_id(write_priv));
    }
    ESP_LOGI(TAG, "sprinkler write called with %d chars", count);
    led1_on();
    int i, ret = HAP_SUCCESS;
    hap_write_data_t *write;
    for (i = 0; i < count; i++) {
        write = &write_data[i];
        if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ACTIVE)) {
            ESP_LOGI(TAG, "sprinkler received write In Use: %s", valve_current_state_string(write->val.i));
            set_valve_state(VALUE_MASTER, write->val.i);
            hap_char_update_val(write->hc, &(write->val));
            master_valve_update(write->val.i);
            *(write->status) = HAP_STATUS_SUCCESS;
        } else {
            *(write->status) = HAP_STATUS_RES_ABSENT;
        }
    }
    led_off();
    return ret;
}

/**
 * @brief Main Thread to handle setting up the service and accessories for the GarageDoor
 */
static void homekit_thread_entry(void *p)
{
    hap_acc_t *sprinkleraccessory = NULL;
    hap_serv_t *zone1valveservice = NULL;
    hap_serv_t *zone2valveservice = NULL;
    hap_serv_t *mastervalveservice = NULL;

    /*
     * Configure the GPIO for the Sprinkler value relays
     */
    sprinkler_setup();

    /*
     * Setup the reset button to reset homekit to defaults
     */
    reset_key_init(RESET_GPIO);

    /* Configure HomeKit core to make the Accessory name (and thus the WAC SSID) unique,
     * instead of the default configuration wherein only the WAC SSID is made unique.
     */
    ESP_LOGI(TAG, "configuring HAP");
    hap_cfg_t hap_cfg;
    hap_get_config(&hap_cfg);
    hap_cfg.unique_param = UNIQUE_NAME;
    hap_set_config(&hap_cfg);

    ESP_LOGI(TAG, "initializing HAP");
    /* Initialize the HAP core */
    hap_init(HAP_TRANSPORT_WIFI);

    /* Initialise the mandatory parameters for Accessory which will be added as
     * the mandatory services internally
     */

// Missing from the ESP32 SDK
#define HAP_CID_SPRINKLERS 28

    hap_acc_cfg_t cfg = {
        .name = "Esp-Sprinkler",
        .manufacturer = "Espressif",
        .model = "EspSprinkler01",
        .serial_num = "001122334455",
        .fw_rev = "0.9.0",
        .hw_rev =  (char*)esp_get_idf_version(),
        .pv = "1.1.0",
        .identify_routine = sprinkler_identify,
        .cid = HAP_CID_SPRINKLERS,
    };
    ESP_LOGI(TAG, "Creating sprinkler accessory...");
    /* Create accessory object */
    sprinkleraccessory = hap_acc_create(&cfg);

    /* Add a dummy Product Data */
    uint8_t product_data[] = {'E','S','P','3','2','H','A','P'};
    hap_acc_add_product_data(sprinkleraccessory, product_data, sizeof(product_data));

    ESP_LOGI(TAG, "Creating valve zone 1 service");

    /* Create the Valve Service. Include the "name" since this is a user visible service  */
    zone1valveservice = hap_serv_valve_create(ACTIVETYPE_INACTIVE, INUSE_INUSE, VALVETYPE_IRRIGRATION);
    hap_serv_add_char(zone1valveservice, hap_char_name_create("Zone 1 Irrigation Value"));
    /* Set the write callback for the service */
    hap_serv_set_write_cb(zone1valveservice, zone1_valve_write);
    /* Set the read callback for the service (optional) */
    hap_serv_set_read_cb(zone1valveservice, zone1_valve_read);
    /* Add the Garage Service to the Accessory Object */
    hap_acc_add_serv(sprinkleraccessory, zone1valveservice);
    zone1_valve_inuse_char = hap_serv_get_char_by_uuid(zone1valveservice, HAP_CHAR_UUID_IN_USE);

    /* Create the Valve Service. Include the "name" since this is a user visible service  */
    zone2valveservice = hap_serv_valve_create(ACTIVETYPE_INACTIVE, INUSE_INUSE, VALVETYPE_IRRIGRATION);
    hap_serv_add_char(zone2valveservice, hap_char_name_create("Zone 2 Irrigation Value"));
    /* Set the write callback for the service */
    hap_serv_set_write_cb(zone2valveservice, zone2_valve_write);
    /* Set the read callback for the service (optional) */
    hap_serv_set_read_cb(zone2valveservice, zone2_valve_read);
    /* Add the Garage Service to the Accessory Object */
    hap_acc_add_serv(sprinkleraccessory, zone2valveservice);
    zone2_valve_inuse_char = hap_serv_get_char_by_uuid(zone2valveservice, HAP_CHAR_UUID_IN_USE);

    /* Create the Valve Service. Include the "name" since this is a user visible service  */
    mastervalveservice = hap_serv_valve_create(ACTIVETYPE_INACTIVE, INUSE_INUSE, VALVETYPE_IRRIGRATION);
    hap_serv_add_char(mastervalveservice, hap_char_name_create("Master Irrigation Value"));
    /* Set the write callback for the service */
    hap_serv_set_write_cb(mastervalveservice, master_valve_write);
    /* Set the read callback for the service (optional) */
    hap_serv_set_read_cb(mastervalveservice, master_valve_read);
    /* Add the Garage Service to the Accessory Object */
    hap_acc_add_serv(sprinkleraccessory, mastervalveservice);
    master_valve_inuse_char = hap_serv_get_char_by_uuid(mastervalveservice, HAP_CHAR_UUID_IN_USE);

#if 0
    /* Create the Firmware Upgrade HomeKit Custom Service.
     * Please refer the FW Upgrade documentation under components/homekit/extras/include/hap_fw_upgrade.h
     * and the top level README for more information.
     */
    hap_fw_upgrade_config_t ota_config = {
        .server_cert_pem = server_cert,
    };
    service = hap_serv_fw_upgrade_create(&ota_config);
    /* Add the service to the Accessory Object */
    hap_acc_add_serv(accessory, service);
#endif

    /* Add the Accessory to the HomeKit Database */
    ESP_LOGI(TAG, "Adding Irrigation Accessory...");
    hap_add_accessory(sprinkleraccessory);

    /* Register an event handler for HomeKit specific events */
    esp_event_handler_register(HAP_EVENT, ESP_EVENT_ANY_ID, &sprinkler_hap_event_handler, NULL);

    /* Query the controller count (just for information) */
    ESP_LOGI(TAG, "Accessory is paired with %d controllers", hap_get_paired_controller_count());


    /* For production accessories, the setup code shouldn't be programmed on to
     * the device. Instead, the setup info, derived from the setup code must
     * be used. Use the factory_nvs_gen utility to generate this data and then
     * flash it into the factory NVS partition.
     *
     * By default, the setup ID and setup info will be read from the factory_nvs
     * Flash partition and so, is not required to set here explicitly.
     *
     * However, for testing purpose, this can be overridden by using hap_set_setup_code()
     * and hap_set_setup_id() APIs, as has been done here.
     * 
     * That said, this homekit controller is meant for home use, and not for production,
     * so it it OK to hard code the device info.
     */
#ifdef CONFIG_HOMEKIT_USE_HARDCODED_SETUP_CODE
    /* Unique Setup code of the format xxx-xx-xxx. Default: 111-22-333 */
    hap_set_setup_code(CONFIG_HOMEKIT_SETUP_CODE);
    /* Unique four character Setup Id. Default: ES32 */
    hap_set_setup_id(CONFIG_HOMEKIT_SETUP_ID);
#ifdef CONFIG_APP_WIFI_USE_WAC_PROVISIONING
    app_hap_setup_payload(CONFIG_HOMEKIT_SETUP_CODE, CONFIG_HOMEKIT_SETUP_ID, true, cfg.cid);
#else
    app_hap_setup_payload(CONFIG_HOMEKIT_SETUP_CODE, CONFIG_HOMEKIT_SETUP_ID, false, cfg.cid);
#endif
#endif

    /* mfi is not supported */
    hap_enable_mfi_auth(HAP_MFI_AUTH_NONE);

    ESP_LOGI(TAG, "Starting WIFI...");
    /* Initialize Wi-Fi */
    wifi_setup();
    wifi_connect();

    wifi_waitforconnect();
    led1_on();

    /* After all the initializations are done, start the HAP core */
    ESP_LOGI(TAG, "Starting HAP...");
    hap_start();
    
    ESP_LOGI(TAG, "HAP initialization complete.");
    led_off();

    /* The task ends here. The read/write callbacks will be invoked by the HAP Framework */
    vTaskDelete(NULL);
}

void app_main()
{
    ESP_LOGI(TAG, "[APP] Startup...");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_LOGI(TAG, "[APP] Creating main thread...");
    configure_led();
    led_both();

    xTaskCreate(homekit_thread_entry, SPRINKLER_TASK_NAME, SPRINKLER_TASK_STACKSIZE, NULL, SPRINKLER_TASK_PRIORITY, NULL);
}
