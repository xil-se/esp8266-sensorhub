#ifndef __I2C_MASTER_H__
#define __I2C_MASTER_H__

#include <stdbool.h>

// Global structs

typedef struct {
    uint8_t gpio_sda;
    uint8_t gpio_scl;
    uint8_t sda_last;
    uint8_t scl_last;
} i2c_data;

#include "drivers/drivers.h"

#define i2c_master_wait    os_delay_us
void i2c_master_stop(i2c_data* i2c);
void i2c_master_start(i2c_data* i2c);
void i2c_master_setAck(i2c_data* i2c, uint8_t level);
uint8_t i2c_master_getAck(i2c_data* i2c);
uint8_t i2c_master_readByte(i2c_data* i2c);
void i2c_master_writeByte(i2c_data* i2c, uint8_t wrdata);

bool i2c_master_checkAck(i2c_data* i2c);
void i2c_master_send_ack(i2c_data* i2c);
void i2c_master_send_nack(i2c_data* i2c);

#endif
