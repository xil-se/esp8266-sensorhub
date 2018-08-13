/*
"THE BEER/MATE-WARE LICENSE":
<xil@xil.se> wrote this file. As long as you retain this notice you
can do whatever you want with this stuff. If we meet some day, and you think
this stuff is worth it, you can buy us a ( > 0 ) beer/mate in return - The Xil TEAM
*/

#ifndef DRIVERS_H
#define DRIVERS_H

#include <stdint.h>
#include <stdbool.h>

// Forward declare driver_bus for the drivers
typedef union driver_bus_params     driver_bus_params;
typedef struct driver_bus           driver_bus;
typedef union driver_params         driver_params;
typedef struct driver               driver;

typedef bool (*driver_print_value)(char* data, uint16_t* index, int i, const char* string, void* value, int size);
typedef bool (*driver_print_string)(char* data, uint16_t* index, int i, const char* string, const char* value);

typedef struct {
    char*               data;
    uint16_t*           index;
    int                 i;
    driver_print_value  print_value;
    driver_print_string print_string;
} driver_print_data;

typedef void (*driver_bus_init)(driver_bus_params* params);
typedef bool (*driver_bus_print)(driver_bus_params* params, driver_print_data* data);
typedef bool (*driver_sensor_init)(driver_params* params, driver_bus* bus);
typedef bool (*driver_sensor_read)(driver_params* params);
typedef bool (*driver_sensor_print)(driver_params* params, driver_print_data* data);

typedef struct driver_bus_type {
    const driver_bus_init   init;
    const driver_bus_print  print;
} driver_bus_type;

typedef struct driver_sensor {
    const driver_sensor_init    init;
    const driver_sensor_read    read;
    const driver_sensor_print   print;
} driver_sensor;

#include "drivers/i2c_master.h"
#include "drivers/bmp180.h"
#include "drivers/bmp280.h"

// Global, constant variables for function pointers
// Busses
extern const driver_bus_type const bus_i2c;
// Sensors
extern const driver_sensor const sensor_bmp180;
extern const driver_sensor const sensor_bmp280;

// Data buses

union driver_bus_params {
    i2c_data        i2c;
//    spi_data        spi;
};

struct driver_bus {
    driver_bus_params       params;
    const driver_bus_type*  bus;
};

// Sensor drivers

union driver_params {
    bmp180_data             bmp180;
    bmp280_data             bmp280;
};

struct driver {
    driver_bus*             bus;
    driver_params           params;
    const driver_sensor*    sensor;
};

#endif // DRIVERS_H
