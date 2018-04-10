// Fs.cpp - Filesystem unit tests
//
// Copyright (C) 2018 Arribada
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
extern "C" {
#include "unity.h"
#include <assert.h>
#include <stdint.h>
#include "Mocksyshal_batt.h"
#include "Mocksyshal_gpio.h"
#include "Mocksyshal_uart.h"
#include "Mocksyshal_spi.h"
#include "Mocksyshal_i2c.h"
#include "Mocksyshal_time.h"
#include "Mockconfig_if.h"
#include "Mockfs.h"
#include "cmd.h"
#include "sys_config.h"
#include "sm.h"
#include <stdlib.h>
}

#include "googletest.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <queue>

#define FS_FILE_ID_CONF             (0) // The File ID of the configuration data
#define FS_FILE_ID_STM32_IMAGE      (1) // STM32 application image
#define FS_FILE_ID_BLE_APP_IMAGE    (2) // BLE application image
#define FS_FILE_ID_BLE_SOFT_IMAGE   (3) // BLE soft-device image
#define FS_FILE_ID_LOG              (4) // Sensor log file

// Dummy functions, used to ignore all calls to this function
void syshal_batt_init_dummy(uint32_t instance, int cmock_num_calls) {}
void syshal_gpio_init_dummy(uint32_t pin, int cmock_num_calls) {}
int syshal_uart_init_dummy(uint32_t instance, int cmock_num_calls) {return SYSHAL_UART_NO_ERROR;}
int syshal_spi_init_dummy(uint32_t instance, int cmock_num_calls) {return SYSHAL_SPI_NO_ERROR;}
int syshal_i2c_init_dummy(uint32_t instance, int cmock_num_calls) {return SYSHAL_I2C_NO_ERROR;}
int config_if_init_dummy(config_if_backend_t backend, int cmock_num_calls) {return CONFIG_IF_NO_ERROR;}
int fs_init_dummy(uint32_t device, int cmock_num_calls) {return FS_NO_ERROR;}
int fs_mount_dummy(uint32_t device, fs_t * fs, int cmock_num_calls) {return FS_NO_ERROR;}

/////// File system handlers ///////
bool file_currently_open = false;

// fs_open callback function
std::queue<uint8_t> fs_open_expected_file_id;
std::queue<fs_mode_t> fs_open_expected_mode;
std::queue<int> fs_open_return_value;
int fs_open_callback(fs_t fs, fs_handle_t * handle, uint8_t file_id, fs_mode_t mode, uint8_t * user_flags, int cmock_num_calls)
{
    EXPECT_EQ(fs_open_expected_file_id.front(), file_id);
    fs_open_expected_file_id.pop();
    EXPECT_EQ(fs_open_expected_mode.front(), mode);
    fs_open_expected_mode.pop();

    int ret_val = fs_open_return_value.front();
    fs_open_return_value.pop();

    if (FS_NO_ERROR == ret_val)
        file_currently_open = true;

    return ret_val;
}

// fs_read callback function
int fs_read_return_value;
int fs_read_callback(fs_handle_t handle, void * dest, uint32_t size, uint32_t * read, int cmock_num_calls)
{
    *read = size;
    return fs_read_return_value;
}

// fs_write callback function
int fs_write_return_value;
int fs_write_callback(fs_handle_t handle, const void * src, uint32_t size, uint32_t * written, int cmock_num_calls)
{
    *written = size;
    return fs_write_return_value;
}

// fs_close callback function
int fs_close_callback(fs_handle_t handle, int cmock_num_calls)
{
    file_currently_open = false;
}

// syshal_time_get_ticks_ms callback function
uint32_t syshal_time_get_ticks_ms_value;
uint32_t syshal_time_get_ticks_ms_callback(int cmock_num_calls)
{
    return syshal_time_get_ticks_ms_value;
}

// config_if_receive callback function
uint8_t * config_if_receive_buffer = NULL;
int config_if_receive_callback(uint8_t * data, uint32_t size, int cmock_num_calls)
{
    config_if_receive_buffer = data;
    return CONFIG_IF_NO_ERROR;
}

// config_if_send callback function
uint8_t * config_if_send_buffer = NULL;
uint32_t config_if_send_size;
int config_if_send_callback(uint8_t * data, uint32_t size, int cmock_num_calls)
{
    config_if_send_buffer = data;
    config_if_send_size = size;
    return CONFIG_IF_NO_ERROR;
}

class SmTest : public ::testing::Test
{

    virtual void SetUp()
    {
        Mocksyshal_batt_Init();
        Mocksyshal_gpio_Init();
        Mocksyshal_uart_Init();
        Mocksyshal_spi_Init();
        Mocksyshal_i2c_Init();
        Mockconfig_if_Init();
        Mockfs_Init();

        // Callbacks
        fs_open_StubWithCallback(fs_open_callback);
        fs_read_StubWithCallback(fs_read_callback);
        fs_write_StubWithCallback(fs_write_callback);
        fs_close_StubWithCallback(fs_close_callback);
        syshal_time_get_ticks_ms_StubWithCallback(syshal_time_get_ticks_ms_callback);
        config_if_receive_StubWithCallback(config_if_receive_callback);
        config_if_send_StubWithCallback(config_if_send_callback);
    }

    virtual void TearDown()
    {
        // Empty queues
        while (!fs_open_expected_file_id.empty()) fs_open_expected_file_id.pop();
        while (!fs_open_expected_mode.empty()) fs_open_expected_mode.pop();
        while (!fs_open_return_value.empty()) fs_open_return_value.pop();

        // Clear all configuration tags
        uint16_t last_index = 0;
        uint16_t tag;
        while (!sys_config_iterate(&tag, &last_index))
        {
            sys_config_unset(tag);
        }

        // Reset global variables to defaults
        config_if_receive_buffer = NULL;
        config_if_send_buffer = NULL;
        config_if_send_size = 0;

        // Cleanup Mocks
        Mocksyshal_batt_Verify();
        Mocksyshal_batt_Destroy();
        Mocksyshal_gpio_Verify();
        Mocksyshal_gpio_Destroy();
        Mocksyshal_uart_Verify();
        Mocksyshal_uart_Destroy();
        Mocksyshal_spi_Verify();
        Mocksyshal_spi_Destroy();
        Mocksyshal_i2c_Verify();
        Mocksyshal_i2c_Destroy();
        Mockconfig_if_Verify();
        Mockconfig_if_Destroy();
        Mockfs_Verify();
        Mockfs_Destroy();
    }

public:

    // Functions
    void HardwareInit()
    {
        // Ignore all calls to these functions
        syshal_batt_init_StubWithCallback(syshal_batt_init_dummy);
        syshal_gpio_init_StubWithCallback(syshal_gpio_init_dummy);
        syshal_uart_init_StubWithCallback(syshal_uart_init_dummy);
        syshal_spi_init_StubWithCallback(syshal_spi_init_dummy);
        syshal_i2c_init_StubWithCallback(syshal_i2c_init_dummy);
        config_if_init_StubWithCallback(config_if_init_dummy);
        fs_init_StubWithCallback(fs_init_dummy);
        fs_mount_StubWithCallback(fs_mount_dummy);
    }

    // A call to fs_get_configuration_data where the configuration data file is non-existant
    void fs_get_configuration_data_no_file()
    {
        // fs_open
        fs_open_expected_file_id.push(FS_FILE_ID_CONF);
        fs_open_expected_mode.push(FS_MODE_READONLY);
        fs_open_return_value.push(FS_ERROR_FILE_NOT_FOUND);
    }

    // A call to fs_get_configuration_data where the configuration data is correctly read
    void fs_get_configuration_data_success()
    {
        // fs_open
        fs_open_expected_file_id.push(FS_FILE_ID_CONF);
        fs_open_expected_mode.push(FS_MODE_READONLY);
        fs_open_return_value.push(FS_NO_ERROR);

        // fs_read
        fs_read_return_value = FS_NO_ERROR;
    }

    // A call to fs_create_configuration_data where the configuration data is correctly made
    void fs_create_configuration_data_success()
    {
        // fs_open
        fs_open_expected_file_id.push(FS_FILE_ID_CONF);
        fs_open_expected_mode.push(FS_MODE_CREATE);
        fs_open_return_value.push(FS_NO_ERROR);

        // fs_write
        fs_write_return_value = FS_NO_ERROR;
    }

    void set_all_configuration_tags()
    {
        uint8_t empty_buffer[SYS_CONFIG_MAX_DATA_SIZE];

        sys_config_set(SYS_CONFIG_TAG_GPS_LOG_POSITION_ENABLE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_gps_log_position_enable_t));
        sys_config_set(SYS_CONFIG_TAG_GPS_LOG_TTFF_ENABLE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_gps_log_ttff_enable_t));
        sys_config_set(SYS_CONFIG_TAG_GPS_TRIGGER_MODE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_gps_trigger_mode_t));
        sys_config_set(SYS_CONFIG_TAG_GPS_UART_BAUD_RATE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_gps_uart_baud_rate_t));
        sys_config_set(SYS_CONFIG_TAG_RTC_SYNC_TO_GPS_ENABLE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_rtc_sync_to_gps_enable_t));
        sys_config_set(SYS_CONFIG_TAG_RTC_CURRENT_DATE_AND_TIME, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_rtc_current_date_and_time_t));
        sys_config_set(SYS_CONFIG_TAG_LOGGING_ENABLE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_logging_enable_t));
        sys_config_set(SYS_CONFIG_TAG_LOGGING_BYTES_WRITTEN, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_logging_bytes_written_t));
        sys_config_set(SYS_CONFIG_TAG_LOGGING_FILE_SIZE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_logging_file_size_t));
        sys_config_set(SYS_CONFIG_TAG_LOGGING_FILE_TYPE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_logging_file_type_t));
        sys_config_set(SYS_CONFIG_TAG_LOGGING_GROUP_SENSOR_READINGS_ENABLE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_logging_group_sensor_readings_enable_t));
        sys_config_set(SYS_CONFIG_TAG_LOGGING_START_END_SYNC_ENABLE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_logging_start_end_sync_enable_t));
        sys_config_set(SYS_CONFIG_TAG_LOGGING_DATE_TIME_STAMP_ENABLE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_logging_date_time_stamp_enable_t));
        sys_config_set(SYS_CONFIG_TAG_LOGGING_HIGH_RESOLUTION_TIMER_ENABLE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_logging_high_resolution_timer_enable_t));
        sys_config_set(SYS_CONFIG_TAG_AXL_LOG_ENABLE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_axl_log_enable_t));
        sys_config_set(SYS_CONFIG_TAG_AXL_CONFIG, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_axl_config_t));
        sys_config_set(SYS_CONFIG_TAG_AXL_G_FORCE_HIGH_THRESHOLD, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_axl_g_force_high_threshold_t));
        sys_config_set(SYS_CONFIG_TAG_AXL_SAMPLE_RATE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_axl_sample_rate_t));
        sys_config_set(SYS_CONFIG_TAG_AXL_MODE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_axl_mode_t));
        sys_config_set(SYS_CONFIG_TAG_PRESSURE_SENSOR_LOG_ENABLE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_pressure_sensor_log_enable_t));
        sys_config_set(SYS_CONFIG_TAG_PRESSURE_SAMPLE_RATE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_pressure_sample_rate_t));
        sys_config_set(SYS_CONFIG_TAG_PRESSURE_LOW_THRESHOLD, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_pressure_low_threshold_t));
        sys_config_set(SYS_CONFIG_TAG_PRESSURE_HIGH_THRESHOLD, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_pressure_high_threshold_t));
        sys_config_set(SYS_CONFIG_TAG_PRESSURE_MODE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_pressure_mode_t));
        sys_config_set(SYS_CONFIG_TAG_TEMP_SENSOR_LOG_ENABLE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_temp_sensor_log_enable_t));
        sys_config_set(SYS_CONFIG_TAG_TEMP_SENSOR_SAMPLE_RATE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_temp_sensor_sample_rate_t));
        sys_config_set(SYS_CONFIG_TAG_TEMP_SENSOR_LOW_THRESHOLD, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_temp_sensor_low_threshold_t));
        sys_config_set(SYS_CONFIG_TAG_TEMP_SENSOR_HIGH_THRESHOLD, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_temp_sensor_high_threshold_t));
        sys_config_set(SYS_CONFIG_TAG_TEMP_SENSOR_MODE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_temp_sensor_mode_t));
        sys_config_set(SYS_CONFIG_TAG_SYSTEM_DEVICE_IDENTIFIER, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_system_device_identifier_t));
        sys_config_set(SYS_CONFIG_TAG_BLUETOOTH_UUID, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_bluetooth_uuid_t));
        sys_config_set(SYS_CONFIG_TAG_BLUETOOTH_BEACON_ENABLE, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_bluetooth_beacon_enable_t));
        sys_config_set(SYS_CONFIG_TAG_BLUETOOTH_BEACON_GEO_FENCE_TRIGGER_LOCATION, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_bluetooth_beacon_geo_fence_trigger_location_t));
        sys_config_set(SYS_CONFIG_TAG_BLUETOOTH_BEACON_ADVERTISING_INTERVAL, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_bluetooth_beacon_advertising_interval_t));
        sys_config_set(SYS_CONFIG_TAG_BLUETOOTH_BEACON_ADVERTISING_CONFIGURATION, &empty_buffer, SYS_CONFIG_TAG_DATA_SIZE(sys_config_bluetooth_beacon_advertising_configuration_t));
    }

};

TEST_F(SmTest, StateSet)
{
    sm_set_state(SM_STATE_BOOT);
    EXPECT_EQ(SM_STATE_BOOT, sm_get_state());
}

TEST_F(SmTest, BootConfigurationDataDoesNotExist)
{
    sm_set_state(SM_STATE_BOOT);
    HardwareInit();

    fs_get_configuration_data_no_file(); // No configuration file present
    fs_create_configuration_data_success(); // Successfully create a file

    syshal_batt_charging_ExpectAndReturn(false);
    syshal_batt_state_ExpectAndReturn(POWER_SUPPLY_CAPACITY_LEVEL_FULL);

    syshal_gpio_set_output_high_Expect(GPIO_LED3); // Status LED

    sm_iterate();

    EXPECT_EQ(SM_STATE_STANDBY_PROVISIONING_NEEDED, sm_get_state());
}

TEST_F(SmTest, BootConfigurationDataExistsButIncomplete)
{
    sm_set_state(SM_STATE_BOOT);
    HardwareInit();

    fs_get_configuration_data_success(); // Successful read of a configuration file

    syshal_batt_charging_ExpectAndReturn(false);
    syshal_batt_state_ExpectAndReturn(POWER_SUPPLY_CAPACITY_LEVEL_FULL);

    syshal_gpio_set_output_high_Expect(GPIO_LED3); // Status LED

    sm_iterate();

    EXPECT_EQ(SM_STATE_STANDBY_PROVISIONING_NEEDED, sm_get_state());
}

TEST_F(SmTest, BootBatteryCharging)
{
    sm_set_state(SM_STATE_BOOT);
    HardwareInit();

    fs_get_configuration_data_success(); // Successful read of a configuration file

    syshal_batt_charging_ExpectAndReturn(true);

    sm_iterate();

    EXPECT_EQ(SM_STATE_STANDBY_BATTERY_CHARGING, sm_get_state());
}

TEST_F(SmTest, BootBatteryLevelLow)
{
    sm_set_state(SM_STATE_BOOT);
    HardwareInit();

    fs_get_configuration_data_success(); // Successful read of a configuration file

    syshal_batt_charging_ExpectAndReturn(false);
    syshal_batt_state_ExpectAndReturn(POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL);

    sm_iterate();

    EXPECT_EQ(SM_STATE_STANDBY_BATTERY_LEVEL_LOW, sm_get_state());
}

TEST_F(SmTest, BootConfigurationComplete)
{
    sm_set_state(SM_STATE_BOOT);
    HardwareInit();

    set_all_configuration_tags(); // Set all configuration tags

    fs_get_configuration_data_success(); // Successful read of a configuration file

    syshal_batt_charging_ExpectAndReturn(false);
    syshal_batt_state_ExpectAndReturn(POWER_SUPPLY_CAPACITY_LEVEL_FULL);

    syshal_gpio_set_output_high_Expect(GPIO_LED3); // Status LED

    sm_iterate();

    EXPECT_EQ(SM_STATE_OPERATIONAL, sm_get_state());
}

TEST_F(SmTest, ProvisioningNeededState)
{
    sm_set_state(SM_STATE_BOOT);
    HardwareInit();

    fs_get_configuration_data_success(); // Successful read of a configuration file

    syshal_batt_charging_ExpectAndReturn(false);
    syshal_batt_state_ExpectAndReturn(POWER_SUPPLY_CAPACITY_LEVEL_FULL);

    syshal_gpio_set_output_high_Expect(GPIO_LED3); // Status LED

    sm_iterate();

    EXPECT_EQ(SM_STATE_STANDBY_PROVISIONING_NEEDED, sm_get_state());

    // Generate an configuration interface connection event
    config_if_event_t event;
    event.id = CONFIG_IF_EVENT_CONNECTED;

    config_if_event_handler(&event);

    sm_iterate();

    EXPECT_EQ(SM_STATE_PROVISIONING, sm_get_state());
}

TEST_F(SmTest, ProvisioningDisconnect)
{
    sm_set_state(SM_STATE_BOOT);
    HardwareInit();

    fs_get_configuration_data_success(); // Successful read of a configuration file

    syshal_batt_charging_ExpectAndReturn(false);
    syshal_batt_state_ExpectAndReturn(POWER_SUPPLY_CAPACITY_LEVEL_FULL);

    syshal_gpio_set_output_high_Expect(GPIO_LED3); // Status LED

    sm_iterate();

    EXPECT_EQ(SM_STATE_STANDBY_PROVISIONING_NEEDED, sm_get_state());

    // Generate an configuration interface connection event
    config_if_event_t event;
    event.id = CONFIG_IF_EVENT_CONNECTED;

    config_if_event_handler(&event);

    sm_iterate();

    EXPECT_EQ(SM_STATE_PROVISIONING, sm_get_state());

    config_if_receive_IgnoreAndReturn(CONFIG_IF_NO_ERROR);

    // Generate a configuration interface disconnect event
    event.id = CONFIG_IF_EVENT_DISCONNECTED;
    config_if_event_handler(&event);

    sm_iterate();

    EXPECT_EQ(SM_STATE_STANDBY_PROVISIONING_NEEDED, sm_get_state());
}

TEST_F(SmTest, StatusRequest)
{
    sm_set_state(SM_STATE_BOOT);
    HardwareInit();

    fs_get_configuration_data_success(); // Successful read of a configuration file

    syshal_batt_charging_ExpectAndReturn(false);
    syshal_batt_state_ExpectAndReturn(POWER_SUPPLY_CAPACITY_LEVEL_FULL);

    syshal_gpio_set_output_high_Expect(GPIO_LED3); // Status LED

    sm_iterate();

    EXPECT_EQ(SM_STATE_STANDBY_PROVISIONING_NEEDED, sm_get_state());

    // Generate an configuration interface connection event
    config_if_event_t event;
    event.id = CONFIG_IF_EVENT_CONNECTED;
    config_if_event_handler(&event);

    sm_iterate();

    EXPECT_EQ(SM_STATE_PROVISIONING, sm_get_state());

    sm_iterate(); // Queue the first receive

    // Generate reset request message
    cmd_t * req = (cmd_t *) config_if_receive_buffer;
    CMD_SET_HDR(req, CMD_STATUS_REQ);

    // Generate receive request
    event.id = CONFIG_IF_EVENT_RECEIVE_COMPLETE;
    event.receive.buffer = config_if_receive_buffer;
    event.receive.size = CMD_SIZE_HDR;
    config_if_event_handler(&event);

    sm_iterate(); // Process the message

    // Check the response
    cmd_t * resp = (cmd_t *) config_if_send_buffer;
    EXPECT_EQ(CMD_SYNCWORD, resp->h.sync);
    EXPECT_EQ(CMD_STATUS_RESP, resp->h.cmd);
    EXPECT_EQ(CMD_NO_ERROR, resp->p.cmd_status_resp.error_code);
    EXPECT_EQ(0, resp->p.cmd_status_resp.stm_firmware_version);
    EXPECT_EQ(0, resp->p.cmd_status_resp.ble_firmware_version);
    EXPECT_EQ(0, resp->p.cmd_status_resp.configuration_format_version);
}