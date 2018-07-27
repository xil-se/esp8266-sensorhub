/*
"THE BEER/MATE-WARE LICENSE":
<xil@xil.se> wrote this file. As long as you retain this notice you
can do whatever you want with this stuff. If we meet some day, and you think
this stuff is worth it, you can buy us a ( > 0 ) beer/mate in return - The Xil TEAM
*/

#ifndef BMP180_H
#define BMP180_H

#include <stdint.h>
#include <stdbool.h>

#include "drivers/i2c_master.h"

typedef struct {
    bool        initilized;

    // calibration data
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

    // busses
    i2c_data*   i2c;
} bmp180_data;


bool bmp180_init(bmp180_data* bmp180, i2c_data* i2c);

int16_t bmp180_read_temp(bmp180_data* bmp180);

/* Pressure is dependent on a fairly updated temperature reading */
int32_t bmp180_read_pressure(bmp180_data* bmp180);

#endif // BMP180_H
