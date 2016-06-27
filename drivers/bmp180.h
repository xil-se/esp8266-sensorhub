#ifndef BMP180_H
#define BMP180_H

#include <stdbool.h>

#include "drivers/i2c_master.h"

bool bmp180_init(i2c_data* i2c);
uint16_t bmp180_read_temp(void);

/* Pressure is dependent on a fairly updated temperature reading */
int32_t bmp180_read_pressure(void);

#endif // BMP180_H
