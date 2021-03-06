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

#include "drivers/drivers.h"
#include "drivers/i2c_master.h"

typedef enum {
    bmp180_pressure_sampling_accuracy_ultra_low_power           = 0,
    bmp180_pressure_sampling_accuracy_standard                  = 1,
    bmp180_pressure_sampling_accuracy_high_resolution           = 2,
    bmp180_pressure_sampling_accuracy_ultra_high_resolution     = 3,
} bmp180_pressure_sampling_accuracy_t;



typedef struct {
    // Configuration
    bmp180_pressure_sampling_accuracy_t     pressure_sampling_accuracy;

    // Internal data
    bool        initilized;

    // Calibration data
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

    // Busses
    i2c_data*   i2c;

    // Collected data
    int16_t     temperature;
    int32_t     pressure;
} bmp180_data;

#endif // BMP180_H
