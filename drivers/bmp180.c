/*
"THE BEER/MATE-WARE LICENSE":
<xil@xil.se> wrote this file. As long as you retain this notice you
can do whatever you want with this stuff. If we meet some day, and you think
this stuff is worth it, you can buy us a ( > 0 ) beer/mate in return - The Xil TEAM
*/

#include <stdint.h>

#include "ets_sys.h"
#include "osapi.h"

#include "drivers/i2c_master.h"

#include "drivers/bmp180.h"

#define I2C_ADDRESS_WRITE           0xEE
#define I2C_ADDRESS_READ            0xEF

#define BMP180_REG_ID               0xD0
#define BMP180_REG_CONTROL          0xF4

#define BMP180_REG_MSB              0xF6
#define BMP180_REG_LSB              0xF7
#define BMP180_REG_XLSB             0xF8

#define BMP180_ID                   0x55

#define BMP180_TEMPERATURE          0x2E
#define BMP180_PRESSURE_OSS0        0x34
#define BMP180_PRESSURE_OSS1        0x74
#define BMP180_PRESSURE_OSS2        0xB4
#define BMP180_PRESSURE_OSS3        0xF4

#define BMP180_CONVERSION_TIME_TEMERATURE     4500    // us
#define BMP180_CONVERSION_TIME_PRESSURE_OSS0  4500    // us
#define BMP180_CONVERSION_TIME_PRESSURE_OSS1  7500    // us
#define BMP180_CONVERSION_TIME_PRESSURE_OSS2  13500   // us
#define BMP180_CONVERSION_TIME_PRESSURE_OSS3  25500   // us

#define BMP180_PRESSURE_SAMPLING_ACCURACY_ULTRA_LOW_POWER       0
#define BMP180_PRESSURE_SAMPLING_ACCURACY_STANDARD              1
#define BMP180_PRESSURE_SAMPLING_ACCURACY_HIGH_RESOLUTION       2
#define BMP180_PRESSURE_SAMPLING_ACCURACY_ULTRA_HIGH_RESOLUTION 3

// Pressure sampling accuracy mode to use
#define BMP180_PRESSURE_SAMPLING_MODE BMP180_PRESSURE_SAMPLING_ACCURACY_HIGH_RESOLUTION

static int32_t ICACHE_FLASH_ATTR read24_int(bmp180_data* bmp180, int reg, bool b24)
{
    uint8_t ack;
    uint8_t val[3] = {0};

    i2c_master_start(bmp180->i2c);

    i2c_master_writeByte(bmp180->i2c, I2C_ADDRESS_WRITE);
    ack = i2c_master_getAck(bmp180->i2c);
    if (ack != 0) {
        goto out;
    }

    i2c_master_writeByte(bmp180->i2c, reg);
    ack = i2c_master_getAck(bmp180->i2c);
    if (ack != 0) {
        goto out;
    }

    i2c_master_stop(bmp180->i2c);

    i2c_master_start(bmp180->i2c);

    i2c_master_writeByte(bmp180->i2c, I2C_ADDRESS_READ);
    ack = i2c_master_getAck(bmp180->i2c);
    if (ack != 0) {
        goto out;
    }

    // MSB
    val[0] = i2c_master_readByte(bmp180->i2c);
    i2c_master_send_ack(bmp180->i2c);

    // LSB
    val[1] = i2c_master_readByte(bmp180->i2c);
    if (b24) {
        i2c_master_send_ack(bmp180->i2c);

        // XLSB
        val[2] = i2c_master_readByte(bmp180->i2c);
    }
    i2c_master_send_nack(bmp180->i2c);

out:
    i2c_master_stop(bmp180->i2c);

    if (b24)
        return (val[0] << 16) | (val[1] << 8) | val[2];
    else
        return (val[0] << 8) | val[1];
}

static int16_t ICACHE_FLASH_ATTR read16(bmp180_data* bmp180, int reg)
{
    return read24_int(bmp180, reg, false);
}

static int32_t ICACHE_FLASH_ATTR read24(bmp180_data* bmp180, int reg)
{
    return read24_int(bmp180, reg, true);
}

int16_t ICACHE_FLASH_ATTR bmp180_read_temp_raw(bmp180_data* bmp180)
{
    uint8_t ack;

    i2c_master_start(bmp180->i2c);

    i2c_master_writeByte(bmp180->i2c, I2C_ADDRESS_WRITE);
    ack = i2c_master_getAck(bmp180->i2c);
    if (ack != 0) {
        goto out;
    }

    i2c_master_writeByte(bmp180->i2c, BMP180_REG_CONTROL);
    ack = i2c_master_getAck(bmp180->i2c);
    if (ack != 0) {
        goto out;
    }

    i2c_master_writeByte(bmp180->i2c, BMP180_TEMPERATURE);
    ack = i2c_master_getAck(bmp180->i2c);
    if (ack != 0) {
        goto out;
    }

    i2c_master_stop(bmp180->i2c);

    os_delay_us(BMP180_CONVERSION_TIME_TEMERATURE);

    return read16(bmp180, BMP180_REG_MSB);

out:
    return 0;
}

int32_t ICACHE_FLASH_ATTR bmp180_read_pressure_raw(bmp180_data* bmp180, uint8_t oss)
{
    uint8_t ack;
    int     reg;
    int     delay;

    switch (oss) {
        case 0:
            reg = BMP180_PRESSURE_OSS0;
            delay = BMP180_CONVERSION_TIME_PRESSURE_OSS0;
            break;
        case 1:
            reg = BMP180_PRESSURE_OSS1;
            delay = BMP180_CONVERSION_TIME_PRESSURE_OSS1;
            break;
        case 2:
            reg = BMP180_PRESSURE_OSS2;
            delay = BMP180_CONVERSION_TIME_PRESSURE_OSS2;
            break;
        case 3:
            reg = BMP180_PRESSURE_OSS3;
            delay = BMP180_CONVERSION_TIME_PRESSURE_OSS3;
            break;
        default:
            return 0;
    }

    i2c_master_start(bmp180->i2c);

    i2c_master_writeByte(bmp180->i2c, I2C_ADDRESS_WRITE);
    ack = i2c_master_getAck(bmp180->i2c);
    if (ack != 0) {
        goto out;
    }

    i2c_master_writeByte(bmp180->i2c, BMP180_REG_CONTROL);
    ack = i2c_master_getAck(bmp180->i2c);
    if (ack != 0) {
        goto out;
    }

    i2c_master_writeByte(bmp180->i2c, reg);
    ack = i2c_master_getAck(bmp180->i2c);
    if (ack != 0) {
        goto out;
    }

    i2c_master_stop(bmp180->i2c);

    os_delay_us(delay);

    return read24(bmp180, BMP180_REG_MSB) >> (8 - oss);

out:
    return 0;
}

int16_t ICACHE_FLASH_ATTR bmp180_read_temp(bmp180_data* bmp180)
{
    uint16_t   temp_raw;
    int16_t    temp;
    int32_t    x1;
    int32_t    x2;

    if (!bmp180->initilized) {
        return 0;
    }

    temp_raw = bmp180_read_temp_raw(bmp180);

    x1 = (temp_raw - bmp180->ac6) * bmp180->ac5 / (2<<14);
    x2 = bmp180->mc * (2<<10) / (x1 + bmp180->md);
    bmp180->b5 = x1 + x2;

    temp = (bmp180->b5 + 8) / (2<<3);

    return temp;
}

int32_t ICACHE_FLASH_ATTR bmp180_read_pressure(bmp180_data* bmp180)
{
    int32_t     pressure;

    int32_t     b6;
    int32_t     x1;
    int32_t     x2;
    int32_t     x3;
    int32_t     b3;
    uint32_t    b4;
    uint32_t    b7;
    int32_t     p;

    uint8_t     oss = BMP180_PRESSURE_SAMPLING_MODE;

    if (!bmp180->initilized) {
        return 0;
    }

    pressure = bmp180_read_pressure_raw(bmp180, oss);

    b6 = bmp180->b5 - 4000;
    x1 = ((int32_t)bmp180->b2 * ((b6 * b6) >> 12)) >> 11;
    x2 = ((int32_t)bmp180->ac2 * b6) >> 11;
    x3 = x1 + x2;
    b3 = (((((int32_t)bmp180->ac1 << 2) + x3) << oss) + 2) >> 2;
    x1 = ((int32_t)bmp180->ac3 * b6) >> 13;
    x2 = (int32_t)(bmp180->b1 * ((b6 * b6) >> 12)) >> 16;
    x3 = (x1 + x2 + 2) >> 2;
    b4 = ((uint32_t)bmp180->ac4 * ((uint32_t)(x3 + 32768)) >> 15);
    b7 = (pressure - b3) * (50000 >> oss);
    p = b7 < 0x80000000 ? (b7 << 1) / b4 : (b7 / b4) << 1;
    x1 = (p >> 8) * (p >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * p) >> 16;
    p = p + ((x1 + x2 + 3791) >> 4);

    return p;
}

bool ICACHE_FLASH_ATTR bmp180_init(bmp180_data* bmp180, i2c_data* i2c)
{
    uint8_t ack;
    uint8_t id = 0;

    bmp180->initilized = false;

    bmp180->i2c = i2c;

    i2c_master_start(bmp180->i2c);

    i2c_master_writeByte(bmp180->i2c, I2C_ADDRESS_WRITE);
    ack = i2c_master_getAck(bmp180->i2c);
    if (ack != 0) {
        return false;
    }

    i2c_master_writeByte(bmp180->i2c, BMP180_REG_ID);
    ack = i2c_master_getAck(bmp180->i2c);
    if (ack != 0) {
        return false;
    }

    i2c_master_stop(bmp180->i2c);

    i2c_master_start(bmp180->i2c);

    i2c_master_writeByte(bmp180->i2c, I2C_ADDRESS_READ);
    ack = i2c_master_getAck(bmp180->i2c);
    if (ack != 0) {
        return false;
    }

    id = i2c_master_readByte(bmp180->i2c);
    i2c_master_send_nack(bmp180->i2c);

    if (id != BMP180_ID) {
        return false;
    }

    bmp180->ac1 = read16(bmp180, 0xAA);
    bmp180->ac2 = read16(bmp180, 0xAC);
    bmp180->ac3 = read16(bmp180, 0xAE);
    bmp180->ac4 = read16(bmp180, 0xB0);
    bmp180->ac5 = read16(bmp180, 0xB2);
    bmp180->ac6 = read16(bmp180, 0xB4);
    bmp180->b1  = read16(bmp180, 0xB6);
    bmp180->b2  = read16(bmp180, 0xB8);
    bmp180->mb  = read16(bmp180, 0xBA);
    bmp180->mc  = read16(bmp180, 0xBC);
    bmp180->md  = read16(bmp180, 0xBE);

    bmp180->initilized = true;

    return true;
}
