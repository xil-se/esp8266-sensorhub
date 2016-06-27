#include "osapi.h"
#include "os_type.h"
#include "ets_sys.h"

#include "user_interface.h"
#include "espconn.h"

#include "gpio.h"

#include "wifi_config.h"

#include "drivers/bmp180.h"

#define SENSOR_TASK_PRIO           2
#define SENSOR_TASK_QUEUE_SIZE     1

#define LISTENING_PORT 8081

#define POLL_TIME   60000 // ms

typedef struct {
    int16_t             temp;
    int32_t             pressure;
} bmp180_values;

static struct {
    struct espconn      conn;
    esp_tcp             tcp;
} network;

static struct {
    int                 cnt;
    os_event_t          queue[SENSOR_TASK_QUEUE_SIZE];
    volatile os_timer_t timer;
    struct {
        bmp180_values       bmp180[1];
    } values;
    i2c_data            i2c[1];
} sensor;

void ICACHE_FLASH_ATTR sensor_timer(void *arg)
{
    system_os_post(SENSOR_TASK_PRIO, 0, (os_param_t)NULL);
}

void ICACHE_FLASH_ATTR sensor_task(os_event_t* event)
{
    uint8_t ack;

    sensor.cnt++;

    sensor.values.bmp180[0].temp = bmp180_read_temp();
    sensor.values.bmp180[0].pressure = bmp180_read_pressure();
}

void write_value(char* string, void* value, int size)
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

void tcp_connect_cb(void* arg)
{
    struct espconn* conn;

    char d[48] = "temp.0:     \n"
                 "pressure.0:         \n";

    write_value(&d[8], &sensor.values.bmp180[0].temp, sizeof(uint16_t));
    write_value(&d[25], &sensor.values.bmp180[0].pressure, sizeof(int32_t));

    conn = (struct espconn*)arg;

    espconn_send(conn, d, os_strlen(d));

    espconn_disconnect(conn);
}

void ICACHE_FLASH_ATTR start_network_services(void)
{
    sint8   rc;

    network.conn.proto.tcp = &network.tcp;

    // TCP connection
    network.conn.type = ESPCONN_TCP;
    // No state
    network.conn.state = ESPCONN_NONE;
    // Set port
    network.conn.proto.tcp->local_port = LISTENING_PORT;

    // Register callbacks
    espconn_regist_connectcb(&network.conn, tcp_connect_cb);
    // espconn_regist_reconcb(&network.conn, tcp_reconnect_cb);
    // espconn_regist_disconcb(&network.conn, tcp_disconnect_cb);

    rc = espconn_accept(&network.conn);
    if (rc != ESPCONN_OK) {
        // handle error
        os_printf("Error opening listening socket: %d\n", rc);
    }
}

void ICACHE_FLASH_ATTR set_wifi_config(void)
{
    struct station_config config;

    os_memset(&config, 0, sizeof(config));

    // Connect to any AP
    config.bssid_set = 0;

    os_memcpy(&config.ssid, WIFI_SSID, os_strlen(WIFI_SSID));
    os_memcpy(&config.password, WIFI_PSK, os_strlen(WIFI_PSK));
    wifi_station_set_config(&config);
}

void ICACHE_FLASH_ATTR user_init(void)
{
    os_memset(&sensor, 0, sizeof(sensor));

    wifi_set_opmode(STATION_MODE);
    set_wifi_config();

    start_network_services();

    i2c_master_gpio_init(&sensor.i2c[0], 13, 12);

    bmp180_init(&sensor.i2c[0]);

    system_os_task(sensor_task, SENSOR_TASK_PRIO, sensor.queue, SENSOR_TASK_QUEUE_SIZE);
    system_os_post(SENSOR_TASK_PRIO, 0, (os_param_t)NULL);

    os_timer_setfn(&sensor.timer, (os_timer_func_t *)sensor_timer, NULL);
    os_timer_arm(&sensor.timer, POLL_TIME, 1);
}
