#include "osapi.h"
#include "os_type.h"
#include "ets_sys.h"

#include "user_interface.h"
#include "espconn.h"

#include "gpio.h"

#include "wifi_config.h"

#define SENSOR_TASK_PRIO           2
#define SENSOR_TASK_QUEUE_SIZE     1

#define LISTENING_PORT 8081

static struct {
    struct espconn      conn;
    esp_tcp             tcp;
} network;

static struct {
    int                 value;
    os_event_t          queue[SENSOR_TASK_QUEUE_SIZE];
    volatile os_timer_t timer;
} sensor = {
    .value              = 0,
};

void ICACHE_FLASH_ATTR sensor_timer(void *arg)
{
    system_os_post(SENSOR_TASK_PRIO, 0, (os_param_t)NULL);
}

void ICACHE_FLASH_ATTR sensor_task(os_event_t* event)
{
    sensor.value++;
    if (sensor.value >= 10) {
        sensor.value = 0;
    }
}

void tcp_connect_cb(void* arg)
{
    struct espconn* conn;

    char d[16] = "test  \n";

    switch (sensor.value) {
        case 0: d[5] = '0'; break;
        case 1: d[5] = '1'; break;
        case 2: d[5] = '2'; break;
        case 3: d[5] = '3'; break;
        case 4: d[5] = '4'; break;
        case 5: d[5] = '5'; break;
        case 6: d[5] = '6'; break;
        case 7: d[5] = '7'; break;
        case 8: d[5] = '8'; break;
        case 9: d[5] = '9'; break;
        default: d[5] = ' '; break;
    }

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
    wifi_set_opmode(STATION_MODE);
    set_wifi_config();

    start_network_services();

    system_os_task(sensor_task, SENSOR_TASK_PRIO, sensor.queue, SENSOR_TASK_QUEUE_SIZE);
    system_os_post(SENSOR_TASK_PRIO, 0, (os_param_t)NULL);

    os_timer_setfn(&sensor.timer, (os_timer_func_t *)sensor_timer, NULL);
    os_timer_arm(&sensor.timer, 2000, 1);
}
