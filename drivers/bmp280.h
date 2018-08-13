/*
"THE BEER/MATE-WARE LICENSE":
<xil@xil.se> wrote this file. As long as you retain this notice you
can do whatever you want with this stuff. If we meet some day, and you think
this stuff is worth it, you can buy us a ( > 0 ) beer/mate in return - The Xil TEAM
*/

#ifndef BMP280_H
#define BMP280_H

#include <stdint.h>
#include <stdbool.h>

#include "drivers/drivers.h"
#include "drivers/i2c_master.h"

typedef struct {
    // Configuration
    bool        sdo_high;
    uint8_t     temp_oversampling;      // 2^temp_oversampling, max temp_oversampling is 5 (ei x16)
    uint8_t     pressure_oversampling;  // 2^pressure_oversampling, max pressure_oversampling is 5 (ei x16)

    // Internal data
    bool        initilized;

    // calibration data
    uint16_t    dig_T1;
    int16_t     dig_T2;
    int16_t     dig_T3;
    uint16_t    dig_P1;
    int16_t     dig_P2;
    int16_t     dig_P3;
    int16_t     dig_P4;
    int16_t     dig_P5;
    int16_t     dig_P6;
    int16_t     dig_P7;
    int16_t     dig_P8;
    int16_t     dig_P9;

    int32_t     t_fine;

    // busses
    i2c_data*   i2c;

    // collected data
    int16_t     temperature;
    int32_t     pressure;
} bmp280_data;

#endif // BMP280_H
