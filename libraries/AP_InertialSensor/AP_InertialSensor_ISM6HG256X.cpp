/*
 * This file is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Written by Heron Precision LLC for the ArduPilot project.
 */
/*
  driver for ST ISM6HG256X intelligent IMU

  6-axis IMU with a low-g accelerometer (up to +/-16g), a separate
  high-g accelerometer (up to +/-256g) and a +/-4000 dps gyroscope.

  The gyroscope and the low-g (+/-16g) accelerometer form the standard
  ArduPilot IMU.  In addition the high-g (+/-256g) accelerometer is
  batched into the same FIFO (tag 0x1D) and used as an auto-ranging
  fallback: on each low-g sample, any axis whose (unfiltered) high-g
  reading shows the true acceleration to be at/beyond the +/-16g low-g
  range is transparently substituted from the high-g channel for that
  sample.  The two accelerometers share the same die and axes
  and run simultaneously, so this extends the usable range to +/-256g at
  no cost to the low-g precision below the rail (the high-g channel is
  coarser at ~7.8 mg/LSB but is the *correct* value once the low-g
  saturates).  The fused output is published as a single ArduPilot accel.

  The part is register-compatible with the ST LSM6DSV16X for the primary
  low-g / gyro path, so that path mirrors AP_InertialSensor_LSM6DSV:
  HAODR mode-1 for round high-accuracy ODRs (1000-8000 Hz) and continuous
  FIFO with tagged burst reads.  Digital filters auto-adapt to ODR so no
  per-rate AAF reconfiguration is needed.  The main sensor-level
  differences from the LSM6DSV are the WHO_AM_I value (0x73) and the
  high-g accelerometer.

  fast sampling is controlled via INS_FAST_SAMPLE / INS_GYRO_RATE
  with base rate 1000 Hz.
 */

#include "AP_InertialSensor_ISM6HG256X.h"

#include <stdio.h>
#include <utility>

#include <AP_BoardConfig/AP_BoardConfig.h>
#include <AP_HAL/utility/sparse-endian.h>
#include <AP_Math/AP_Math.h>

extern const AP_HAL::HAL& hal;

namespace {

// Enable the second-stage digital low-pass filter (LPF2) on the
// accelerometer output.  Bandwidth is set by ISM6HG256X_ACCEL_LPF2_BW.
#ifndef ISM6HG256X_ACCEL_LPF2_ENABLED
#define ISM6HG256X_ACCEL_LPF2_ENABLED 1
#endif

// LPF2 bandwidth selection written to CTRL8 HP_LPF2_XL_BW bits [7:5].
// When CTRL9.LPF2_XL_EN = 1 these select the second-stage cutoff:
//   0x00 = ODR/4   0x20 = ODR/10   0x40 = ODR/20   0x60 = ODR/45
//   0x80 = ODR/100 0xA0 = ODR/200  0xC0 = ODR/400  0xE0 = ODR/800
#ifndef ISM6HG256X_ACCEL_LPF2_BW
#define ISM6HG256X_ACCEL_LPF2_BW 0x20  // ODR/10 -> 200 Hz @ 2 kHz ODR
#endif

// ---- FIFO control registers (R/W) ----
#define ISM6HG256X_REG_FIFO_CTRL1           0x07  // FIFO watermark threshold [7:0]
#define ISM6HG256X_REG_FIFO_CTRL2           0x08  // FIFO watermark threshold [8], compression
#define ISM6HG256X_REG_FIFO_CTRL3           0x09  // FIFO gyro/accel batch data rate
#define ISM6HG256X_REG_FIFO_CTRL4           0x0A  // FIFO mode selection

#define ISM6HG256X_FIFO_CTRL3_BDR_DISABLED  0x00
#define ISM6HG256X_FIFO_CTRL4_MODE_BYPASS    0x00
#define ISM6HG256X_FIFO_CTRL4_MODE_CONTINUOUS 0x06

// ---- Counter batch-data-rate register 1 (R/W) ----
// [3] XL_HG_BATCH_EN: batch the high-g accelerometer into the FIFO
#define ISM6HG256X_REG_COUNTER_BDR_REG1     0x0B
#define ISM6HG256X_COUNTER_BDR_XL_HG_BATCH_EN (1U << 3)

// ---- WHO_AM_I register (R) ----
#define ISM6HG256X_REG_WHO_AM_I             0x0F
#define ISM6HG256X_ID_ISM6HG256X            0x73

// ---- Accelerometer control register 1 (R/W) ----
// [6:4] OP_MODE_XL: operating mode   [3:0] ODR_XL: output data rate
#define ISM6HG256X_REG_CTRL1                0x10

// ---- Gyroscope control register 2 (R/W) ----
// [6:4] OP_MODE_G: operating mode   [3:0] ODR_G: output data rate
#define ISM6HG256X_REG_CTRL2                0x11

// ---- Control register 3 (R/W) ----
#define ISM6HG256X_REG_CTRL3                0x12
#define ISM6HG256X_CTRL3_BDU                (1U << 6)  // block data update
#define ISM6HG256X_CTRL3_IF_INC             (1U << 2)  // auto-increment address
#define ISM6HG256X_CTRL3_SW_RESET           (1U << 0)  // software reset

// [4] OP_MODE: 0=high-performance, 1=high-accuracy ODR
#define ISM6HG256X_CTRL_MODE_HAODR          0x10

// ---- Control register 6 - gyro full-scale selection (R/W) ----
// [2:0] FS_G: gyroscope full-scale
#define ISM6HG256X_REG_CTRL6                0x15
#define ISM6HG256X_CTRL6_FS_G_250DPS        0x01
#define ISM6HG256X_CTRL6_FS_G_500DPS        0x02
#define ISM6HG256X_CTRL6_FS_G_1000DPS       0x03
#define ISM6HG256X_CTRL6_FS_G_2000DPS       0x04
#define ISM6HG256X_CTRL6_FS_G_4000DPS       0x05

// ---- Control register 8 - accel full-scale & LPF2 BW (R/W) ----
// [7:5] HP_LPF2_XL_BW   [1:0] FS_XL: accelerometer full-scale (low-g)
#define ISM6HG256X_REG_CTRL8                0x17
#define ISM6HG256X_CTRL8_FS_XL_2G           0x00
#define ISM6HG256X_CTRL8_FS_XL_4G           0x01
#define ISM6HG256X_CTRL8_FS_XL_8G           0x02
#define ISM6HG256X_CTRL8_FS_XL_16G          0x03

// ---- Control register 9 - accel LPF2 enable (R/W) ----
#define ISM6HG256X_REG_CTRL9                0x18
#define ISM6HG256X_CTRL9_LPF2_XL_EN         (1U << 3)

// ---- High-g accelerometer control register 1 (R/W) ----
// [2:0] FS_XL_HG   [5:3] ODR_XL_HG   [7] XL_HG_REGOUT_EN
#define ISM6HG256X_REG_CTRL1_XL_HG          0x4E
#define ISM6HG256X_CTRL1_XL_HG_FS_256G      0x03        // fs_xl_hg = 256g
#define ISM6HG256X_CTRL1_XL_HG_REGOUT_EN    (1U << 7)   // route high-g to output/FIFO
// ODR_XL_HG codes (written to bits [5:3])
#define ISM6HG256X_HG_ODR_480HZ             0x03
#define ISM6HG256X_HG_ODR_960HZ             0x04
#define ISM6HG256X_HG_ODR_1920HZ            0x05
#define ISM6HG256X_HG_ODR_3840HZ            0x06
#define ISM6HG256X_HG_ODR_7680HZ            0x07

// ---- FIFO status registers (R) ----
#define ISM6HG256X_REG_FIFO_STATUS1         0x1B  // DIFF_FIFO [7:0]
#define ISM6HG256X_REG_FIFO_STATUS2         0x1C  // flags + DIFF_FIFO [8]
#define ISM6HG256X_FIFO_STATUS2_DIFF_FIFO_8 (1U << 0)

// ---- Status register (R) ----
#define ISM6HG256X_REG_STATUS               0x1E
#define ISM6HG256X_STATUS_XLDA              (1U << 0)  // accel data available
#define ISM6HG256X_STATUS_GDA               (1U << 1)  // gyro data available
#define ISM6HG256X_STATUS_TDA               (1U << 2)  // temperature data available

// ---- Data output registers (R) ----
#define ISM6HG256X_REG_OUT_TEMP_L           0x20  // temperature output (16-bit)
#define ISM6HG256X_REG_OUTX_L_G             0x22  // gyro XYZ output (6 bytes)
#define ISM6HG256X_REG_OUTX_L_A             0x28  // accel (low-g) XYZ output (6 bytes)

// ---- HAODR configuration register (R/W) ----
// [1:0] HAODR_SEL: high-accuracy ODR mode selection
#define ISM6HG256X_REG_HAODR_CFG            0x62
#define ISM6HG256X_HAODR_CFG_MODE1          0x01

// HAODR mode-1 ODR codes (written to CTRL1/CTRL2 ODR_XL/ODR_G [3:0])
#define ISM6HG256X_MODE1_ODR_125HZ          0x06
#define ISM6HG256X_MODE1_ODR_250HZ          0x07
#define ISM6HG256X_MODE1_ODR_500HZ          0x08
#define ISM6HG256X_MODE1_ODR_1000HZ         0x09
#define ISM6HG256X_MODE1_ODR_2000HZ         0x0A
#define ISM6HG256X_MODE1_ODR_4000HZ         0x0B
#define ISM6HG256X_MODE1_ODR_8000HZ         0x0C

// ---- FIFO data output registers (R) ----
#define ISM6HG256X_REG_FIFO_DATA_OUT_TAG    0x78  // FIFO tag byte
#define ISM6HG256X_REG_FIFO_DATA_OUT_X_L    0x79  // FIFO data start

// ---- SPI protocol ----
#define ISM6HG256X_SPI_READ_FLAG            0x80

// ---- Driver timing constants ----
#define ISM6HG256X_DEFAULT_BACKEND_RATE_HZ  1000
#define ISM6HG256X_INIT_MAX_TRIES           5
#define ISM6HG256X_RESET_TIMEOUT_MS         100
#define ISM6HG256X_DATA_READY_TIMEOUT_MS    20
#define ISM6HG256X_POWERUP_DELAY_MS         5

// ---- FIFO sizing ----
#define ISM6HG256X_PRIMARY_FIFO_WATERMARK_WORDS 2
#define ISM6HG256X_FIFO_MAX_DRAIN_WORDS     32
#define ISM6HG256X_FIFO_BURST_WORDS         16

// temperature update interval in milliseconds
#define ISM6HG256X_TEMPERATURE_UPDATE_MS    100

// ---- Temperature conversion ----
#define ISM6HG256X_TEMPERATURE_ZERO_C       25.0f
#define ISM6HG256X_TEMPERATURE_SENSITIVITY  256.0f  // LSB/degC

// ---- Accelerometer sensitivity (mg/LSB -> m/s^2) ----
#define ISM6HG256X_ACCEL_SCALE_2G           (GRAVITY_MSS *  2.0f / 32768.0f)
#define ISM6HG256X_ACCEL_SCALE_4G           (GRAVITY_MSS *  4.0f / 32768.0f)
#define ISM6HG256X_ACCEL_SCALE_8G           (GRAVITY_MSS *  8.0f / 32768.0f)
#define ISM6HG256X_ACCEL_SCALE_16G          (GRAVITY_MSS * 16.0f / 32768.0f)

// ---- Gyroscope sensitivity per ST datasheet (mdps/LSB -> rad/s) ----
#define ISM6HG256X_GYRO_SCALE_250DPS        radians(8.75f   / 1000.0f)
#define ISM6HG256X_GYRO_SCALE_500DPS        radians(17.50f  / 1000.0f)
#define ISM6HG256X_GYRO_SCALE_1000DPS       radians(35.0f   / 1000.0f)
#define ISM6HG256X_GYRO_SCALE_2000DPS       radians(70.0f   / 1000.0f)
#define ISM6HG256X_GYRO_SCALE_4000DPS       radians(140.0f  / 1000.0f)

// ---- High-g accelerometer sensitivity (mg/LSB -> m/s^2) ----
// +/-256g over a signed 16-bit output = 7.8125 mg/LSB
#define ISM6HG256X_ACCEL_HG_SCALE_256G      (GRAVITY_MSS * 256.0f / 32768.0f)

// The low-g accelerometer is considered out of range - and replaced by the
// high-g reading on that axis - when EITHER:
//  (a) the *unfiltered* high-g channel shows the true acceleration at/above
//      ISM6HG256X_LOWG_RANGE (just under the +/-16g rail), or
//  (b) the low-g FIFO word itself has reached its rail.
// (a) is the primary trigger.  The low-g FIFO word is LPF2-filtered, so a brief
// >16g spike ramps it toward the rail too slowly to be flagged in time; the
// high-g channel (no LPF2) sees the spike immediately, so triggering on its
// magnitude captures fast transients that low-g rail-detection alone would miss.
#define ISM6HG256X_LOWG_RANGE               (15.8f * GRAVITY_MSS)
// Low-g raw magnitude (LSB) treated as the rail.  Full scale is +/-32768 LSB
// = +/-16g; ~32600 LSB is ~15.9g, i.e. essentially at the rail.
#define ISM6HG256X_LOWG_SATURATION_LSB      32600.0f

// clip limit reported to the ArduPilot frontend.  Because the high-g channel
// covers low-g saturation, the fused accel only truly clips beyond +/-256g.
#define ISM6HG256X_FUSED_CLIP_LIMIT         (255.0f * GRAVITY_MSS)

struct PACKED RawFifoWord {
    uint8_t tag;
    le16_t axis[3];
};

static_assert(sizeof(RawFifoWord) == 7, "RawFifoWord must be 7 bytes");
constexpr uint16_t ISM6HG256X_FIFO_BURST_BUFFER_SIZE = ISM6HG256X_FIFO_BURST_WORDS * sizeof(RawFifoWord) + 1;

}

AP_InertialSensor_ISM6HG256X::AP_InertialSensor_ISM6HG256X(AP_InertialSensor &imu,
                                                           AP_HAL::OwnPtr<AP_HAL::Device> dev,
                                                           enum Rotation rotation)
    : AP_InertialSensor_Backend(imu)
    , _dev(std::move(dev))
    , _rotation(rotation)
    , _accel_scale(ISM6HG256X_ACCEL_SCALE_16G)
    , _gyro_scale(ISM6HG256X_GYRO_SCALE_2000DPS)
    , _accel_hg_scale(ISM6HG256X_ACCEL_HG_SCALE_256G)
    , _have_hg(false)
    , _temperature_last_ms(0)
{
    // the high-g channel transparently covers low-g saturation, so the fused
    // accel does not clip until +/-256g - raise the frontend clip limit to match
    // (default is 15.5g, which would flag every legitimate high-g report).
    _clip_limit = ISM6HG256X_FUSED_CLIP_LIMIT;
}

AP_InertialSensor_ISM6HG256X::~AP_InertialSensor_ISM6HG256X()
{
    if (_fifo_buffer != nullptr) {
        hal.util->free_type(_fifo_buffer, ISM6HG256X_FIFO_BURST_BUFFER_SIZE,
                            AP_HAL::Util::MEM_DMA_SAFE);
    }
}

AP_InertialSensor_Backend *AP_InertialSensor_ISM6HG256X::probe(AP_InertialSensor &imu,
                                                               AP_HAL::OwnPtr<AP_HAL::SPIDevice> dev,
                                                               enum Rotation rotation)
{
    if (!dev) {
        return nullptr;
    }

    auto *sensor = NEW_NOTHROW AP_InertialSensor_ISM6HG256X(imu, std::move(dev), rotation);
    if (sensor == nullptr) {
        return nullptr;
    }

    if (!sensor->init()) {
        delete sensor;
        return nullptr;
    }

    return sensor;
}

void AP_InertialSensor_ISM6HG256X::start()
{
    // pre-fetch instance numbers for checking fast sampling settings
    if (!_imu.get_gyro_instance(gyro_instance) || !_imu.get_accel_instance(accel_instance)) {
        return;
    }

    // determine fast sampling rate (SPI only)
    _backend_rate_hz = ISM6HG256X_DEFAULT_BACKEND_RATE_HZ;
    if (enable_fast_sampling(accel_instance) && get_fast_sampling_rate() > 1) {
        _fast_sampling = (_dev->bus_type() == AP_HAL::Device::BUS_TYPE_SPI);
    }
    if (_fast_sampling) {
        _backend_rate_hz = calculate_backend_rate(ISM6HG256X_DEFAULT_BACKEND_RATE_HZ);
    }
    _backend_period_us = 1000000UL / _backend_rate_hz;

    if (!_imu.register_accel(accel_instance, _backend_rate_hz, _dev->get_bus_id_devtype(DEVTYPE_INS_ISM6HG256X)) ||
        !_imu.register_gyro(gyro_instance, _backend_rate_hz, _dev->get_bus_id_devtype(DEVTYPE_INS_ISM6HG256X))) {
        return;
    }

    {
        WITH_SEMAPHORE(_dev->get_semaphore());
        if (!write_register(ISM6HG256X_REG_FIFO_CTRL1, 0x00, true) ||
            !write_register(ISM6HG256X_REG_FIFO_CTRL2, 0x00, true) ||
            !write_register(ISM6HG256X_REG_FIFO_CTRL3, ISM6HG256X_FIFO_CTRL3_BDR_DISABLED, true) ||
            !write_register(ISM6HG256X_REG_FIFO_CTRL4, ISM6HG256X_FIFO_CTRL4_MODE_BYPASS, true)) {
            return;
        }
        if (!configure_primary_fifo()) {
            return;
        }

        // re-configure ODR registers for the target sampling rate
        const uint8_t odr = odr_code_for_rate(_backend_rate_hz);
        write_register(ISM6HG256X_REG_CTRL1, ISM6HG256X_CTRL_MODE_HAODR | odr, true);
        write_register(ISM6HG256X_REG_CTRL2, ISM6HG256X_CTRL_MODE_HAODR | odr, true);
        // match the high-g ODR to the (possibly fast-sampling) backend rate
        configure_accel_hg(_backend_rate_hz);
        configure_primary_fifo();
    }

    set_gyro_orientation(gyro_instance, _rotation);
    set_accel_orientation(accel_instance, _rotation);

    _fifo_buffer = static_cast<uint8_t *>(hal.util->malloc_type(ISM6HG256X_FIFO_BURST_BUFFER_SIZE,
                                                                 AP_HAL::Util::MEM_DMA_SAFE));
    if (_fifo_buffer == nullptr) {
        AP_HAL::panic("ISM6HG256X: Unable to allocate FIFO buffer");
    }

    periodic_handle = _dev->register_periodic_callback(_backend_period_us,
                                                       FUNCTOR_BIND_MEMBER(&AP_InertialSensor_ISM6HG256X::poll_data, void));
}

bool AP_InertialSensor_ISM6HG256X::update()
{
    update_accel(accel_instance);
    update_gyro(gyro_instance);
    return true;
}

bool AP_InertialSensor_ISM6HG256X::get_output_banner(char* banner, uint8_t banner_len)
{
    snprintf(banner, banner_len, "IMU%u: ISM6HG256X+HG %s sampling %.1fkHz",
             gyro_instance,
             _fast_sampling ? "fast" : "normal",
             _backend_rate_hz * 0.001f);
    return true;
}

bool AP_InertialSensor_ISM6HG256X::init()
{
    _dev->set_read_flag(ISM6HG256X_SPI_READ_FLAG);
    return hardware_init();
}

bool AP_InertialSensor_ISM6HG256X::hardware_init()
{
    hal.scheduler->delay(ISM6HG256X_POWERUP_DELAY_MS);

    WITH_SEMAPHORE(_dev->get_semaphore());
    _dev->set_speed(AP_HAL::Device::SPEED_LOW);
    // 16 checked registers: the LSM6DSV set plus CTRL1_XL_HG and COUNTER_BDR_REG1
    if (!_dev->setup_checked_registers(16, 20)) {
        return false;
    }

    for (uint8_t attempt = 0; attempt < ISM6HG256X_INIT_MAX_TRIES; attempt++) {
        if (!check_whoami()) {
            continue;
        }

        switch (_ism6hg256x_type) {
        case ISM6HG256X_Type::ISM6HG256X:
            _gyro_scale = ISM6HG256X_GYRO_SCALE_2000DPS;
            _accel_scale = ISM6HG256X_ACCEL_SCALE_16G;
            break;
        }

        if (!reset_device()) {
            continue;
        }

        // Enable HAODR mode-1 while every channel is still powered down. The
        // datasheet requires HAODR be enabled/disabled only in power-down, and
        // it applies to the low-g, high-g and gyro alike - so it must be set
        // before configure_accel_hg() / CTRL1 / CTRL2 spin any of them up.
        if (!write_register(ISM6HG256X_REG_HAODR_CFG, ISM6HG256X_HAODR_CFG_MODE1, true)) {
            continue;
        }

        if (!configure_gyro()) {
            continue;
        }

        if (!configure_accel()) {
            continue;
        }

        if (!configure_accel_hg(ISM6HG256X_DEFAULT_BACKEND_RATE_HZ)) {
            continue;
        }

        if (!write_register(ISM6HG256X_REG_CTRL1, ISM6HG256X_CTRL_MODE_HAODR | ISM6HG256X_MODE1_ODR_1000HZ, true)) {
            continue;
        }

        if (!write_register(ISM6HG256X_REG_CTRL2, ISM6HG256X_CTRL_MODE_HAODR | ISM6HG256X_MODE1_ODR_1000HZ, true)) {
            continue;
        }

        if (!write_register(ISM6HG256X_REG_CTRL3, ISM6HG256X_CTRL3_BDU | ISM6HG256X_CTRL3_IF_INC, true)) {
            continue;
        }

        if (!write_register(ISM6HG256X_REG_FIFO_CTRL4, 0x00, true)) {
            continue;
        }

        if (!wait_for_data_ready()) {
            continue;
        }

        _dev->set_speed(AP_HAL::Device::SPEED_HIGH);
        return true;
    }

    _dev->set_speed(AP_HAL::Device::SPEED_HIGH);
    return false;
}

bool AP_InertialSensor_ISM6HG256X::check_whoami()
{
    if (!read_registers(ISM6HG256X_REG_WHO_AM_I, &_whoami, 1)) {
        return false;
    }

    switch (_whoami) {
    case ISM6HG256X_ID_ISM6HG256X:
        _ism6hg256x_type = ISM6HG256X_Type::ISM6HG256X;
        return true;
    }

    return false;
}

bool AP_InertialSensor_ISM6HG256X::reset_device()
{
    if (!write_register(ISM6HG256X_REG_CTRL3, ISM6HG256X_CTRL3_SW_RESET)) {
        return false;
    }

    const uint32_t start_ms = AP_HAL::millis();
    while (AP_HAL::millis() - start_ms < ISM6HG256X_RESET_TIMEOUT_MS) {
        uint8_t ctrl3 = 0;
        hal.scheduler->delay(1);
        if (!read_registers(ISM6HG256X_REG_CTRL3, &ctrl3, 1)) {
            continue;
        }
        if ((ctrl3 & ISM6HG256X_CTRL3_SW_RESET) == 0) {
            return true;
        }
    }

    return false;
}

bool AP_InertialSensor_ISM6HG256X::configure_gyro()
{
    return write_register(ISM6HG256X_REG_CTRL6, ISM6HG256X_CTRL6_FS_G_2000DPS, true);
}

bool AP_InertialSensor_ISM6HG256X::configure_accel()
{
#if ISM6HG256X_ACCEL_LPF2_ENABLED
    const uint8_t ctrl8 = ISM6HG256X_CTRL8_FS_XL_16G | ISM6HG256X_ACCEL_LPF2_BW;
    return write_register(ISM6HG256X_REG_CTRL8, ctrl8, true) &&
           write_register(ISM6HG256X_REG_CTRL9, ISM6HG256X_CTRL9_LPF2_XL_EN, true);
#else
    return write_register(ISM6HG256X_REG_CTRL8, ISM6HG256X_CTRL8_FS_XL_16G, true);
#endif
}

// enable the high-g (+/-256g) accelerometer at the rate nearest the low-g
// backend rate, route it to the output, and batch it into the primary FIFO
// (tag 0x1D) so it is time-ordered with the low-g / gyro samples.
bool AP_InertialSensor_ISM6HG256X::configure_accel_hg(uint16_t rate_hz)
{
    const uint8_t ctrl1_hg = ISM6HG256X_CTRL1_XL_HG_REGOUT_EN |
                             uint8_t(hg_odr_code_for_rate(rate_hz) << 3) |
                             ISM6HG256X_CTRL1_XL_HG_FS_256G;
    return write_register(ISM6HG256X_REG_CTRL1_XL_HG, ctrl1_hg, true) &&
           write_register(ISM6HG256X_REG_COUNTER_BDR_REG1,
                          ISM6HG256X_COUNTER_BDR_XL_HG_BATCH_EN, true);
}

bool AP_InertialSensor_ISM6HG256X::configure_primary_fifo()
{
    const uint8_t odr = odr_code_for_rate(_backend_rate_hz);
    const uint8_t fifo_ctrl3 = uint8_t((odr << 4) | odr);

    return write_register(ISM6HG256X_REG_FIFO_CTRL1,
                          uint8_t(ISM6HG256X_PRIMARY_FIFO_WATERMARK_WORDS & 0xFFU),
                          true) &&
           write_register(ISM6HG256X_REG_FIFO_CTRL2, 0x00, true) &&
           write_register(ISM6HG256X_REG_FIFO_CTRL3, fifo_ctrl3, true) &&
           write_register(ISM6HG256X_REG_FIFO_CTRL4, ISM6HG256X_FIFO_CTRL4_MODE_CONTINUOUS, true);
}

uint8_t AP_InertialSensor_ISM6HG256X::odr_code_for_rate(uint16_t rate_hz) const
{
    switch (rate_hz) {
    case 8000: return ISM6HG256X_MODE1_ODR_8000HZ;
    case 4000: return ISM6HG256X_MODE1_ODR_4000HZ;
    case 2000: return ISM6HG256X_MODE1_ODR_2000HZ;
    case 1000:
    default:   return ISM6HG256X_MODE1_ODR_1000HZ;
    }
}

// map the low-g backend rate to the nearest high-g ODR code.  The high-g
// channel has no HAODR round-rate variants, so its rates (960/1920/3840/7680)
// run a few percent below the low-g rate; this only means the substituted
// high-g sample can be up to ~1 sample old, which is immaterial for the
// multi-g events that trigger the fusion.
uint8_t AP_InertialSensor_ISM6HG256X::hg_odr_code_for_rate(uint16_t rate_hz) const
{
    switch (rate_hz) {
    case 8000: return ISM6HG256X_HG_ODR_7680HZ;
    case 4000: return ISM6HG256X_HG_ODR_3840HZ;
    case 2000: return ISM6HG256X_HG_ODR_1920HZ;
    case 1000:
    default:   return ISM6HG256X_HG_ODR_960HZ;
    }
}

// calculate the backend sample rate accounting for fast sampling
// multiplier and loop rate constraints
uint16_t AP_InertialSensor_ISM6HG256X::calculate_backend_rate(uint16_t base_rate_hz) const
{
    // constrain the gyro rate to be at least the loop rate
    uint8_t min_mult = 1;
    if (get_loop_rate_hz() > base_rate_hz) {
        min_mult = 2;
    }
    if (get_loop_rate_hz() > base_rate_hz * 2) {
        min_mult = 4;
    }
    const uint8_t mult = constrain_int16(get_fast_sampling_rate(), min_mult, 8);
    return constrain_int16(base_rate_hz * mult, base_rate_hz, 8000);
}

bool AP_InertialSensor_ISM6HG256X::fifo_tag_supported_for_primary(const FifoTag tag)
{
    return tag == FifoTag::GyroNC || tag == FifoTag::AccelNC || tag == FifoTag::AccelHG;
}

AP_InertialSensor_ISM6HG256X::FifoTag AP_InertialSensor_ISM6HG256X::decode_fifo_tag(const uint8_t raw_tag)
{
    switch ((raw_tag >> 3) & 0x1FU) {
    case 0x00:
        return FifoTag::Empty;
    case 0x01:
        return FifoTag::GyroNC;
    case 0x02:
        return FifoTag::AccelNC;
    case 0x03:
        return FifoTag::Temperature;
    case 0x04:
        return FifoTag::Timestamp;
    case 0x05:
        return FifoTag::CfgChange;
    case 0x06:
        return FifoTag::AccelNC_T2;
    case 0x07:
        return FifoTag::AccelNC_T1;
    case 0x08:
        return FifoTag::Accel2xC;
    case 0x09:
        return FifoTag::Accel3xC;
    case 0x0A:
        return FifoTag::GyroNC_T2;
    case 0x0B:
        return FifoTag::GyroNC_T1;
    case 0x0C:
        return FifoTag::Gyro2xC;
    case 0x0D:
        return FifoTag::Gyro3xC;
    case 0x1D:
        return FifoTag::AccelHG;
    default:
        return FifoTag::Unsupported;
    }
}

uint8_t AP_InertialSensor_ISM6HG256X::decode_fifo_tag_count(const uint8_t raw_tag)
{
    return (raw_tag >> 1) & 0x03U;
}

bool AP_InertialSensor_ISM6HG256X::wait_for_data_ready()
{
    for (uint8_t i = 0; i < ISM6HG256X_DATA_READY_TIMEOUT_MS; i++) {
        uint8_t status = 0;
        hal.scheduler->delay(1);
        if (!read_registers(ISM6HG256X_REG_STATUS, &status, 1)) {
            continue;
        }
        if ((status & ISM6HG256X_STATUS_GDA) != 0 &&
            (status & ISM6HG256X_STATUS_XLDA) != 0) {
            return true;
        }
    }

    return false;
}

bool AP_InertialSensor_ISM6HG256X::read_registers(uint8_t reg, uint8_t *data, uint8_t len)
{
    return _dev->read_registers(reg, data, len);
}

bool AP_InertialSensor_ISM6HG256X::write_register(uint8_t reg, uint8_t value, bool checked)
{
    return _dev->write_register(reg, value, checked);
}

bool AP_InertialSensor_ISM6HG256X::read_fifo_status(FifoFrame &frame, uint32_t now_us)
{
    uint8_t fifo_status[2] {};
    if (!read_registers(ISM6HG256X_REG_FIFO_STATUS1, fifo_status, sizeof(fifo_status))) {
        _inc_accel_error_count(accel_instance);
        _inc_gyro_error_count(gyro_instance);
        return false;
    }

    frame.unread_words = fifo_status[0] |
                         (((fifo_status[1] & ISM6HG256X_FIFO_STATUS2_DIFF_FIFO_8) != 0U) ? 0x100U : 0U);

    return true;
}

bool AP_InertialSensor_ISM6HG256X::read_fifo_words_block(const uint16_t n_words, uint32_t now_us)
{
    if (_fifo_buffer == nullptr || n_words == 0 || n_words > ISM6HG256X_FIFO_BURST_WORDS) {
        return false;
    }

    _fifo_buffer[0] = ISM6HG256X_REG_FIFO_DATA_OUT_TAG | ISM6HG256X_SPI_READ_FLAG;
    // zero MOSI payload for SPI full-duplex read
    memset(_fifo_buffer + 1, 0, n_words * sizeof(RawFifoWord));
    if (!_dev->transfer_fullduplex(_fifo_buffer, n_words * sizeof(RawFifoWord) + 1)) {
        _inc_accel_error_count(accel_instance);
        _inc_gyro_error_count(gyro_instance);
        return false;
    }

    return true;
}

bool AP_InertialSensor_ISM6HG256X::consume_fifo_word(FifoFrame &frame, SampleFrame &sample, const uint8_t *raw_word)
{
    RawFifoWord raw;
    memcpy(&raw, raw_word, sizeof(raw));

    frame.tag = decode_fifo_tag(raw.tag);
    frame.tag_count = decode_fifo_tag_count(raw.tag);

    if (!fifo_tag_supported_for_primary(frame.tag)) {
        return true;
    }

    const Vector3f axes{
        float(int16_t(le16toh(raw.axis[0]))),
        float(int16_t(le16toh(raw.axis[1]))),
        float(int16_t(le16toh(raw.axis[2]))),
    };

    switch (frame.tag) {
    case FifoTag::GyroNC:
        sample.gyro = axes * _gyro_scale;
        break;
    case FifoTag::AccelHG:
        // high-g word: cache the latest reading (sensor frame, m/s^2). It is not
        // published on its own - it only backfills clipped low-g axes below.
        _accel_hg = axes * _accel_hg_scale;
        _have_hg = true;
        break;
    case FifoTag::AccelNC:
        // low-g word: publish per-axis, substituting the high-g reading for any
        // axis that has reached the +/-16g rail (where the low-g value is invalid).
        sample.accel = fuse_lowg_highg(axes);
        break;
    default:
        return false;
    }

    return true;
}

// combine a low-g accelerometer sample (raw LSB, sensor frame) with the cached
// high-g reading, taking the high-g value on any axis that is out of the low-g
// range.  Returns the fused acceleration in m/s^2 (sensor frame).
//
// The out-of-range test is driven primarily by the (unfiltered) high-g
// magnitude, not by the low-g rail: the low-g FIFO word is LPF2-filtered and
// lags/attenuates brief >16g transients, so detecting saturation from it alone
// would miss exactly the fast impacts the high-g channel exists to catch. The
// low-g rail is kept as a secondary trigger for robustness.
Vector3f AP_InertialSensor_ISM6HG256X::fuse_lowg_highg(const Vector3f &lowg_raw) const
{
    Vector3f accel = lowg_raw * _accel_scale;
    if (!_have_hg) {
        return accel;
    }
    if (fabsf(_accel_hg.x) >= ISM6HG256X_LOWG_RANGE || fabsf(lowg_raw.x) >= ISM6HG256X_LOWG_SATURATION_LSB) {
        accel.x = _accel_hg.x;
    }
    if (fabsf(_accel_hg.y) >= ISM6HG256X_LOWG_RANGE || fabsf(lowg_raw.y) >= ISM6HG256X_LOWG_SATURATION_LSB) {
        accel.y = _accel_hg.y;
    }
    if (fabsf(_accel_hg.z) >= ISM6HG256X_LOWG_RANGE || fabsf(lowg_raw.z) >= ISM6HG256X_LOWG_SATURATION_LSB) {
        accel.z = _accel_hg.z;
    }
    return accel;
}

uint16_t AP_InertialSensor_ISM6HG256X::drain_fifo(uint32_t now_us)
{
    FifoFrame frame{};

    if (!read_fifo_status(frame, now_us)) {
        return 0;
    }

    if (frame.unread_words == 0) {
        return 0;
    }

    uint16_t samples_published = 0;
    uint16_t drained = 0;

    while (drained < frame.unread_words && drained < ISM6HG256X_FIFO_MAX_DRAIN_WORDS) {
        const uint16_t remaining = MIN(uint16_t(frame.unread_words - drained),
                                       uint16_t(ISM6HG256X_FIFO_MAX_DRAIN_WORDS - drained));
        const uint16_t block_words = MIN(remaining, ISM6HG256X_FIFO_BURST_WORDS);

        if (!read_fifo_words_block(block_words, now_us)) {
            break;
        }

        const uint8_t *raw_word = _fifo_buffer + 1;
        for (uint16_t i = 0; i < block_words; i++, raw_word += sizeof(RawFifoWord)) {
            if (!consume_fifo_word(frame, frame.sample, raw_word)) {
                return samples_published;
            }
            drained++;
            if (frame.tag == FifoTag::GyroNC) {
                publish_gyro_sample(frame.sample);
                samples_published++;
                frame.sample.gyro.zero();
            } else if (frame.tag == FifoTag::AccelNC) {
                publish_accel_sample(frame.sample);
                frame.sample.accel.zero();
            }
        }
    }

    if (samples_published > 0) {
        _dev->adjust_periodic_callback(periodic_handle, _backend_period_us);
    }

    return samples_published;
}

void AP_InertialSensor_ISM6HG256X::publish_gyro_sample(SampleFrame &sample)
{
    _rotate_and_correct_gyro(gyro_instance, sample.gyro);
    _notify_new_gyro_raw_sample(gyro_instance, sample.gyro);
}

void AP_InertialSensor_ISM6HG256X::publish_accel_sample(SampleFrame &sample)
{
    _rotate_and_correct_accel(accel_instance, sample.accel);
    _notify_new_accel_raw_sample(accel_instance, sample.accel);
    update_temperature();
}

void AP_InertialSensor_ISM6HG256X::check_register_monitor()
{
    _dev->set_speed(AP_HAL::Device::SPEED_LOW);
    AP_HAL::Device::checkreg reg;
    if (!_dev->check_next_register(reg)) {
        log_register_change(_dev->get_bus_id(), reg);
        _inc_accel_error_count(accel_instance);
        _inc_gyro_error_count(gyro_instance);
    }
    _dev->set_speed(AP_HAL::Device::SPEED_HIGH);
}

void AP_InertialSensor_ISM6HG256X::update_temperature()
{
    const uint32_t now_ms = AP_HAL::millis();
    if (now_ms - _temperature_last_ms < ISM6HG256X_TEMPERATURE_UPDATE_MS) {
        return;
    }
    _temperature_last_ms = now_ms;

    uint8_t tbuf[2];
    if (!read_registers(ISM6HG256X_REG_OUT_TEMP_L, tbuf, sizeof(tbuf))) {
        _inc_accel_error_count(accel_instance);
        return;
    }
    const int16_t temperature_raw = int16_t(uint16_t(tbuf[0] | (tbuf[1] << 8)));
    const float temp_degc = ISM6HG256X_TEMPERATURE_ZERO_C + temperature_raw / ISM6HG256X_TEMPERATURE_SENSITIVITY;
    _publish_temperature(accel_instance, temp_degc);
}

void AP_InertialSensor_ISM6HG256X::poll_data()
{
    drain_fifo(AP_HAL::micros());
    check_register_monitor();
}
