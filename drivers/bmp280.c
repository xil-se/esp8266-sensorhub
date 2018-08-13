/*
"THE BEER/MATE-WARE LICENSE":
<xil@xil.se> wrote this file. As long as you retain this notice you
can do whatever you want with this stuff. If we meet some day, and you think
this stuff is worth it, you can buy us a ( > 0 ) beer/mate in return - The Xil TEAM
*/

#include <stdint.h>
#include <stdbool.h>

#include "ets_sys.h"
#include "osapi.h"

#include "drivers/i2c_master.h"
#include "drivers/bmp280.h"

#define I2C_ADDRESS_WRITE(bmp280)      \
    (0xEC | (bmp280->sdo_high ? 1 << 1 : 0) | 0)
#define I2C_ADDRESS_READ(bmp280)       \
    (0xEC | (bmp280->sdo_high ? 1 << 1 : 0) | 1)

#define BMP280_REG_ID               0xD0
#define BMP280_REG_CTRL_MEAS        0xF4
#define BMP280_REG_STATUS           0xF3

#define BMP280_REG_TEMP_MSB         0xFA
#define BMP280_REG_TEMP_LSB         0xFB
#define BMP280_REG_TEMP_XLSB        0xFC

#define BMP280_REG_PRESS_MSB        0xF7
#define BMP280_REG_PRESS_LSB        0xF8
#define BMP280_REG_PRESS_XLSB       0xF9

#define BMP280_ID                   0x58

#define BMP280_POWER_MODE_SLEEP     0x00
#define BMP280_POWER_MODE_FORCED    0x01
#define BMP280_POWER_MODE_NORMAL    0x11

static uint16_t ICACHE_FLASH_ATTR byteswap16(uint16_t u16)
{
    uint8_t* u8;

    u8 = (uint8_t*)&u16;

    return (u8[0] << 8) | (u8[1] << 0);
}

static int32_t ICACHE_FLASH_ATTR read24_int(bmp280_data* bmp280, int reg, bool b24)
{
    uint8_t ack;
    uint8_t val[3] = {0};

    i2c_master_start(bmp280->i2c);

    i2c_master_writeByte(bmp280->i2c, I2C_ADDRESS_WRITE(bmp280));
    ack = i2c_master_getAck(bmp280->i2c);
    if (ack != 0) {
        goto out;
    }

    i2c_master_writeByte(bmp280->i2c, reg);
    ack = i2c_master_getAck(bmp280->i2c);
    if (ack != 0) {
        goto out;
    }

    i2c_master_stop(bmp280->i2c);

    i2c_master_start(bmp280->i2c);

    i2c_master_writeByte(bmp280->i2c, I2C_ADDRESS_READ(bmp280));
    ack = i2c_master_getAck(bmp280->i2c);
    if (ack != 0) {
        goto out;
    }

    // MSB
    val[0] = i2c_master_readByte(bmp280->i2c);
    i2c_master_send_ack(bmp280->i2c);

    // LSB
    val[1] = i2c_master_readByte(bmp280->i2c);
    if (b24) {
        i2c_master_send_ack(bmp280->i2c);

        // XLSB
        val[2] = i2c_master_readByte(bmp280->i2c);
    }
    i2c_master_send_nack(bmp280->i2c);

out:
    i2c_master_stop(bmp280->i2c);

    if (b24)
        return (val[0] << 16) | (val[1] << 8) | val[2];
    else
        return (val[0] << 8) | val[1];
}

static int16_t ICACHE_FLASH_ATTR read16(bmp280_data* bmp280, int reg)
{
    return read24_int(bmp280, reg, false);
}

static int32_t ICACHE_FLASH_ATTR read24(bmp280_data* bmp280, int reg)
{
    return read24_int(bmp280, reg, true);
}

static int16_t ICACHE_FLASH_ATTR bmp280_read_temp(bmp280_data* bmp280)
{
    int16_t     temp;
    int32_t     temp_raw;

    int32_t     var1;
    int32_t     var2;

    if (!bmp280->initilized) {
        return 0;
    }

    // 20 bit precison, skip last 4
    temp_raw = read24(bmp280, BMP280_REG_TEMP_MSB) >> 4;

    var1 = ((((temp_raw >> 3) - ((int32_t)bmp280->dig_T1 << 1))) * ((int32_t)bmp280->dig_T2)) >> 11;
    var2 = (((((temp_raw >> 4) - ((int32_t)bmp280->dig_T1)) * ((temp_raw >> 4) - ((int32_t)bmp280->dig_T1))) >> 12) * ((int32_t)bmp280->dig_T3)) >> 14;

    bmp280->t_fine = var1 + var2;

    // temp will be in 0.01C
    temp = (bmp280->t_fine * 5 + 128) >> 8;

    // return value in 0.1C
    return temp / 10;
}

static int32_t ICACHE_FLASH_ATTR bmp280_read_pressure(bmp280_data* bmp280)
{
    int64_t     pressure;
    int32_t     pressure_raw;

    int64_t     var1;
    int64_t     var2;

    // 20 bit precison, skip last 4
    pressure_raw = read24(bmp280, BMP280_REG_PRESS_MSB) >> 4;

    var1 = ((int64_t)bmp280->t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)bmp280->dig_P6;
    var2 = var2 + ((var1 * (int64_t)bmp280->dig_P5) << 17);
    var2 = var2 + (((int64_t)bmp280->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)bmp280->dig_P3) >> 8) +
           ((var1 * (int64_t)bmp280->dig_P2) << 12);
    var1 = ((((int64_t)1) << 47) + var1) * ((int64_t)bmp280->dig_P1) >> 33;

    if (var1 == 0)
    {
        return 0;
    }

    pressure = 1048576 - pressure_raw;
    pressure = (((pressure << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)bmp280->dig_P9) * (pressure >> 13) * (pressure >> 13)) >> 25;
    var2 = (((int64_t)bmp280->dig_P8) * pressure) >> 19;

    // pressure will be in Pa, fixed point 24.8
    pressure = ((pressure + var1 + var2) >> 8) + (((int64_t)bmp280->dig_P7) << 4);

    // return value in Pa
    return (int32_t)(pressure >> 8);
}

static bool ICACHE_FLASH_ATTR bmp280_start_measure(bmp280_data* bmp280)
{
    uint8_t     ack;
    uint8_t     value;
    uint8_t     status;
    bool        rc = false;

    int         counter = 100;

    if (!bmp280->initilized) {
        return false;
    }

    value = ((0x07 & (bmp280->temp_oversampling + 1)) << 5) |
            ((0x07 & (bmp280->pressure_oversampling + 1)) << 2) |
            ((0x03 & BMP280_POWER_MODE_FORCED) << 0);

    i2c_master_start(bmp280->i2c);

    i2c_master_writeByte(bmp280->i2c, I2C_ADDRESS_WRITE(bmp280));
    ack = i2c_master_getAck(bmp280->i2c);
    if (ack != 0) {
        goto out;
    }

    i2c_master_writeByte(bmp280->i2c, BMP280_REG_CTRL_MEAS);
    ack = i2c_master_getAck(bmp280->i2c);
    if (ack != 0) {
        goto out;
    }

    i2c_master_writeByte(bmp280->i2c, value);
    ack = i2c_master_getAck(bmp280->i2c);
    if (ack != 0) {
        goto out;
    }

    i2c_master_stop(bmp280->i2c);

    do {
        os_delay_us(10);

        i2c_master_start(bmp280->i2c);

        i2c_master_writeByte(bmp280->i2c, I2C_ADDRESS_WRITE(bmp280));
        ack = i2c_master_getAck(bmp280->i2c);
        if (ack != 0) {
            goto out;
        }

        i2c_master_writeByte(bmp280->i2c, BMP280_REG_STATUS);
        ack = i2c_master_getAck(bmp280->i2c);
        if (ack != 0) {
            goto out;
        }

        i2c_master_stop(bmp280->i2c);

        i2c_master_start(bmp280->i2c);

        i2c_master_writeByte(bmp280->i2c, I2C_ADDRESS_READ(bmp280));
        ack = i2c_master_getAck(bmp280->i2c);
        if (ack != 0) {
            goto out;
        }

        status = i2c_master_readByte(bmp280->i2c);
        i2c_master_send_nack(bmp280->i2c);

        i2c_master_stop(bmp280->i2c);

    } while ((status & 0x09) != 0 && counter-- > 0);

    if (counter > 0) {
        rc = true;
    }

out:
    return rc;
}

bool ICACHE_FLASH_ATTR bmp280_read(driver_params* params)
{
    bmp280_data*    bmp280  = &params->bmp280;

    bool            status;

    status = bmp280_start_measure(bmp280);

    if (status == true) {
        bmp280->temperature = bmp280_read_temp(bmp280);
        bmp280->pressure    = bmp280_read_pressure(bmp280);
    } else {
        bmp280->temperature = 0;
        bmp280->pressure = 0;
    }

    return true;
}

bool ICACHE_FLASH_ATTR bmp280_print(driver_params* params, driver_print print, driver_print_data* data)
{
    bmp280_data*    bmp280  = &params->bmp280;

    print(data->data, data->index, data->i, "temp", &bmp280->temperature, sizeof(bmp280->temperature));
    print(data->data, data->index, data->i, "pressure", &bmp280->pressure, sizeof(bmp280->pressure));
}

bool ICACHE_FLASH_ATTR bmp280_init(driver_params* params, driver_bus* bus)
{
    bmp280_data*    bmp280  = &params->bmp280;
    i2c_data*       i2c     = &bus->params.i2c;

    uint8_t         ack;
    uint8_t         id      = 0;

    bmp280->i2c = i2c;

    i2c_master_start(bmp280->i2c);

    i2c_master_writeByte(bmp280->i2c, I2C_ADDRESS_WRITE(bmp280));
    ack = i2c_master_getAck(bmp280->i2c);
    if (ack != 0) {
        return false;
    }

    i2c_master_writeByte(bmp280->i2c, BMP280_REG_ID);
    ack = i2c_master_getAck(bmp280->i2c);
    if (ack != 0) {
        return false;
    }

    i2c_master_stop(bmp280->i2c);

    i2c_master_start(bmp280->i2c);

    i2c_master_writeByte(bmp280->i2c, I2C_ADDRESS_READ(bmp280));
    ack = i2c_master_getAck(bmp280->i2c);
    if (ack != 0) {
        return false;
    }

    id = i2c_master_readByte(bmp280->i2c);
    i2c_master_send_nack(bmp280->i2c);

    i2c_master_stop(bmp280->i2c);

    if (id != BMP280_ID) {
        return false;
    }

    // Registers are LSB/MSB
    bmp280->dig_T1 = byteswap16(read16(bmp280, 0x88));
    bmp280->dig_T2 = byteswap16(read16(bmp280, 0x8A));
    bmp280->dig_T3 = byteswap16(read16(bmp280, 0x8C));
    bmp280->dig_P1 = byteswap16(read16(bmp280, 0x8E));
    bmp280->dig_P2 = byteswap16(read16(bmp280, 0x90));
    bmp280->dig_P3 = byteswap16(read16(bmp280, 0x92));
    bmp280->dig_P4 = byteswap16(read16(bmp280, 0x94));
    bmp280->dig_P5 = byteswap16(read16(bmp280, 0x96));
    bmp280->dig_P6 = byteswap16(read16(bmp280, 0x98));
    bmp280->dig_P7 = byteswap16(read16(bmp280, 0x9A));
    bmp280->dig_P8 = byteswap16(read16(bmp280, 0x9C));
    bmp280->dig_P9 = byteswap16(read16(bmp280, 0x9E));

    bmp280->initilized = true;

    return true;
}

// Global variable with function pointers
const driver_sensor const sensor_bmp280 = {
    .init       = bmp280_init,
    .read       = bmp280_read,
    .print      = bmp280_print,
};

