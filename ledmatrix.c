/*
"THE BEER/MATE-WARE LICENSE":
<xil@xil.se> wrote this file. As long as you retain this notice you
can do whatever you want with this stuff. If we meet some day, and you think
this stuff is worth it, you can buy us a ( > 0 ) beer/mate in return - The Xil TEAM
*/

#include "osapi.h"
#include "mem.h"

#include "os_type.h"
#include "ets_sys.h"

#include "user_interface.h"
#include "espconn.h"

#include "drivers/ws2801.h"

#define LEDMATRIX_TASK_PRIO           2
#define LEDMATRIX_TASK_QUEUE_SIZE     1

#define POLL_TIME   1000 // ms

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

static struct {
    os_event_t          queue[LEDMATRIX_TASK_QUEUE_SIZE];
    volatile os_timer_t timer;
    struct {
        ws2801_data_t*      ws2801;
    } values;
} ledmatrix;

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

static void ICACHE_FLASH_ATTR add_value(char* data, int* index, int i, const char* string, void* value, int size)
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
}

static void ICACHE_FLASH_ATTR build_result(char* data, int* length)
{
    int index;
    uint8_t i;

    index = 0;

    *length = index;
}

static void ICACHE_FLASH_ATTR set_values_ws2801(void)
{
    int i;
    int j;

    ws2801_data_t*  ws2801;
    ws2801 = ledmatrix.values.ws2801;

    for (i = 0; i < ws2801->chains; i++) {
        for (j = 0; j < ws2801->chain[i].leds; j++) {
            ws2801->chain[i].led[j].c.c = ws2801->chain[i].led[j].c.c << 8;
            ws2801->chain[i].led[j].c.c |= ((ws2801->chain[i].led[j].c.c >> 24) & 0xff);
            ws2801->chain[i].led[j].c.c &= 0x00ffffff;
        }
    }

    ws2801_write(ledmatrix.values.ws2801);
}

static void ICACHE_FLASH_ATTR ledmatrix_timer(void *arg)
{
    system_os_post(LEDMATRIX_TASK_PRIO, 0, (os_param_t)NULL);
}

static void ICACHE_FLASH_ATTR ledmatrix_task(os_event_t* event)
{
    set_values_ws2801();
}

static void ICACHE_FLASH_ATTR setup_ws2801(void)
{
    int i;
    int j;
    int chains = 6;
    ws2801_data_t*  ws2801;

    ws2801 = (void*)os_malloc(sizeof(*ws2801) + sizeof(ws2801->chain[0]) * chains);
    ws2801->chains = chains;

    for (i = 0; i < ws2801->chains; i++) {
        ws2801->chain[i].leds = 12;
        ws2801->chain[i].led = (void*)os_malloc(sizeof(ws2801->chain[i].led[0]) * ws2801->chain[i].leds);

        for (j = 0; j < ws2801->chain[i].leds; j++) {
            ws2801->chain[i].led[j].c.c = 0x00008080 << (((i+j)*8)%24);
            ws2801->chain[i].led[j].c.c |= ((ws2801->chain[i].led[j].c.c >> 24) & 0xff);
            ws2801->chain[i].led[j].c.c &= 0x00ffffff;
        }

        ws2801->chain[i].pin_clock = 14;
    }

    ws2801->chain[0].pin_data = 3;
    ws2801->chain[1].pin_data = 5;
    ws2801->chain[2].pin_data = 13;
    ws2801->chain[3].pin_data = 0;
    ws2801->chain[4].pin_data = 4;
    ws2801->chain[5].pin_data = 12;

    ledmatrix.values.ws2801 = ws2801;

    ws2801_init(ledmatrix.values.ws2801);
    ws2801_write(ledmatrix.values.ws2801);
}

void ICACHE_FLASH_ATTR user_init(void)
{
    os_memset(&ledmatrix, 0, sizeof(ledmatrix));

    // task for ledmatrix
    system_os_task(ledmatrix_task, LEDMATRIX_TASK_PRIO, ledmatrix.queue, LEDMATRIX_TASK_QUEUE_SIZE);
    system_os_post(LEDMATRIX_TASK_PRIO, 0, (os_param_t)NULL);

    // timer for ledmatrix
    os_timer_setfn(&ledmatrix.timer, (os_timer_func_t *)ledmatrix_timer, NULL);
    os_timer_arm(&ledmatrix.timer, POLL_TIME, 1);

    setup_ws2801();
}
