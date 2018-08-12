#ifndef DRIVERS_H
#define DRIVERS_H

// Forward declare driver_bus for the drivers
typedef struct driver_bus           driver_bus;
typedef union driver_bus_params     driver_bus_params;

#include "drivers/i2c_master.h"

typedef void (*driver_bus_init)(driver_bus_params* params);
//#define void (driver_read*)(driver_param* params)

union driver_bus_params {
    i2c_data        i2c;
//    spi_data        spi;
};

struct driver_bus {
    driver_bus_params       params;
    driver_bus_init         init;
};

#endif // DRIVERS_H
