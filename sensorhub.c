/*
"THE BEER/MATE-WARE LICENSE":
<xil@xil.se> wrote this file. As long as you retain this notice you
can do whatever you want with this stuff. If we meet some day, and you think
this stuff is worth it, you can buy us a ( > 0 ) beer/mate in return - The Xil TEAM
*/

#include "osapi.h"

#include "os_type.h"
#include "ets_sys.h"

#include "user_interface.h"
#include "espconn.h"

#include "gpio.h"

#include "network_config.h"

#include "drivers/drivers.h"

#define SENSORHUB_VERSION           3

#define SENSOR_TASK_PRIO            2
#define SENSOR_TASK_QUEUE_SIZE      1

static const uint8_t RECEIVER[4] = RECEIVER_IP;

#define POLL_TIME   60000 // ms
#define MAX_RESPONSE_SIZE           256 // bytes

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

#define NETWORK_MODE_POLLING
//#define NETWORK_MODE_PUSHING

static struct {
    struct espconn      conn_tcp;
    struct espconn      conn_udp;
    esp_tcp             tcp;
    esp_udp             udp;
} network;

// Data bus configuration
static driver_bus       driver_buses[] = {
    { .bus = &bus_i2c, .params.i2c.gpio_sda = 13, .params.i2c.gpio_scl = 12, },
    { .bus = &bus_i2c, .params.i2c.gpio_sda =  5, .params.i2c.gpio_scl =  4, },
};

// Sensor driver configuration
static driver           driver_sensors[] = {
    { .bus = &driver_buses[0], .sensor = &sensor_bmp180,
                               .params.bmp180.pressure_sampling_accuracy = bmp180_pressure_sampling_accuracy_high_resolution, },
    { .bus = &driver_buses[1], .sensor = &sensor_bmp280,
                               .params.bmp280.sdo_high = false,
                               .params.bmp280.temp_oversampling = 1,
                               .params.bmp280.pressure_oversampling = 2, },
};

static struct {
    int                 cnt;
    os_event_t          queue[SENSOR_TASK_QUEUE_SIZE];
    volatile os_timer_t timer;
} sensor;

static void ICACHE_FLASH_ATTR write_value(char* string, void* value, int size)
{
    uint8_t* v = (uint8_t*)value;
    int i = 0;

    while (i < size) {
        switch (v[size - i - 1] & 0xf0) {
            case 0x00: string[(i<<1) + 0] = '0'; break;
            case 0x10: string[(i<<1) + 0] = '1'; break;
            case 0x20: string[(i<<1) + 0] = '2'; break;
            case 0x30: string[(i<<1) + 0] = '3'; break;
            case 0x40: string[(i<<1) + 0] = '4'; break;
            case 0x50: string[(i<<1) + 0] = '5'; break;
            case 0x60: string[(i<<1) + 0] = '6'; break;
            case 0x70: string[(i<<1) + 0] = '7'; break;
            case 0x80: string[(i<<1) + 0] = '8'; break;
            case 0x90: string[(i<<1) + 0] = '9'; break;
            case 0xA0: string[(i<<1) + 0] = 'A'; break;
            case 0xB0: string[(i<<1) + 0] = 'B'; break;
            case 0xC0: string[(i<<1) + 0] = 'C'; break;
            case 0xD0: string[(i<<1) + 0] = 'D'; break;
            case 0xE0: string[(i<<1) + 0] = 'E'; break;
            case 0xF0: string[(i<<1) + 0] = 'F'; break;
            default:   string[(i<<1) + 0] = ' '; break;
        }

        switch (v[size - i - 1] & 0x0f) {
            case 0x00: string[(i<<1) + 1] = '0'; break;
            case 0x01: string[(i<<1) + 1] = '1'; break;
            case 0x02: string[(i<<1) + 1] = '2'; break;
            case 0x03: string[(i<<1) + 1] = '3'; break;
            case 0x04: string[(i<<1) + 1] = '4'; break;
            case 0x05: string[(i<<1) + 1] = '5'; break;
            case 0x06: string[(i<<1) + 1] = '6'; break;
            case 0x07: string[(i<<1) + 1] = '7'; break;
            case 0x08: string[(i<<1) + 1] = '8'; break;
            case 0x09: string[(i<<1) + 1] = '9'; break;
            case 0x0A: string[(i<<1) + 1] = 'A'; break;
            case 0x0B: string[(i<<1) + 1] = 'B'; break;
            case 0x0C: string[(i<<1) + 1] = 'C'; break;
            case 0x0D: string[(i<<1) + 1] = 'D'; break;
            case 0x0E: string[(i<<1) + 1] = 'E'; break;
            case 0x0F: string[(i<<1) + 1] = 'F'; break;
            default:   string[(i<<1) + 1] = ' '; break;
        }
        i++;
    }
}

static bool ICACHE_FLASH_ATTR add_value(char* data, uint16_t* index, int i, const char* string, void* value, int size)
{
    int length;

    length = os_strlen(string);
    memcpy(&data[*index], string, length);
    (*index) += length;

    data[*index] = '.';
    (*index)++;

    length = sizeof(uint8_t);
    write_value(&data[*index], &i, length);
    (*index) += length*2;

    data[*index] = ':';
    (*index)++;

    data[*index] = ' ';
    (*index)++;

    length = size;
    write_value(&data[*index], value, length);
    (*index) += length*2;

    data[*index] = '\n';
    (*index)++;

    return true;
}

static bool ICACHE_FLASH_ATTR add_string(char* data, uint16_t* index, int i, const char* string, const char* value)
{
    int length;

    length = os_strlen(string);
    memcpy(&data[*index], string, length);
    (*index) += length;

    data[*index] = '.';
    (*index)++;

    length = sizeof(uint8_t);
    write_value(&data[*index], &i, length);
    (*index) += length*2;

    data[*index] = ':';
    (*index)++;

    data[*index] = ' ';
    (*index)++;

    length = os_strlen(value);
    memcpy(&data[*index], value, length);
    (*index) += length;

    data[*index] = '\n';
    (*index)++;

    return true;
}

static void ICACHE_FLASH_ATTR build_result(char* data, int* length)
{
    uint16_t            index;
    uint8_t             i;
    uint16_t            version;
    driver_print_data   print_data;

    print_data.data = data;
    print_data.index = &index;
    print_data.print_value = add_value;
    print_data.print_string = add_string;

    index = 0;

    version = SENSORHUB_VERSION;
    add_value(data, &index, 0, "version", &version, sizeof(version));

    for (i = 0; i < ARRAY_SIZE(driver_buses); i++) {
        print_data.i = i;
        driver_buses[i].bus->print(&driver_buses[i].params, &print_data);
    }

    for (i = 0; i < ARRAY_SIZE(driver_sensors); i++) {
        print_data.i = i;
        driver_sensors[i].sensor->print(&driver_sensors[i].params, &print_data);
    }

    if (index > *length)
    {
        index = 0;
        add_value(data, &index, 0, "ERROR: Buffer too small", length, sizeof(*length));
    }

    *length = index;
}

void ICACHE_FLASH_ATTR send_data(void)
{
    int length;
    static char data[MAX_RESPONSE_SIZE] = "";

    length = sizeof(data);
    build_result(data, &length);

    // Set ip
    os_memcpy(network.conn_udp.proto.udp->remote_ip, RECEIVER, sizeof(network.conn_udp.proto.udp->remote_ip));
    // Set port
    network.conn_udp.proto.udp->remote_port = RECEIVER_PORT;

    espconn_sendto(&network.conn_udp, data, length);
}

void ICACHE_FLASH_ATTR init_udp(void)
{
    network.conn_udp.proto.udp = &network.udp;

    // TCP connection
    network.conn_udp.type = ESPCONN_UDP;
    // No state
    network.conn_udp.state = ESPCONN_NONE;
    // Set ip
    os_memcpy(network.conn_udp.proto.udp->remote_ip, RECEIVER, sizeof(network.conn_udp.proto.udp->remote_ip));
    // Set port
    network.conn_udp.proto.udp->remote_port = RECEIVER_PORT;
    network.conn_udp.proto.udp->local_port = espconn_port();

    espconn_create(&network.conn_udp);
}

static void ICACHE_FLASH_ATTR tcp_connect_cb(void* arg)
{
    struct espconn* conn;

    int length;
    static char data[MAX_RESPONSE_SIZE] = "";

    length = sizeof(data);
    build_result(data, &length);

    conn = (struct espconn*)arg;

    espconn_send(conn, data, length);

    espconn_disconnect(conn);
}

static void ICACHE_FLASH_ATTR start_network_services(void)
{
    sint8   rc;

    network.conn_tcp.proto.tcp = &network.tcp;

    // TCP connection
    network.conn_tcp.type = ESPCONN_TCP;
    // No state
    network.conn_tcp.state = ESPCONN_NONE;
    // Set port
    network.conn_tcp.proto.tcp->local_port = LISTENING_PORT;

    // Register callbacks
    espconn_regist_connectcb(&network.conn_tcp, tcp_connect_cb);
    // espconn_regist_reconcb(&network.conn_tcp, tcp_reconnect_cb);
    // espconn_regist_disconcb(&network.conn_tcp, tcp_disconnect_cb);

    rc = espconn_accept(&network.conn_tcp);
    if (rc != ESPCONN_OK) {
        // handle error
        os_printf("Error opening listening socket: %d\n", rc);
    }
}

static void ICACHE_FLASH_ATTR set_wifi_config(void)
{
    struct station_config config;

    os_memset(&config, 0, sizeof(config));

    // Connect to any AP
    config.bssid_set = 0;

    os_memcpy(&config.ssid, WIFI_SSID, os_strlen(WIFI_SSID));
    os_memcpy(&config.password, WIFI_PSK, os_strlen(WIFI_PSK));
    wifi_station_set_config(&config);
}

static void ICACHE_FLASH_ATTR sensor_timer(void *arg)
{
    system_os_post(SENSOR_TASK_PRIO, 0, (os_param_t)NULL);
}

static void ICACHE_FLASH_ATTR sensor_task(os_event_t* event)
{
    int     i;

    sensor.cnt++;

    // Read all sensors
    for (i = 0; i < ARRAY_SIZE(driver_sensors); i++) {
        driver_sensors[i].sensor->read(&driver_sensors[i].params);
    }

#ifdef NETWORK_MODE_PUSHING
    send_data();
#endif // NETWORK_MODE_PUSHING
}

void ICACHE_FLASH_ATTR user_init(void)
{
    int     i;

    os_memset(&sensor, 0, sizeof(sensor));

    wifi_set_opmode(STATION_MODE);
    set_wifi_config();

#ifdef NETWORK_MODE_POLLING
    start_network_services();
#endif // NETWORK_MODE_POLLING

#ifdef NETWORK_MODE_PUSHING
    init_udp();
#endif // NETWORK_MODE_PUSHING

    // configure busses
    for (i = 0; i < ARRAY_SIZE(driver_buses); i++) {
        driver_buses[i].bus->init(&driver_buses[i].params);
    }

    // initialize sensors
    for (i = 0; i < ARRAY_SIZE(driver_sensors); i++) {
        driver_sensors[i].sensor->init(&driver_sensors[i].params, driver_sensors[i].bus);
    }

    // task for sensors
    system_os_task(sensor_task, SENSOR_TASK_PRIO, sensor.queue, SENSOR_TASK_QUEUE_SIZE);
    system_os_post(SENSOR_TASK_PRIO, 0, (os_param_t)NULL);

    // timer for sensors
    os_timer_setfn(&sensor.timer, (os_timer_func_t *)sensor_timer, NULL);
    os_timer_arm(&sensor.timer, POLL_TIME, 1);

    system_set_os_print(1);
    system_print_meminfo();
    system_set_os_print(0);
}
