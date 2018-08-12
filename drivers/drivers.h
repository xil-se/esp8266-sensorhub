#ifndef DRIVERS_H
#define DRIVERS_H

#include <stdbool.h>

// Forward declare driver_bus for the drivers
typedef union driver_bus_params     driver_bus_params;
typedef struct driver_bus           driver_bus;
typedef union driver_params         driver_params;
typedef struct driver               driver;

typedef void (*driver_bus_init)(driver_bus_params* params);
typedef bool (*driver_init)(driver_params* params, driver_bus* bus);
typedef bool (*driver_read)(driver_params* params);

typedef struct driver_sensor {
    const driver_init       init;
    const driver_read       read;
} driver_sensor;

#include "drivers/i2c_master.h"
#include "drivers/bmp180.h"

// Data buses

union driver_bus_params {
    i2c_data        i2c;
//    spi_data        spi;
};

struct driver_bus {
    driver_bus_params       params;
    driver_bus_init         init;
};

// Sensor drivers

union driver_params {
    bmp180_data             bmp180;
};

struct driver {
    driver_bus*             bus;
    driver_params           params;
    const driver_sensor*    sensor;
};

#endif // DRIVERS_H
