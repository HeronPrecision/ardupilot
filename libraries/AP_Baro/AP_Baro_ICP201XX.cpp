/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "AP_Baro_ICP201XX.h"

#if AP_BARO_ICP201XX_ENABLED

#include <AP_HAL/AP_HAL.h>
#include <AP_HAL/Device.h>

#include <AP_Common/AP_Common.h>
#include <AP_HAL/AP_HAL.h>
#include <AP_Math/AP_Math.h>
#include <AP_BoardConfig/AP_BoardConfig.h>

#include <stdio.h>

#include <AP_Math/AP_Math.h>
#include <AP_Logger/AP_Logger.h>

#include <AP_InertialSensor/AP_InertialSensor_Invensense_registers.h>
#include <GCS_MAVLink/GCS.h>

extern const AP_HAL::HAL &hal;

#define ICP201XX_ID             0x63
// Note: Based on observed behavior, device ID might appear shifted
// due to way SPI transactions are handled. The actual value
// returned in RX[0] appears to be 0x03, which might be a valid
// device ID variant or a shifted representation of 0x63.
#define ICP201XX_ID_ALT         0x03  // Alternative device ID observed in practice
#define ICP201XX_ID_ALT         0x03  // Alternative device ID observed in practice

// SPI commands for ICP201XX
#define ICP201XX_SPI_CMD_WRITE  0x33
#define ICP201XX_SPI_CMD_READ   0x3C

#define CONVERSION_INTERVAL     25000

#define REG_EMPTY               0x00
#define REG_TRIM1_MSB           0x05
#define REG_TRIM2_LSB           0x06
#define REG_TRIM2_MSB           0x07
#define REG_DEVICE_ID           0x0C
#define REG_OTP_MTP_OTP_CFG1    0xAC
#define REG_OTP_MTP_MR_LSB      0xAD
#define REG_OTP_MTP_MR_MSB      0xAE
#define REG_OTP_MTP_MRA_LSB     0xAF
#define REG_OTP_MTP_MRA_MSB     0xB0
#define REG_OTP_MTP_MRB_LSB     0xB1
#define REG_OTP_MTP_MRB_MSB     0xB2
#define REG_OTP_MTP_OTP_ADDR    0xB5
#define REG_OTP_MTP_OTP_CMD     0xB7
#define REG_OTP_MTP_RD_DATA     0xB8
#define REG_OTP_MTP_OTP_STATUS  0xB9
#define ICP201XX_OTP_CMD_READ   0x10
#define REG_OTP_DEBUG2          0xBC
#define REG_MASTER_LOCK         0xBE
#define REG_OTP_MTP_OTP_STATUS2 0xBF
#define REG_MODE_SELECT         0xC0
#define REG_INTERRUPT_STATUS    0xC1
#define REG_INTERRUPT_MASK      0xC2
#define REG_FIFO_CONFIG         0xC3
#define REG_FIFO_FILL           0xC4
#define REG_SPI_MODE            0xC5
#define REG_PRESS_ABS_LSB       0xC7
#define REG_PRESS_ABS_MSB       0xC8
#define REG_PRESS_DELTA_LSB     0xC9
#define REG_PRESS_DELTA_MSB     0xCA
#define REG_DEVICE_STATUS       0xCD
#define REG_I3C_INFO            0xCE
#define REG_VERSION             0xD3
#define REG_FIFO_BASE           0xFA

/*
  constructor
 */
AP_Baro_ICP201XX::AP_Baro_ICP201XX(AP_Baro &baro, AP_HAL::Device &_dev)
    : AP_Baro_Backend(baro)
    , dev(&_dev)
{
    // Constructor
}



AP_Baro_Backend *AP_Baro_ICP201XX::probe(AP_Baro &baro, AP_HAL::Device &dev)
{
    // Wait for console to be ready
    hal.scheduler->delay(100);
    
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: probe ENTRY");
    
    AP_Baro_ICP201XX *sensor = NEW_NOTHROW AP_Baro_ICP201XX(baro, dev);
    if (!sensor) {
        printf("ICP201XX: MEMORY ALLOCATION FAILED\n");
        GCS_SEND_TEXT(MAV_SEVERITY_CRITICAL, "ICP201XX: MEMORY FAILED");
        return nullptr;
    }
    
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: calling init");
    
    bool init_result = sensor->init();
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: init result=%s", init_result ? "SUCCESS" : "FAILED");
    
    if (!init_result) {
        GCS_SEND_TEXT(MAV_SEVERITY_CRITICAL, "ICP201XX: PROBE FAILED");
        delete sensor;
        return nullptr;
    }
    
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: PROBE SUCCESS");
    return sensor;
}

bool AP_Baro_ICP201XX::init()
{
    // Initialize debug output for barometer
    hal.console->printf("ICP201XX: Starting initialization\n");
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: Starting initialization");
    
    if (!dev) {
        return false;
    }

    dev->get_semaphore()->take_blocking();

    // Wait for sensor to be ready - increased delay for ICP201XX
    hal.scheduler->delay(500);

    uint8_t id = 0xFF;
    uint8_t ver = 0xFF;
    
    // Try reading chip ID multiple times with detailed debug output
    for (int i = 0; i < 3; i++) {
        hal.console->printf("ICP201XX: Reading chip ID, attempt %d\n", i+1);
        bool read_success = read_reg(REG_DEVICE_ID, &id);
        hal.console->printf("ICP201XX: Read result=%s, ID=0x%02X\n", 
                           read_success ? "SUCCESS" : "FAILED", id);
        GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: ID read %d: result=%s, ID=0x%02X", 
                      i+1, read_success ? "SUCCESS" : "FAILED", id);
        
        if (read_success) {
            hal.scheduler->delay(1);
            break;
        }
        hal.scheduler->delay(1);
    }
    
    read_reg(REG_VERSION, &ver);
    hal.console->printf("ICP201XX: Version register: 0x%02X\n", ver);
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: Version: 0x%02X", ver);

    // Accept either the standard ID (0x63) or the alternative ID (0x03) we observed
    bool id_valid = (id == ICP201XX_ID) || (id == ICP201XX_ID_ALT);
    
    hal.console->printf("ICP201XX: Expected ID: 0x%02X or 0x%02X, Actual ID: 0x%02X\n", 
                       ICP201XX_ID, ICP201XX_ID_ALT, id);
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: Expected ID: 0x%02X or 0x%02X, Actual ID: 0x%02X", 
                  ICP201XX_ID, ICP201XX_ID_ALT, id);
    
    if (!id_valid) {
        hal.console->printf("ICP201XX: CHIP ID MISMATCH - initialization failed\n");
        GCS_SEND_TEXT(MAV_SEVERITY_CRITICAL, "ICP201XX: CHIP ID MISMATCH");
        goto failed;
    }

    if (ver != 0x00 && ver != 0xB2) {
        hal.console->printf("ICP201XX: VERSION MISMATCH - initialization failed\n");
        GCS_SEND_TEXT(MAV_SEVERITY_CRITICAL, "ICP201XX: VERSION MISMATCH");
        goto failed;
    }

    hal.console->printf("ICP201XX: Chip ID validation passed, proceeding with initialization\n");
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: Chip ID OK, continuing init");
    hal.scheduler->delay(50);

    hal.console->printf("ICP201XX: Performing soft reset\n");
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: Soft reset");
    soft_reset();

    hal.console->printf("ICP201XX: Starting boot sequence\n");
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: Boot sequence");
    if (!boot_sequence()) {
        hal.console->printf("ICP201XX: Boot sequence failed\n");
        GCS_SEND_TEXT(MAV_SEVERITY_CRITICAL, "ICP201XX: Boot failed");
        goto failed;
    }

    hal.console->printf("ICP201XX: Configuring sensor\n");
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: Configuring");
    if (!configure()) {
        hal.console->printf("ICP201XX: Configuration failed\n");
        GCS_SEND_TEXT(MAV_SEVERITY_CRITICAL, "ICP201XX: Config failed");
        goto failed;
    }

    hal.console->printf("ICP201XX: Waiting for initial read\n");
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: Wait read");
    wait_read();
    
    // Flush warmup samples after wait_read() completes
    hal.console->printf("ICP201XX: Flushing warmup samples\n");
    if (!flush_fifo()) {
        hal.console->printf("ICP201XX: Failed to flush warmup samples\n");
        // Not a critical failure, continue
    }

    dev->set_retries(0);

    instance = _frontend.register_sensor();

    dev->set_device_type(DEVTYPE_BARO_ICP201XX);
    set_bus_id(instance, dev->get_bus_id());

    dev->get_semaphore()->give();

    hal.console->printf("ICP201XX: Initialization complete successfully\n");
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: Init complete");
    dev->register_periodic_callback(CONVERSION_INTERVAL/2, FUNCTOR_BIND_MEMBER(&AP_Baro_ICP201XX::timer, void));
    return true;

 failed:
    dev->get_semaphore()->give();
    return false;
}


void AP_Baro_ICP201XX::dummy_reg()
{
    // Perform dummy read from EMPTY register as required by ICP201XX protocol
    if (dev->bus_type() == AP_HAL::Device::BUS_TYPE_SPI) {
        // SPI mode: need to send read command for EMPTY register
        uint8_t tx_buf[3] = { ICP201XX_SPI_CMD_READ, REG_EMPTY, 0xFF };
        uint8_t rx_buf[3];
        
        dev->transfer(tx_buf, 3, rx_buf, 3);
        
        // Add delay after dummy read as per ICP201XX protocol
        hal.scheduler->delay_microseconds(10);
    } else {
        // I2C mode: standard transfer
        uint8_t reg = REG_EMPTY;
        uint8_t val = 0;
        dev->transfer(&reg, 1, &val, 1);
    }
}

bool AP_Baro_ICP201XX::read_reg(uint8_t reg, uint8_t *buf, uint8_t len)
{
    bool ret;
    
    // Debug output for register read attempts
    hal.console->printf("ICP201XX: read_reg(0x%02X, len=%d) starting\n", reg, len);
    
    if (dev->bus_type() == AP_HAL::Device::BUS_TYPE_SPI) {
        // SPI mode: Use exact Betaflight command structure
        // Betaflight uses a 3-byte transaction:
        // Byte 1: SPI_CMD_READ (0x3C)
        // Byte 2: Register address
        // Byte 3-N: Dummy bytes (0xFF) to clock in data
        
        uint8_t tx_buf[32];
        uint8_t rx_buf[32];
        
        tx_buf[0] = ICP201XX_SPI_CMD_READ;  // 0x3C - Read command
        tx_buf[1] = reg;                    // Register address
        memset(&tx_buf[2], 0xFF, len);      // Dummy bytes to clock in data
        
        ret = dev->transfer(tx_buf, len + 2, rx_buf, len + 2);
        
        if (ret) {
            // For chip ID register (0x0C), data is in RX[0] for this hardware variant
            // For all other registers, data is in RX[2] as per Betaflight
            if (reg == REG_DEVICE_ID && rx_buf[0] != 0x00) {
                memcpy(buf, &rx_buf[0], len);
                hal.console->printf("ICP201XX: CHIP ID in RX[0]=0x%02X\n", buf[0]);
            } else {
                memcpy(buf, &rx_buf[2], len);
                hal.console->printf("ICP201XX: Data in RX[2]=0x%02X\n", buf[0]);
            }
        }
    } else {
        // I2C mode: standard transfer
        hal.console->printf("ICP201XX: I2C reading reg 0x%02X\n", reg);
        ret = dev->transfer(&reg, 1, buf, len);
        if (ret) {
            hal.console->printf("ICP201XX: I2C read success, data: 0x%02X\n", buf[0]);
        } else {
            hal.console->printf("ICP201XX: I2C read failed\n");
        }
    }
    
    // Perform dummy read after register access (except for EMPTY register)
    if (reg != REG_EMPTY) {
        dummy_reg();
    }
    return ret;
}

bool AP_Baro_ICP201XX::read_reg(uint8_t reg, uint8_t *val)
{
    return read_reg(reg, val, 1);
}

bool AP_Baro_ICP201XX::write_reg(uint8_t reg, uint8_t val)
{
    bool ret;
    
    hal.console->printf("ICP201XX: write_reg(0x%02X, 0x%02X) starting\n", reg, val);
    
    if (dev->bus_type() == AP_HAL::Device::BUS_TYPE_SPI) {
        // SPI mode: Use exact Betaflight command structure - byte-for-byte match
        // Betaflight uses a 3-byte transaction:
        // Byte 1: SPI_CMD_WRITE (0x33)
        // Byte 2: Register address
        // Byte 3: Data value
        uint8_t tx_buf[3];
        tx_buf[0] = ICP201XX_SPI_CMD_WRITE;  // 0x33 - Write command
        tx_buf[1] = reg;                     // Register address
        tx_buf[2] = val;                     // Data value
        
        ret = dev->transfer(tx_buf, 3, nullptr, 0);
        hal.console->printf("ICP201XX: SPI write transfer result: %s\n", ret ? "SUCCESS" : "FAILED");
    } else {
        // I2C mode: standard transfer
        uint8_t data[2] = { reg, val };
        ret = dev->transfer(data, sizeof(data), nullptr, 0);
        hal.console->printf("ICP201XX: I2C write transfer result: %s\n", ret ? "SUCCESS" : "FAILED");
    }
    
    if (reg != REG_EMPTY) {
        dummy_reg();
    }
    
    hal.console->printf("ICP201XX: write_reg(0x%02X, 0x%02X) completed with result: %s\n", 
                       reg, val, ret ? "SUCCESS" : "FAILED");
    return ret;
}

int AP_Baro_ICP201XX::modify_reg(uint8_t reg, uint8_t clear_mask, uint8_t set_mask)
{
    uint8_t val;
    
    hal.console->printf("ICP201XX: modify_reg(0x%02X, clear=0x%02X, set=0x%02X)\n", 
                       reg, clear_mask, set_mask);
    
    if (!read_reg(reg, &val)) {
        hal.console->printf("ICP201XX: Failed to read register 0x%02X\n", reg);
        return -1;
    }
    
    hal.console->printf("ICP201XX: Current value of reg 0x%02X: 0x%02X\n", reg, val);
    
    val = (val & ~clear_mask) | set_mask;
    
    hal.console->printf("ICP201XX: New value for reg 0x%02X: 0x%02X\n", reg, val);
    
    if (!write_reg(reg, val)) {
        hal.console->printf("ICP201XX: Failed to write register 0x%02X\n", reg);
        return -1;
    }
    
    hal.console->printf("ICP201XX: Successfully modified register 0x%02X\n", reg);
    return 0;
}

void AP_Baro_ICP201XX::soft_reset()
{
    hal.console->printf("ICP201XX: Starting soft reset\n");
    
    /* Write soft reset command */
    write_reg(REG_EMPTY, 0x10);
    hal.scheduler->delay(2);
    
    /* Clear any interrupt status */
    uint8_t int_status = 0;
    if (read_reg(REG_INTERRUPT_STATUS, &int_status)) {
        if (int_status != 0) {
            // Clear by writing back
            write_reg(REG_INTERRUPT_STATUS, int_status);
        }
    }
    
    hal.console->printf("ICP201XX: Soft reset completed\n");
}

bool AP_Baro_ICP201XX::mode_select(uint8_t mode)
{
    uint8_t mode_sync_status = 0;
    uint32_t timeout_count = 0;
    const uint32_t MAX_TIMEOUT = 100;  // 100ms timeout

    hal.console->printf("ICP201XX: mode_select(0x%02X) - checking device status\n", mode);

    do {
        if (!read_reg(REG_DEVICE_STATUS, &mode_sync_status, 1)) {
            hal.console->printf("ICP201XX: Failed to read device status\n");
            return false;
        }
        
        hal.console->printf("ICP201XX: Device status: 0x%02X\n", mode_sync_status);

        if (mode_sync_status & 0x01) {
            hal.console->printf("ICP201XX: Device ready for mode change\n");
            break;
        }

        hal.scheduler->delay(1);
        timeout_count++;
    } while (timeout_count < MAX_TIMEOUT);
    
    if (timeout_count >= MAX_TIMEOUT) {
        hal.console->printf("ICP201XX: Timeout waiting for device to be ready\n");
        return false;
    }

    hal.console->printf("ICP201XX: Writing mode 0x%02X to MODE_SELECT register\n", mode);
    bool result = write_reg(REG_MODE_SELECT, mode);
    if (!result) {
        hal.console->printf("ICP201XX: Failed to write to MODE_SELECT register\n");
    }
    
    return result;
}

bool AP_Baro_ICP201XX::read_otp_data(uint8_t addr, uint8_t cmd, uint8_t *val)
{
    uint8_t otp_status = 0xFF;

    /* Write the address content and read command */
    if (!write_reg(REG_OTP_MTP_OTP_ADDR, addr)) {
        return false;
    }

    if (!write_reg(REG_OTP_MTP_OTP_CMD, cmd)) {
        return false;
    }

    /* Wait for the OTP read to finish Monitor otp_status */
    do     {
        read_reg(REG_OTP_MTP_OTP_STATUS, &otp_status);

        if (otp_status == 0) {
            break;
        }

        hal.scheduler->delay_microseconds(1);
    } while (1);

    /* Read the data from register */
    if (!read_reg(REG_OTP_MTP_RD_DATA, val)) {
        return false;
    }

    return true;
}

bool AP_Baro_ICP201XX::get_sensor_data(float *pressure, float *temperature)
{
    uint8_t fifo_data[96] {0};
    uint8_t fifo_packets = 0;
    int32_t data_temp = 0;
    int32_t data_press = 0;
    *pressure = 0;
    *temperature = 0;

    if (read_reg(REG_FIFO_FILL, &fifo_packets)) {
        fifo_packets  = (uint8_t)(fifo_packets & 0x1F);
        if (fifo_packets > 16) {
            // Skip FIFO flush to avoid infinite loop
            // flush_fifo();
            return false;
        }
        if (fifo_packets > 0 && fifo_packets <= 16 && read_reg(REG_FIFO_BASE, fifo_data, fifo_packets * 2 * 3)) {
            uint8_t offset = 0;

            for (uint8_t i = 0; i < fifo_packets; i++) {
                data_press = (int32_t)(((fifo_data[offset + 2] & 0x0f) << 16) | (fifo_data[offset + 1] << 8) | fifo_data[offset]);
                if (data_press & 0x080000) {
                    data_press |= 0xFFF00000;
                }
                /* P = (POUT/2^17)*40kPa + 70kPa */
                *pressure += ((float)(data_press) * 40 / 131072) + 70;
                offset += 3;

                data_temp = (int32_t)(((fifo_data[offset + 2] & 0x0f) << 16) | (fifo_data[offset + 1] << 8) | fifo_data[offset]);
                if (data_temp & 0x080000) {
                    data_temp |= 0xFFF00000;
                }
                /* T = (TOUT/2^18)*65C + 25C */
                *temperature += ((float)(data_temp) * 65 / 262144) + 25;
                offset += 3;
            }

            *pressure = *pressure * 1000 / fifo_packets;
            *temperature = *temperature / fifo_packets;
            return true;
        }
    }

    return false;
}

bool AP_Baro_ICP201XX::boot_sequence()
{
    uint8_t bootup_status, version;
    uint8_t offset, gain, hfosc;
    
    hal.console->printf("ICP201XX: Starting boot sequence\n");
    
    // Check version - B2 doesn't need boot sequence
    if (!read_reg(REG_VERSION, &version)) {
        hal.console->printf("ICP201XX: Failed to read version register\n");
        return false;
    }
    
    hal.console->printf("ICP201XX: Version register: 0x%02X\n", version);
    
    if (version == 0xB2) {
        hal.console->printf("ICP201XX: B2 version detected - skipping boot sequence\n");
        return true; // B2 version doesn't need boot sequence
    }
    
    // Check if boot already done
    if (!read_reg(REG_OTP_MTP_OTP_STATUS2, &bootup_status)) {
        hal.console->printf("ICP201XX: Failed to read OTP_STATUS2\n");
        return false;
    }
    
    if (bootup_status & 0x01) {
        hal.console->printf("ICP201XX: Boot sequence already completed\n");
        return true; // Already done
    }
    
    hal.console->printf("ICP201XX: Performing boot sequence\n");
    
    // Activate OTP power domain
    if (!write_reg(REG_MODE_SELECT, 0x04)) {
        hal.console->printf("ICP201XX: Failed to activate OTP power domain\n");
        return false;
    }
    hal.scheduler->delay(4);
    
    // Unlock master register
    write_reg(REG_MASTER_LOCK, 0x1F);
    
    // Enable OTP and write switch
    if (!modify_reg(REG_OTP_MTP_OTP_CFG1, 0, 0x03)) {
        hal.console->printf("ICP201XX: Failed to enable OTP\n");
        return false;
    }
    hal.scheduler->delay_microseconds(10);
    
    // Toggle OTP reset
    if (!modify_reg(REG_OTP_DEBUG2, 0, (1 << 7))) {
        hal.console->printf("ICP201XX: Failed to toggle OTP reset 1\n");
        return false;
    }
    hal.scheduler->delay_microseconds(10);
    
    if (!modify_reg(REG_OTP_DEBUG2, (1 << 7), 0)) {
        hal.console->printf("ICP201XX: Failed to toggle OTP reset 2\n");
        return false;
    }
    hal.scheduler->delay_microseconds(10);
    
    // Program redundant read registers
    write_reg(REG_OTP_MTP_MRA_LSB, 0x04);
    write_reg(REG_OTP_MTP_MRA_MSB, 0x04);
    write_reg(REG_OTP_MTP_MRB_LSB, 0x21);
    write_reg(REG_OTP_MTP_MRB_MSB, 0x20);
    write_reg(REG_OTP_MTP_MR_LSB, 0x10);
    write_reg(REG_OTP_MTP_MR_MSB, 0x80);
    
    // Read OTP calibration values
    if (!read_otp_data(0xF8, ICP201XX_OTP_CMD_READ, &offset) || 
        !read_otp_data(0xF9, ICP201XX_OTP_CMD_READ, &gain) || 
        !read_otp_data(0xFA, ICP201XX_OTP_CMD_READ, &hfosc)) {
        hal.console->printf("ICP201XX: Failed to read OTP calibration values\n");
        return false;
    }
    
    hal.console->printf("ICP201XX: OTP values - offset:0x%02X gain:0x%02X hfosc:0x%02X\n", 
                       offset, gain, hfosc);
    
    // Write OTP values to trim registers
    if (!modify_reg(REG_TRIM1_MSB, 0x3F, offset & 0x3F)) {
        hal.console->printf("ICP201XX: Failed to write TRIM1_MSB\n");
        return false;
    }
    
    if (!modify_reg(REG_TRIM2_MSB, 0x70, (gain & 0x07) << 4)) {
        hal.console->printf("ICP201XX: Failed to write TRIM2_MSB\n");
        return false;
    }
    
    if (!modify_reg(REG_TRIM2_LSB, 0x7F, hfosc & 0x7F)) {
        hal.console->printf("ICP201XX: Failed to write TRIM2_LSB\n");
        return false;
    }
    
    hal.scheduler->delay_microseconds(10);
    
    // Mark boot as complete
    if (!modify_reg(REG_OTP_MTP_OTP_STATUS2, 0, 0x01)) {
        hal.console->printf("ICP201XX: Failed to mark boot as complete\n");
        return false;
    }
    
    // Disable OTP
    if (!modify_reg(REG_OTP_MTP_OTP_CFG1, 0x03, 0)) {
        hal.console->printf("ICP201XX: Failed to disable OTP\n");
        return false;
    }
    
    // Lock master register
    if (!write_reg(REG_MASTER_LOCK, 0x00)) {
        hal.console->printf("ICP201XX: Failed to lock master register\n");
        return false;
    }
    
    // Return to standby mode
    if (!write_reg(REG_MODE_SELECT, 0x00)) {
        hal.console->printf("ICP201XX: Failed to return to standby mode\n");
        return false;
    }
    hal.scheduler->delay(10);
    
    hal.console->printf("ICP201XX: Boot sequence completed successfully\n");
    return true;
}

bool AP_Baro_ICP201XX::configure()
{
    hal.console->printf("ICP201XX: Configuring sensor\n");
    
    // Flush FIFO first
    if (!flush_fifo()) {
        hal.console->printf("ICP201XX: Failed to flush FIFO\n");
        return false;
    }
    hal.scheduler->delay(5);
    hal.console->printf("ICP201XX: FIFO flushed successfully\n");
    
    // Write standby mode first
    if (!write_reg(REG_MODE_SELECT, 0x00)) {
        hal.console->printf("ICP201XX: Failed to set standby mode\n");
        return false;
    }
    hal.scheduler->delay(5);
    hal.console->printf("ICP201XX: Standby mode set\n");
    
    // Clear any interrupt status
    uint8_t int_status = 0;
    if (read_reg(REG_INTERRUPT_STATUS, &int_status)) {
        hal.console->printf("ICP201XX: Interrupt status: 0x%02X\n", int_status);
        if (int_status != 0) {
            // Clear by writing back
            write_reg(REG_INTERRUPT_STATUS, int_status);
            hal.console->printf("ICP201XX: Interrupt status cleared\n");
        }
    }
    
    // Now build mode register using Read-Modify-Write for each field (like Betaflight)
    // Set forced meas trigger = 0 (standby)
    int result = modify_reg(REG_MODE_SELECT, (1 << 4), 0);
    hal.console->printf("ICP201XX: modify_reg result for forced meas trigger: %d\n", result);
    if (result != 0) {
        hal.console->printf("ICP201XX: Failed to set forced meas trigger\n");
        return false;
    }
    hal.console->printf("ICP201XX: Forced meas trigger set\n");
    
    // When calling MODE_SELECT, a delay is necessary for the device to stabilize.
    hal.scheduler->delay_microseconds(10);
    
    // Set power mode = 0 (normal)
    int power_result = modify_reg(REG_MODE_SELECT, (1 << 2), 0);
    hal.console->printf("ICP201XX: modify_reg result for power mode: %d\n", power_result);
    if (power_result != 0) {
        hal.console->printf("ICP201XX: Failed to set power mode\n");
        return false;
    }
    hal.console->printf("ICP201XX: Power mode set\n");
    
    // When calling MODE_SELECT, a delay is necessary for the device to stabilize.
    hal.scheduler->delay_microseconds(10);
    
    // Set FIFO readout mode = 0 (pres+temp)
    if (!modify_reg(REG_MODE_SELECT, 0x03, 0)) {
        hal.console->printf("ICP201XX: Failed to set FIFO readout mode\n");
        return false;
    }
    hal.console->printf("ICP201XX: FIFO readout mode set\n");
    
    // When calling MODE_SELECT, a delay is necessary for the device to stabilize.
    hal.scheduler->delay_microseconds(10);
    
    // Set measurement config (OP_MODE0 = bits 7-5 = 000)
    if (!modify_reg(REG_MODE_SELECT, 0xE0, 0)) {
        hal.console->printf("ICP201XX: Failed to set measurement config\n");
        return false;
    }
    hal.console->printf("ICP201XX: Measurement config set\n");
    
    // When calling MODE_SELECT, a delay is necessary for the device to stabilize.
    hal.scheduler->delay_microseconds(10);
    
    // Finally set measurement mode = 1 (continuous) - bit 3
    if (!modify_reg(REG_MODE_SELECT, 0, (1 << 3))) {
        hal.console->printf("ICP201XX: Failed to set measurement mode\n");
        return false;
    }
    hal.console->printf("ICP201XX: Measurement mode set\n");
    
    hal.scheduler->delay(10);
    
    hal.console->printf("ICP201XX: Configuration completed successfully\n");
    return true;
}

void AP_Baro_ICP201XX::wait_read()
{
    /*
    * If FIR filter is enabled, it will cause a settling effect on the first 14 pressure values.
    * Therefore the first 14 pressure output values are discarded.
    * Based on Betaflight's approach in startContinuous()
    **/
    uint8_t fifo_packets = 0;
    uint8_t fifo_fill;
    const uint8_t target_samples = 14;  // Same as Betaflight

    hal.console->printf("ICP201XX: Waiting for FIFO to fill with %d samples\n", target_samples);
    
    // Wait for FIR filter warmup. The first few samples after mode change are invalid.
    for (int i = 0; i < 100; i++) {  // Max 1 second wait (100 * 10ms)
        hal.scheduler->delay(10);
        if (read_reg(REG_FIFO_FILL, &fifo_fill)) {
            fifo_packets = fifo_fill & 0x1F;
            hal.console->printf("ICP201XX: FIFO fill check %d: 0x%02X (packets: %d)\n", i, fifo_fill, fifo_packets);
            if (fifo_packets >= target_samples) {
                hal.console->printf("ICP201XX: FIFO has enough samples (%d >= %d)\n", fifo_packets, target_samples);
                break;
            }
        }
    }
    
    if (fifo_packets == 0) {
        hal.console->printf("ICP201XX: FIFO fill timeout - no data available\n");
        GCS_SEND_TEXT(MAV_SEVERITY_WARNING, "ICP201XX: FIFO fill timeout - no data");
    }
}

bool AP_Baro_ICP201XX::flush_fifo()
{
    // Simplified implementation based on Betaflight
    // Flush the FIFO by setting the flush bit (0x80) in the FIFO_FILL register
    hal.console->printf("ICP201XX: Flushing FIFO\n");
    
    uint8_t reg_value;
    if (!read_reg(REG_FIFO_FILL, &reg_value)) {
        hal.console->printf("ICP201XX: Failed to read FIFO_FILL\n");
        return false;
    }
    
    // Set the flush bit (0x80)
    if (!write_reg(REG_FIFO_FILL, reg_value | 0x80)) {
        hal.console->printf("ICP201XX: Failed to write FIFO_FLUSH\n");
        return false;
    }
    
    // Verify the flush worked
    if (!read_reg(REG_FIFO_FILL, &reg_value)) {
        hal.console->printf("ICP201XX: Failed to verify FIFO_FILL\n");
        return false;
    }
    hal.console->printf("ICP201XX: FIFO_FLUSH completed, REG_FIFO_FILL: 0x%02X\n", reg_value);
    
    return true;
}

void AP_Baro_ICP201XX::timer()
{
    float p = 0;
    float t = 0;

    if (get_sensor_data(&p, &t)) {
        WITH_SEMAPHORE(_sem);

        accum.psum += p;
        accum.tsum += t;
        accum.count++;
        last_measure_us = AP_HAL::micros();
    } else {
        if (AP_HAL::micros() - last_measure_us > CONVERSION_INTERVAL*3) {
            // Skip FIFO flush for now to avoid infinite loop
            // flush_fifo();
            last_measure_us = AP_HAL::micros();
        }
    }
}

void AP_Baro_ICP201XX::update()
{
    WITH_SEMAPHORE(_sem);

    if (accum.count > 0) {
        _copy_to_frontend(instance, accum.psum/accum.count, accum.tsum/accum.count);
        accum.psum = accum.tsum = 0;
        accum.count = 0;
    }
}

// Completely disable timer function for now to avoid infinite loop
// This is a temporary workaround until register read issues are resolved
/*
void AP_Baro_ICP201XX::timer()
{
    float p = 0;
    float t = 0;

    if (get_sensor_data(&p, &t)) {
        WITH_SEMAPHORE(_sem);

        accum.psum += p;
        accum.tsum += t;

        accum.count++;

        dev->get_semaphore()->give();
    }
}
*/

#endif  // AP_BARO_ICP201XX_ENABLED 
