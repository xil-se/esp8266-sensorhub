
#include <stdint.h>

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

static struct {
    bool        initialized;
    int16_t     ac1;
    int16_t     ac2;
    int16_t     ac3;
    uint16_t    ac4;
    uint16_t    ac5;
    uint16_t    ac6;
    int16_t     b1;
    int16_t     b2;
    int16_t     mb;
    int16_t     mc;
    int16_t     md;
    int32_t     b5;
} bmp180_data  = {
    .initialized        = false,
};

static int32_t read24_int(int reg, bool b24)
{
    uint8_t ack;
    uint8_t val[3] = {0};

    i2c_master_start();

    i2c_master_writeByte(I2C_ADDRESS_WRITE);
    ack = i2c_master_getAck();
    if (ack != 0) {
        goto out;
    }

    i2c_master_writeByte(reg);
    ack = i2c_master_getAck();
    if (ack != 0) {
        goto out;
    }

    i2c_master_stop();

    i2c_master_start();

    i2c_master_writeByte(I2C_ADDRESS_READ);
    ack = i2c_master_getAck();
    if (ack != 0) {
        goto out;
    }

    // MSB
    val[0] = i2c_master_readByte();
    i2c_master_send_ack();

    // LSB
    val[1] = i2c_master_readByte();
    if (b24) {
        i2c_master_send_ack();

        // XLSB
        val[2] = i2c_master_readByte();
    }
    i2c_master_send_nack();

out:
    i2c_master_stop();

    if (b24)
        return (val[0] << 16) | (val[1] << 8) | val[2];
    else
        return (val[0] << 8) | val[1];
}

static int16_t read16(int reg)
{
    return read24_int(reg, false);
}

static int32_t read24(int reg)
{
    return read24_int(reg, true);
}

int16_t bmp180_read_temp_raw(void)
{
    uint8_t ack;

    i2c_master_start();

    i2c_master_writeByte(I2C_ADDRESS_WRITE);
    ack = i2c_master_getAck();
    if (ack != 0) {
        goto out;
    }

    i2c_master_writeByte(BMP180_REG_CONTROL);
    ack = i2c_master_getAck();
    if (ack != 0) {
        goto out;
    }

    i2c_master_writeByte(BMP180_TEMPERATURE);
    ack = i2c_master_getAck();
    if (ack != 0) {
        goto out;
    }

    i2c_master_stop();

    os_delay_us(BMP180_CONVERSION_TIME_TEMERATURE);

    return read16(BMP180_REG_MSB);

out:
    return 0;
}

int32_t bmp180_read_pressure_raw(uint8_t oss)
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

    i2c_master_start();

    i2c_master_writeByte(I2C_ADDRESS_WRITE);
    ack = i2c_master_getAck();
    if (ack != 0) {
        goto out;
    }

    i2c_master_writeByte(BMP180_REG_CONTROL);
    ack = i2c_master_getAck();
    if (ack != 0) {
        goto out;
    }

    i2c_master_writeByte(reg);

    ack = i2c_master_getAck();
    if (ack != 0) {
        goto out;
    }

    i2c_master_stop();

    os_delay_us(delay);

    return read24(BMP180_REG_MSB) >> (8 - oss);

out:
    return 0;
}

uint16_t bmp180_read_temp(void)
{
    int16_t    temp;
    int32_t    x1;
    int32_t    x2;
    int32_t    b5;

    if (!bmp180_data.initialized) {
        return 0;
    }

    temp = bmp180_read_temp_raw();

    x1 = (temp - bmp180_data.ac6) * bmp180_data.ac5 / (2<<14);
    x2 = bmp180_data.mc * (2<<10) / (x1 + bmp180_data.md);
    b5 = x1 + x2;

    temp = (b5 + 8) / (2<<3);

    return temp;
}

int32_t bmp180_read_pressure(void)
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

    uint8_t     oss = 0;

    if (!bmp180_data.initialized) {
        return 0;
    }

    pressure = bmp180_read_pressure_raw(oss);

    b6 = bmp180_data.b5 - 4000;
    x1 = ((int32_t)bmp180_data.b2 * ((b6 * b6) >> 12)) >> 11;
    x2 = ((int32_t)bmp180_data.ac2 * b6) >> 11;
    x3 = x1 + x2;
    b3 = (((((int32_t)bmp180_data.ac1 << 2) + x3) << oss) + 2) >> 2;
    x1 = ((int32_t)bmp180_data.ac3 * b6) >> 13;
    x2 = (int32_t)(bmp180_data.b1 * ((b6 * b6) >> 12)) >> 16;
    x3 = (x1 + x2 + 2) >> 2;
    b4 = ((uint32_t)bmp180_data.ac4 * ((uint32_t)(x3 + 32768)) >> 15);
    b7 = (pressure - b3) * (50000 >> oss);
    p = b7 < 0x80000000 ? (b7 << 1) / b4 : (b7 / b4) << 1;
    x1 = (p >> 8) * (p >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * p) >> 16;
    p = p + ((x1 + x2 + 3791) >> 4);

    return p;
}

bool bmp180_init(void)
{
    uint8_t ack;
    uint8_t id = 0;

    i2c_master_start();

    i2c_master_writeByte(I2C_ADDRESS_WRITE);
    ack = i2c_master_getAck();
    if (ack != 0) {
        return false;
    }

    i2c_master_writeByte(BMP180_REG_ID);
    ack = i2c_master_getAck();
    if (ack != 0) {
        return false;
    }

    i2c_master_stop();

    i2c_master_start();

    i2c_master_writeByte(I2C_ADDRESS_READ);
    ack = i2c_master_getAck();
    if (ack != 0) {
        return false;
    }

    id = i2c_master_readByte();
    i2c_master_send_nack();

    if (id != BMP180_ID) {
        return false;
    }

    bmp180_data.ac1 = read16(0xAA);
    bmp180_data.ac2 = read16(0xAC);
    bmp180_data.ac3 = read16(0xAE);
    bmp180_data.ac4 = read16(0xB0);
    bmp180_data.ac5 = read16(0xB2);
    bmp180_data.ac6 = read16(0xB4);
    bmp180_data.b1  = read16(0xB6);
    bmp180_data.b2  = read16(0xB8);
    bmp180_data.mb  = read16(0xBA);
    bmp180_data.mc  = read16(0xBC);
    bmp180_data.md  = read16(0xBE);

    bmp180_data.initialized = true;

    return true;
}
