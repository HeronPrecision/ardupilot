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

// SPI commands for ICP201XX
#define ICP201XX_SPI_CMD_WRITE  0x33
#define ICP201XX_SPI_CMD_READ   0x3C

#define CONVERSION_INTERVAL     25000

// Hardware timing requirement for MODE_SELECT register (matching Betaflight)
// This delay is UNCONDITIONAL and always applied after MODE_SELECT writes
#define MODE_SELECT_LATCH_US    200

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
#define REG_OTP_MTP_OTP_CMD     0xB6
#define REG_OTP_MTP_RD_DATA     0xB8
#define REG_OTP_MTP_OTP_STATUS  0xB9
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
    // CRITICAL DEBUG MESSAGE - This should always appear
    hal.console->printf("=== ICP201XX: DEBUG HELLO MESSAGE IN CONSTRUCTOR ===\n");
    hal.console->printf("=== ICP201XX: CONSTRUCTOR CALLED - THIS IS OUR TEST ===\n");
    GCS_SEND_TEXT(MAV_SEVERITY_CRITICAL, "ICP201XX: HELLO - CONSTRUCTOR DEBUG TEST");
}



AP_Baro_Backend *AP_Baro_ICP201XX::probe(AP_Baro &baro, AP_HAL::Device &dev)
{
    // DEBUG: Wait for console to be ready
    hal.scheduler->delay(1000);
    
    // DEBUG: Very early debug output
    hal.console->printf("=== ICP201XX: PROBE FUNCTION STARTING ===\n");
    hal.console->printf("=== ICP201XX: HELLO - PROBE DEBUG MESSAGE ===\n");
    hal.console->printf("ICP201XX: probe() ENTRY - bus %u addr 0x%02x\n", 
           dev.bus_num(), dev.get_bus_address());
    hal.console->printf("=== ICP201XX: IF YOU SEE THIS, OUR DEBUG METHOD WORKS ===\n");
    GCS_SEND_TEXT(MAV_SEVERITY_CRITICAL, "ICP201XX: HELLO - PROBE DEBUG TEST");
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: probe ENTRY");
    
    AP_Baro_ICP201XX *sensor = NEW_NOTHROW AP_Baro_ICP201XX(baro, dev);
    if (!sensor) {
        hal.console->printf("ICP201XX: MEMORY ALLOCATION FAILED\n");
        GCS_SEND_TEXT(MAV_SEVERITY_CRITICAL, "ICP201XX: MEMORY FAILED");
        return nullptr;
    }
    
    hal.console->printf("=== ICP201XX: SENSOR OBJECT CREATED ===\n");
    hal.console->printf("=== ICP201XX: THIS IS ANOTHER HELLO TEST ===\n");
    hal.console->printf("ICP201XX: sensor created, calling init()\n");
    GCS_SEND_TEXT(MAV_SEVERITY_CRITICAL, "ICP201XX: HELLO - SENSOR CREATED");
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: calling init");
    
    bool init_result = sensor->init();
    hal.console->printf("ICP201XX: init() returned %s\n", init_result ? "SUCCESS" : "FAILED");
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: init result=%s", init_result ? "SUCCESS" : "FAILED");
    
    if (!init_result) {
        hal.console->printf("ICP201XX: PROBE FAILED - init returned false\n");
        GCS_SEND_TEXT(MAV_SEVERITY_CRITICAL, "ICP201XX: PROBE FAILED");
        delete sensor;
        return nullptr;
    }
    
    hal.console->printf("ICP201XX: PROBE SUCCESS!\n");
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "ICP201XX: PROBE SUCCESS");
    return sensor;
}

bool AP_Baro_ICP201XX::init()
{
    if (!dev) {
        return false;
    }

    dev->get_semaphore()->take_blocking();

    // Wait for sensor to be ready - matching Betaflight startup delay
    hal.scheduler->delay(100);

    uint8_t id = 0xFF;
    uint8_t ver = 0xFF;
    
    // Try reading chip ID multiple times
    for (int i = 0; i < 3; i++) {
        if (read_reg(REG_DEVICE_ID, &id)) {
            hal.scheduler->delay(1);
            break;
        }
        hal.scheduler->delay(1);
    }
    
    read_reg(REG_VERSION, &ver);

    hal.console->printf("ICP201XX: Read chip_id=0x%02X version=0x%02X (expecting id=0x%02X)\n", id, ver, ICP201XX_ID);
    
    // ONLY accept the correct chip ID (0x63) - no exceptions!
    if (id != ICP201XX_ID) {
        hal.console->printf("ICP201XX: CHIP ID MISMATCH! Got 0x%02X, expected 0x%02X\n", id, ICP201XX_ID);
        goto failed;
    }

    if (ver != 0x00 && ver != 0xB2) {
        hal.console->printf("ICP201XX: Invalid version 0x%02X\n", ver);
        goto failed;
    }

    hal.scheduler->delay(10);

    // Boot sequence handles OTP calibration
    if (!boot_sequence()) {
        hal.console->printf("ICP201XX: Boot sequence failed\n");
        goto failed;
    }

    // Soft reset before configuration
    soft_reset();

    if (!configure()) {
        hal.console->printf("ICP201XX: Configuration failed\n");
        goto failed;
    }

    // Wait for FIR filter warmup and discard initial samples
    wait_read();

    dev->set_retries(0);

    instance = _frontend.register_sensor();

    dev->set_device_type(DEVTYPE_BARO_ICP201XX);
    set_bus_id(instance, dev->get_bus_id());

    dev->get_semaphore()->give();

    // Register timer at 25ms interval (read every 25ms to collect ~3 samples at 120Hz)
    dev->register_periodic_callback(25000, FUNCTOR_BIND_MEMBER(&AP_Baro_ICP201XX::timer, void));
    
    hal.console->printf("ICP201XX: Init successful\n");
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
        // Use full-duplex SPI for proper 3-byte transaction
        uint8_t buf[3] = { ICP201XX_SPI_CMD_READ, REG_EMPTY, 0xFF };
        dev->transfer_fullduplex(buf, 3);
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
    
    if (dev->bus_type() == AP_HAL::Device::BUS_TYPE_SPI) {
        // SPI mode: Use Betaflight-compatible command structure with full-duplex SPI
        // ICP201XX requires proper 3-byte full-duplex transactions
        // Command format: [CMD, REG, 0xFF] -> response data appears in byte 2 (third byte)
        uint8_t spi_buf[32];
        
        // For chip ID read: send [0x3C, 0x0C, 0xFF] and get data in spi_buf[2]
        spi_buf[0] = ICP201XX_SPI_CMD_READ;  // 0x3C
        spi_buf[1] = reg;                     // Register address
        memset(&spi_buf[2], 0xFF, len);       // Dummy bytes to clock in data
        
        // Perform full-duplex SPI transaction
        ret = dev->transfer_fullduplex(spi_buf, len + 2);
        
        if (ret) {
            // Data appears starting at byte 2 (third byte of response)
            memcpy(buf, &spi_buf[2], len);
            

        }
    } else {
        // I2C mode: standard transfer
        ret = dev->transfer(&reg, 1, buf, len);
    }
    
    // Perform dummy read after register access (except for EMPTY register)
    dummy_reg();
    return ret;
}

bool AP_Baro_ICP201XX::read_reg(uint8_t reg, uint8_t *val)
{
    return read_reg(reg, val, 1);
}

bool AP_Baro_ICP201XX::write_reg(uint8_t reg, uint8_t val)
{
    bool ret;
    
    if (dev->bus_type() == AP_HAL::Device::BUS_TYPE_SPI) {
        // SPI mode: Use Betaflight-compatible 3-byte full-duplex transaction
        // Transaction format: [WRITE_CMD, REG, VAL]
        uint8_t buf[3];
        buf[0] = ICP201XX_SPI_CMD_WRITE;  // 0x33
        buf[1] = reg;
        buf[2] = val;
        ret = dev->transfer_fullduplex(buf, 3);
    } else {
        // I2C mode: standard transfer
        uint8_t data[2] = { reg, val };
        ret = dev->transfer(data, sizeof(data), nullptr, 0);
    }
    
    dummy_reg();
    return ret;
}

void AP_Baro_ICP201XX::soft_reset()
{
    // Perform soft reset command
    write_reg(REG_MODE_SELECT, 0x80); // Soft reset command
    hal.scheduler->delay(50); // Reset delay matching Betaflight
    
    // Write standby mode
    write_reg(REG_MODE_SELECT, 0x00);
    hal.scheduler->delay(5);
    
    // Flush FIFO
    flush_fifo();
    hal.scheduler->delay(5);
    
    // Clear any interrupt status
    uint8_t int_status = 0;
    if (read_reg(REG_INTERRUPT_STATUS, &int_status)) {
        if (int_status != 0) {
            write_reg(REG_INTERRUPT_STATUS, int_status); // Clear by writing back
        }
    }
}

bool AP_Baro_ICP201XX::mode_select(uint8_t mode)
{
    // Write mode and apply UNCONDITIONAL latch delay
    bool ret = write_reg(REG_MODE_SELECT, mode);
    
    // CRITICAL: MODE_SELECT requires 200µs latch delay - ALWAYS apply this
    hal.scheduler->delay_microseconds(MODE_SELECT_LATCH_US);
    
    return ret;
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
            flush_fifo();
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
    uint8_t reg_value = 0;
    uint8_t offset = 0, gain = 0, Hfosc = 0;
    uint8_t version = 0;
    uint8_t bootup_status = 0;

    // Read version register
    if (!read_reg(REG_VERSION, &version)) {
        return false;
    }

    if (version == 0xB2) {
        // B2 version doesn't need boot sequence
        hal.console->printf("ICP201XX: B2 version detected, skipping boot sequence\n");
        return true;
    }

    // Read boot up status and avoid re-running boot sequence if already done
    if (!read_reg(REG_OTP_MTP_OTP_STATUS2, &bootup_status)) {
        return false;
    }

    if (bootup_status & 0x01) {
        // Boot sequence already done
        hal.console->printf("ICP201XX: Boot sequence already completed\n");
        return true;
    }

    hal.console->printf("ICP201XX: Running boot sequence for non-B2 variant\n");

    // Activate OTP power domain
    if (!write_reg(REG_MODE_SELECT, 0x04)) return false;
    hal.scheduler->delay(4);

    // Unlock master register
    write_reg(REG_MASTER_LOCK, 0x1F);

    // Enable OTP and write switch
    if (!read_reg(REG_OTP_MTP_OTP_CFG1, &reg_value)) return false;
    reg_value |= 0x03;
    if (!write_reg(REG_OTP_MTP_OTP_CFG1, reg_value)) return false;
    hal.scheduler->delay_microseconds(10);

    // Toggle OTP reset
    if (!read_reg(REG_OTP_DEBUG2, &reg_value)) return false;
    reg_value |= (1 << 7);
    if (!write_reg(REG_OTP_DEBUG2, reg_value)) return false;
    hal.scheduler->delay_microseconds(10);
    
    if (!read_reg(REG_OTP_DEBUG2, &reg_value)) return false;
    reg_value &= ~(1 << 7);
    if (!write_reg(REG_OTP_DEBUG2, reg_value)) return false;
    hal.scheduler->delay_microseconds(10);

    // Program redundant read registers
    write_reg(REG_OTP_MTP_MRA_LSB, 0x04);
    write_reg(REG_OTP_MTP_MRA_MSB, 0x04);
    write_reg(REG_OTP_MTP_MRB_LSB, 0x21);
    write_reg(REG_OTP_MTP_MRB_MSB, 0x20);
    write_reg(REG_OTP_MTP_MR_LSB, 0x10);
    write_reg(REG_OTP_MTP_MR_MSB, 0x80);

    // Read OTP calibration values
    if (!read_otp_data(0xF8, 0x10, &offset)) return false;
    if (!read_otp_data(0xF9, 0x10, &gain)) return false;
    if (!read_otp_data(0xFA, 0x10, &Hfosc)) return false;

    // Write OTP values to trim registers
    if (!read_reg(REG_TRIM1_MSB, &reg_value)) return false;
    reg_value = (reg_value & ~0x3F) | (offset & 0x3F);
    if (!write_reg(REG_TRIM1_MSB, reg_value)) return false;

    if (!read_reg(REG_TRIM2_MSB, &reg_value)) return false;
    reg_value = (reg_value & ~0x70) | ((gain & 0x07) << 4);
    if (!write_reg(REG_TRIM2_MSB, reg_value)) return false;

    if (!read_reg(REG_TRIM2_LSB, &reg_value)) return false;
    reg_value = (reg_value & ~0x7F) | (Hfosc & 0x7F);
    if (!write_reg(REG_TRIM2_LSB, reg_value)) return false;

    hal.scheduler->delay_microseconds(10);

    // Mark boot as complete
    if (!read_reg(REG_OTP_MTP_OTP_STATUS2, &reg_value)) return false;
    reg_value |= 0x01;
    if (!write_reg(REG_OTP_MTP_OTP_STATUS2, reg_value)) return false;

    // Disable OTP and write switch
    if (!read_reg(REG_OTP_MTP_OTP_CFG1, &reg_value)) return false;
    reg_value &= ~0x03;
    if (!write_reg(REG_OTP_MTP_OTP_CFG1, reg_value)) return false;

    // Lock master register
    if (!write_reg(REG_MASTER_LOCK, 0x00)) return false;

    // Return to standby mode
    if (!write_reg(REG_MODE_SELECT, 0x00)) return false;
    hal.scheduler->delay(10);

    hal.console->printf("ICP201XX: Boot sequence completed successfully\n");
    return true;
}

bool AP_Baro_ICP201XX::configure()
{
    // Configure using Read-Modify-Write for each field, matching Betaflight exactly
    // This ensures proper sequencing and timing
    
    // Set forced meas trigger = 0 (standby)
    uint8_t reg_value;
    if (!read_reg(REG_MODE_SELECT, &reg_value)) return false;
    reg_value = (reg_value & ~(1 << 4)) | (0 << 4);
    if (!mode_select(reg_value)) return false;

    // Set power mode = 0 (normal)
    if (!read_reg(REG_MODE_SELECT, &reg_value)) return false;
    reg_value = (reg_value & ~(1 << 2)) | (0 << 2);
    if (!mode_select(reg_value)) return false;

    // Set FIFO readout mode = 0 (pressure+temp interleaved)
    if (!read_reg(REG_MODE_SELECT, &reg_value)) return false;
    reg_value = (reg_value & ~0x03) | 0;
    if (!mode_select(reg_value)) return false;

    // Set operation mode = Mode 1 (bits 7-5 = 001 = 120Hz ODR)
    if (!read_reg(REG_MODE_SELECT, &reg_value)) return false;
    reg_value = (reg_value & ~0xE0) | (1 << 5);  // Mode 1
    if (!mode_select(reg_value)) return false;

    // Finally, set measurement mode = 1 (continuous) - bit 3
    if (!read_reg(REG_MODE_SELECT, &reg_value)) return false;
    reg_value = (reg_value & ~(1 << 3)) | (1 << 3);
    if (!mode_select(reg_value)) return false;

    hal.scheduler->delay(10);
    
    hal.console->printf("ICP201XX: Configured for Mode 1 (120Hz ODR) continuous operation\n");
    return true;
}

void AP_Baro_ICP201XX::wait_read()
{
    // Wait for FIR filter warmup. The first 14 samples after mode change are invalid.
    // At 120Hz ODR (Mode 1), 14 samples = ~117ms
    const uint8_t target_samples = 14;
    uint8_t fifo_fill = 0;
    uint8_t fifo_packets = 0;

    // Wait up to 1 second for FIFO to fill with warmup samples
    for (int i = 0; i < 100; i++) {
        hal.scheduler->delay(10);
        if (read_reg(REG_FIFO_FILL, &fifo_fill)) {
            fifo_packets = fifo_fill & 0x1F;
            if (fifo_packets >= target_samples) {
                break;
            }
        }
    }

    // Check if FIFO filled during warmup - if not, sensor may not be working
    if (fifo_packets == 0) {
        hal.console->printf("ICP201XX: Warning - No FIFO data during warmup\n");
    } else {
        hal.console->printf("ICP201XX: FIR warmup complete, flushing %d samples\n", fifo_packets);
    }

    // Flush warmup samples
    flush_fifo();
}

bool AP_Baro_ICP201XX::flush_fifo()
{
    // Flush FIFO by setting the flush bit (0x80)
    uint8_t reg_value;

    if (!read_reg(REG_FIFO_FILL, &reg_value)) {
        return false;
    }

    reg_value |= 0x80;

    if (!write_reg(REG_FIFO_FILL, reg_value)) {
        return false;
    }

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
            flush_fifo();
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

#endif  // AP_BARO_ICP201XX_ENABLED 
