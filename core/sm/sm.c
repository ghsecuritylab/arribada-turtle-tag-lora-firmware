/* sm.c - Main state machine
 *
 * Copyright (C) 2018 Arribada
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "bsp.h"
#include "cexception.h"
#include "exceptions.h"
#include "cmd.h"
#include "config_if.h"
#include "debug.h"
#include "sm.h"
#include "sys_config.h"
#include "syshal_gpio.h"
#include "syshal_i2c.h"
#include "syshal_spi.h"
#include "syshal_uart.h"
#include "syshal_batt.h"
#include "syshal_gps.h"
#include "syshal_usb.h"
#include "version.h"

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Local variables ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static const char * sm_state_str[] =
{
    [SM_STATE_BOOT]                         = "SM_STATE_BOOT",
    [SM_STATE_STANDBY_BATTERY_CHARGING]     = "SM_STATE_STANDBY_BATTERY_CHARGING",
    [SM_STATE_STANDBY_BATTERY_LEVEL_LOW]    = "SM_STATE_STANDBY_BATTERY_LEVEL_LOW",
    [SM_STATE_STANDBY_LOG_FILE_FULL]        = "SM_STATE_STANDBY_LOG_FILE_FULL",
    [SM_STATE_STANDBY_PROVISIONING_NEEDED]  = "SM_STATE_STANDBY_PROVISIONING_NEEDED",
    [SM_STATE_STANDBY_TRIGGER_PENDING]      = "SM_STATE_STANDBY_TRIGGER_PENDING",
    [SM_STATE_PROVISIONING]                 = "SM_STATE_PROVISIONING",
    [SM_STATE_OPERATIONAL]                  = "SM_STATE_OPERATIONAL",
};

static sm_state_t state = SM_STATE_BOOT; // Default starting state
static uint8_t tx_buffer[CMD_MAX_SIZE]; // TX buffer used by config_if
static uint8_t rx_buffer[CMD_MAX_SIZE]; // RX buffer used by config_if

static volatile bool config_if_tx_pending = false; // Is a transmit pending
static volatile bool config_if_rx_pending = false; // Is a receive pending
static volatile uint16_t config_if_rx_size; // What was the size of the last receive in bytes

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// PROTOTYPES //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
////////////////////////////// HARDWARE INIT CODE //////////////////////////////
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// REQUEST HANDLERS //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void cfg_read_req(cmd_t *req, uint16_t size)
{
    // Check request size is correct
    if (CMD_SIZE(cmd_cfg_read_req_t) != size)
        Throw(EXCEPTION_REQ_WRONG_SIZE);

    // If we're already currently responding to another request
    if (config_if_tx_pending)
        Throw(EXCEPTION_RESP_TX_PENDING);

    cmd_cfg_read_req_t * req_data = (cmd_cfg_read_req_t *) req;

    if (CFG_READ_REQ_READ_ALL == req_data->configuration_tag) // Read all configuration tags
    {
        DEBUG_PR_WARN("READ ALL TAGS IN %s() NOT IMPLEMENTED", __FUNCTION__);
        return;
    }

    // Read just one configuration tag
    cmd_t * resp = (cmd_t *) &tx_buffer[0];
    CMD_SET_HDR(resp, CMD_CFG_READ_RESP);

    int return_code = sys_config_get(req_data->configuration_tag, &resp->p.cmd_cfg_read_resp.bytes);

    resp->p.cmd_cfg_read_resp.length = 0;

    if (SYS_CONFIG_ERROR_INVALID_TAG == return_code)  // Tag is not valid. Return an error code
    {
        resp->p.cmd_cfg_read_resp.error_code = CMD_ERROR_INVALID_CONFIG_TAG;
    }
    else if (SYS_CONFIG_ERROR_TAG_NOT_SET == return_code)  // Tag is not set. Return an error code
    {
        resp->p.cmd_cfg_read_resp.error_code = CMD_ERROR_CONFIG_TAG_NOT_SET;
    }
    else if (return_code < 0) // Unknown/Unhandled error
    {
        resp->p.cmd_cfg_read_resp.error_code = CMD_ERROR_UNKNOWN;
    }
    else
    {
        resp->p.cmd_cfg_read_resp.error_code = CMD_NO_ERROR;
        resp->p.cmd_cfg_read_resp.length = return_code;
    }

    config_if_tx_pending = true;
    config_if_send((uint8_t *) resp, CMD_SIZE(cmd_cfg_read_resp_t));
}

void cfg_write_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void cfg_save_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void cfg_restore_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void cfg_erase_req(cmd_t *req, uint16_t size)
{
    // Check request size is correct
    if (CMD_SIZE(cmd_cfg_erase_req_t) != size)
        Throw(EXCEPTION_REQ_WRONG_SIZE);

    // If we're already currently responding to another request
    if (config_if_tx_pending)
        Throw(EXCEPTION_RESP_TX_PENDING);

    cmd_cfg_erase_req_t * req_data = (cmd_cfg_erase_req_t *) req;

    if (CFG_READ_REQ_READ_ALL == req_data->configuration_tag) // Read all configuration tags
    {
        DEBUG_PR_WARN("READ ALL TAGS IN %s() NOT IMPLEMENTED", __FUNCTION__);
        return;
    }

    // Erase just one configuration tag
    cmd_t * resp = (cmd_t *) &tx_buffer[0];
    CMD_SET_HDR(resp, CMD_GENERIC_RESP);

    int return_code = sys_config_unset(req_data->configuration_tag);

    switch (return_code)
    {
        case SYS_CONFIG_ERROR_INVALID_TAG:
            resp->p.cmd_generic_resp.error_code = CMD_ERROR_INVALID_CONFIG_TAG;
            break;

        case SYS_CONFIG_NO_ERROR:
            resp->p.cmd_generic_resp.error_code = CMD_ERROR_INVALID_CONFIG_TAG;
            break;

        default:
            resp->p.cmd_generic_resp.error_code = CMD_ERROR_UNKNOWN;
            break;
    }

    config_if_tx_pending = true;
    config_if_send((uint8_t *) resp, CMD_SIZE(cmd_generic_resp_t));
}

void cfg_protect_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void cfg_unprotect_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void cfg_write_cnf(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void gps_write_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void gps_read_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void gps_config_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void ble_config_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void ble_write_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void ble_read_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void status_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);

    /*
    // Generate and send response
    cmd_t * resp;
    CMD_SET_HDR(resp, CMD_STATUS_RESP, sizeof(cmd_status_resp_t));

    resp->p.cmd_status_resp.error_code = CMD_NO_ERROR;
    resp->p.cmd_status_resp.firmware_version = ;
    resp->p.cmd_status_resp.firmware_checksum = ;
    resp->p.cmd_status_resp.configuration_format_version = ;
    */
}

void status_resp(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void fw_send_image_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void fw_send_image_complete_cnf(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void fw_apply_image_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void reset_req(cmd_t *req, uint16_t size)
{

    // Check request size is correct
    if (CMD_SIZE(cmd_reset_req_t) != size)
        Throw(EXCEPTION_REQ_WRONG_SIZE);

    // If we're already currently responding to another request
    if (config_if_tx_pending)
        Throw(EXCEPTION_RESP_TX_PENDING);

    // Generate and send response
    cmd_t * resp = (cmd_t *) &tx_buffer[0];
    CMD_SET_HDR(resp, CMD_GENERIC_RESP);

    resp->p.cmd_generic_resp.error_code = CMD_NO_ERROR;

    config_if_tx_pending = true;
    config_if_send((uint8_t *) resp, CMD_SIZE(cmd_generic_resp_t));

    // Wait for response to have been sent
    while (config_if_tx_pending)
    {}

#ifdef __CORTEX_M // FIXME: find and replace with the ARM macro
    NVIC_SystemReset();
#endif

    // If a system reset isn't available then block until a watchdog reset
    for (;;) {}
}

void battery_status_req(cmd_t *req, uint16_t size)
{

    // Check request size is correct
    if (size != CMD_SIZE_HDR)
        Throw(EXCEPTION_REQ_WRONG_SIZE);

    // If we're already currently responding to another request
    if (config_if_tx_pending)
        Throw(EXCEPTION_RESP_TX_PENDING);

    DEBUG_PR_WARN("%s NOT IMPLEMENTED, responding with spoof data", __FUNCTION__);

    // Generate and send response
    cmd_t * resp = (cmd_t *) &tx_buffer[0];
    CMD_SET_HDR(resp, CMD_BATTERY_STATUS_RESP);

    resp->p.cmd_battery_status_resp.error_code = CMD_NO_ERROR;
    resp->p.cmd_battery_status_resp.charging_indicator = 1;
    resp->p.cmd_battery_status_resp.charge_level = 100;

    /*
    resp->p.cmd_battery_status_resp.error_code = CMD_NO_ERROR;
    resp->p.cmd_battery_status_resp.charging_indicator = syshal_batt_charging();
    resp->p.cmd_battery_status_resp.charge_level = syshal_batt_level();
    */

    config_if_tx_pending = true;
    config_if_send((uint8_t *) resp, CMD_SIZE(cmd_battery_status_resp_t));

}

void log_create_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void log_erase_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

void log_read_req(cmd_t *req, uint16_t size)
{
    DEBUG_PR_WARN("%s NOT IMPLEMENTED", __FUNCTION__);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// CMD HANDLERS ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

int config_if_event_handler(config_if_event_t * event)
{
    // This is called from an interrupt so we'll keep it short
    switch (event->id)
    {

        case CONFIG_IF_EVENT_SEND_COMPLETE:
            DEBUG_PR_TRACE("CONFIG_IF_EVENT_SEND_COMPLETE");
            config_if_tx_pending = false;
            break;

        case CONFIG_IF_EVENT_RECEIVE_COMPLETE:
            DEBUG_PR_TRACE("CONFIG_IF_EVENT_RECEIVE_COMPLETE");
            config_if_rx_pending = true; // Indicate that there is a packet in the RX buffer that needs processing
            config_if_rx_size = event->receive.size;// Store the size of the packet
            break;

        case CONFIG_IF_EVENT_CONNECTED:
            DEBUG_PR_TRACE("CONFIG_IF_EVENT_CONNECTED");
            break;

        case CONFIG_IF_EVENT_DISCONNECTED:
            DEBUG_PR_TRACE("CONFIG_IF_EVENT_DISCONNECTED");
            // Clear all pending transmissions/receptions
            config_if_tx_pending = false;
            config_if_rx_pending = false;
            config_if_rx_size = 0;
            break;

    }

    return CONFIG_IF_NO_ERROR;
}

static void handle_config_if_requests(void)
{
    // Process any pending event outside of an interrupt
    if (config_if_rx_pending)
    {

        cmd_t * req = (cmd_t *) &rx_buffer[0]; // Overlay command structure on receive buffer

        switch (req->h.cmd)
        {
            case CMD_CFG_READ_REQ:
                DEBUG_PR_INFO("CFG_READ_REQ");
                cfg_read_req(req, config_if_rx_size);
                break;

            case CMD_CFG_WRITE_REQ:
                DEBUG_PR_INFO("CFG_WRITE_REQ");
                cfg_write_req(req, config_if_rx_size);
                break;

            case CMD_CFG_SAVE_REQ:
                DEBUG_PR_INFO("CFG_SAVE_REQ");
                cfg_save_req(req, config_if_rx_size);
                break;

            case CMD_CFG_RESTORE_REQ:
                DEBUG_PR_INFO("CFG_RESTORE_REQ");
                cfg_restore_req(req, config_if_rx_size);
                break;

            case CMD_CFG_ERASE_REQ:
                DEBUG_PR_INFO("CFG_ERASE_REQ");
                cfg_erase_req(req, config_if_rx_size);
                break;

            case CMD_CFG_PROTECT_REQ:
                DEBUG_PR_INFO("CFG_PROTECT_REQ");
                cfg_protect_req(req, config_if_rx_size);
                break;

            case CMD_CFG_UNPROTECT_REQ:
                DEBUG_PR_INFO("CFG_UNPROTECT_REQ");
                cfg_unprotect_req(req, config_if_rx_size);
                break;

            case CMD_CFG_WRITE_CNF:
                DEBUG_PR_INFO("CFG_WRITE_CNF");
                cfg_write_cnf(req, config_if_rx_size);
                break;

            case CMD_GPS_WRITE_REQ:
                DEBUG_PR_INFO("GPS_WRITE_REQ");
                gps_write_req(req, config_if_rx_size);
                break;

            case CMD_GPS_READ_REQ:
                DEBUG_PR_INFO("GPS_READ_REQ");
                gps_read_req(req, config_if_rx_size);
                break;

            case CMD_GPS_CONFIG_REQ:
                DEBUG_PR_INFO("GPS_CONFIG_REQ");
                gps_config_req(req, config_if_rx_size);
                break;

            case CMD_BLE_CONFIG_REQ:
                DEBUG_PR_INFO("BLE_CONFIG_REQ");
                ble_config_req(req, config_if_rx_size);
                break;

            case CMD_BLE_WRITE_REQ:
                DEBUG_PR_INFO("BLE_WRITE_REQ");
                ble_write_req(req, config_if_rx_size);
                break;

            case CMD_BLE_READ_REQ:
                DEBUG_PR_INFO("BLE_READ_REQ");
                ble_read_req(req, config_if_rx_size);
                break;

            case CMD_STATUS_REQ:
                DEBUG_PR_INFO("STATUS_REQ");
                status_req(req, config_if_rx_size);
                break;

            case CMD_FW_SEND_IMAGE_REQ:
                DEBUG_PR_INFO("FW_SEND_IMAGE_REQ");
                fw_send_image_req(req, config_if_rx_size);
                break;

            case CMD_FW_APPLY_IMAGE_REQ:
                DEBUG_PR_INFO("FW_APPLY_IMAGE_REQ");
                fw_apply_image_req(req, config_if_rx_size);
                break;

            case CMD_RESET_REQ:
                DEBUG_PR_INFO("RESET_REQ");
                reset_req(req, config_if_rx_size);
                break;

            case CMD_BATTERY_STATUS_REQ:
                DEBUG_PR_INFO("BATTERY_STATUS_REQ");
                battery_status_req(req, config_if_rx_size);
                break;

            case CMD_LOG_CREATE_REQ:
                DEBUG_PR_INFO("LOG_CREATE_REQ");
                log_create_req(req, config_if_rx_size);
                break;

            case CMD_LOG_ERASE_REQ:
                DEBUG_PR_INFO("LOG_ERASE_REQ");
                log_erase_req(req, config_if_rx_size);
                break;

            case CMD_LOG_READ_REQ:
                DEBUG_PR_INFO("LOG_READ_REQ");
                log_read_req(req, config_if_rx_size);
                break;

            default:
                DEBUG_PR_WARN("Unhandled command: id %d", req->h.cmd);
                // Don't return an error. Fail silent
                break;
        }

        config_if_rx_pending = false;
    }

}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////// STATE EXECUTION CODE /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void boot_state(void)
{
    // Initialize all configured peripherals
    syshal_gpio_init(GPIO_LED3);
    syshal_gpio_init(GPIO_LED4);
    syshal_gpio_init(GPIO_LED5);
    syshal_gpio_init(GPIO_LED6);
    syshal_uart_init(UART_1);
    //syshal_uart_init(UART_3);
    //syshal_uart_init(UART_4);
    syshal_spi_init(SPI_1);
    syshal_spi_init(SPI_2);
    syshal_i2c_init(I2C_1);
    syshal_i2c_init(I2C_2);
    //syshal_batt_init(I2C_1);
    //syshal_gps_init();
    //syshal_usb_init();

    config_if_init(CONFIG_IF_BACKEND_USB);

    // Print General System Info
    DEBUG_PR_SYS("Arribada Tracker Device");
    DEBUG_PR_SYS("Version:  %s", GIT_VERSION);
    DEBUG_PR_SYS("Compiled: %s %s With %s", COMPILE_DATE, COMPILE_TIME, COMPILER_NAME);

    syshal_gpio_set_output_high(GPIO_LED3); // Indicate boot passed

    // If the battery is charging then the system shall transition to the BATTERY_CHARGING state
    //if (syshal_batt_charging())
    //    sm_set_state(SM_STATE_STANDBY_BATTERY_CHARGING);

    // If the battery level is too low then the system shall transition to the BATTERY_LEVEL_LOW state
    //if (syshal_batt_state() == POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL)
    //    sm_set_state(SM_STATE_STANDBY_BATTERY_LEVEL_LOW);

    // If either the USB 5V input signal or the BLE reed switch are active and the battery level is sufficient then it shall be possible to transition directly to the PROVISIONING state.
    //if (syshal_gpio_get_input(GPIO_USB_DETECT) || syshal_gpio_get_input(GPIO_BLE_REED))
    //    sm_set_state(SM_STATE_PROVISIONING);

    // Otherwise, if no configuration file is present or the current configuration is invalid then the system shall transition to the PROVISIONING_NEEDED sub-state.
    sm_set_state(SM_STATE_STANDBY_PROVISIONING_NEEDED);

    //sm_set_state(SM_STATE_OPERATIONAL);
}

void standby_battery_charging_state()
{

}

void standby_battery_level_low_state()
{

}

void standby_log_file_full_state()
{

}

void standby_provisioning_needed_state()
{
// This sub-state is entered if the currently active configuration in RAM (upon being read from flash) is invalid, incomplete or has no sensors enabled.
// The system shall continue to monitor the battery level and charging status and may enter into the BATTERY_LEVEL_LOW or BATTERY_CHARGING sub-states.
// The system shall also monitor the USB 5V input signal and the BLE reed switch and if the battery level is sufficient it shall be possible to transition to the PROVISIONING state.

    // Blink an LED to indicate this state
    static uint32_t blinkTimer = 0;
    const uint32_t blinkTimeMs = 1000;

    if (syshal_time_get_ticks_ms() >= (blinkTimeMs + blinkTimer))
    {
        syshal_gpio_set_output_high(GPIO_LED4);
        syshal_time_delay_ms(50);
        syshal_gpio_set_output_low(GPIO_LED4);
        blinkTimer = syshal_time_get_ticks_ms();
    }

    // Queue a receive request
    config_if_receive(rx_buffer, sizeof(rx_buffer));

    handle_config_if_requests();

    //parse_rx_buffer();

    // If the battery is charging then the system shall transition to the BATTERY_CHARGING state
    //if (syshal_batt_charging())
    //    sm_set_state(SM_STATE_STANDBY_BATTERY_CHARGING);

    // If the battery level is too low then the system shall transition to the BATTERY_LEVEL_LOW state
    //if (syshal_batt_state() == POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL)
    //    sm_set_state(SM_STATE_STANDBY_BATTERY_LEVEL_LOW);
}

void standby_trigger_pending_state()
{

}

void provisioning_state(void)
{

}

void operational_state(void)
{
//    if (syshal_usb_available())
//    {
//        DEBUG_PR_INFO("USB message received");
//        uint8_t buffer[100];
//        uint32_t size = syshal_usb_receive(buffer, sizeof(buffer));
//        syshal_usb_transfer(buffer, size);
//    }
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// STATE HANDLERS ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void exception_handler(CEXCEPTION_T e)
{

    switch (e)
    {
        case EXCEPTION_REQ_WRONG_SIZE:
            DEBUG_PR_ERROR("EXCEPTION_REQ_WRONG_SIZE");
            config_if_rx_pending = false; // This req message is erroneous so ignore it
            break;

        case EXCEPTION_RESP_TX_PENDING:
            // We're already transmitted a response
            // This is thrown to avoid overwritting the TX buffer before it is sent
            DEBUG_PR_ERROR("EXCEPTION_RESP_TX_PENDING");
            break;

        default:
            DEBUG_PR_ERROR("Unknown exception");
            break;
    }

}

sm_state_t sm_get_state(void)
{
    return state;
}

void sm_set_state(sm_state_t s)
{
    static sm_state_t pastState = SM_STATE_BOOT;
    state = s;
    if (pastState != s)
        DEBUG_PR_INFO("Switching state to: %s from: %s", sm_state_str[s], sm_state_str[pastState]);
    pastState = s;
}

void sm_iterate(void)
{
    CEXCEPTION_T e = CEXCEPTION_NONE;

    Try
    {
        switch (state)
        {
            case SM_STATE_BOOT:
                boot_state();
                break;

            case SM_STATE_STANDBY_BATTERY_CHARGING:
                standby_battery_charging_state();
                break;
            case SM_STATE_STANDBY_BATTERY_LEVEL_LOW:
                standby_battery_level_low_state();
                break;
            case SM_STATE_STANDBY_LOG_FILE_FULL:
                standby_log_file_full_state();
                break;
            case SM_STATE_STANDBY_PROVISIONING_NEEDED:
                standby_provisioning_needed_state();
                break;
            case SM_STATE_STANDBY_TRIGGER_PENDING:
                standby_trigger_pending_state();
                break;

            case SM_STATE_PROVISIONING:
                provisioning_state();
                break;

            case SM_STATE_OPERATIONAL:
                operational_state();
                break;

            default:
                // TODO: add an illegal error state here for catching
                // invalid state changes
                break;
        }
    } Catch (e)
    {
        exception_handler(e);
    }
}