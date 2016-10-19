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
#include "utils/rgbhsv.h"

#define LEDMATRIX_TASK_PRIO           2
#define LEDMATRIX_TASK_QUEUE_SIZE     1

#define POLL_TIME   1 // ms

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

static struct {
    os_event_t          queue[LEDMATRIX_TASK_QUEUE_SIZE];
    volatile os_timer_t timer;
    struct {
        ws2801_data_t*      ws2801;
    } values;
} ledmatrix;

static void ICACHE_FLASH_ATTR set_values_ws2801(void)
{
    static int t = 0;
    int i;

    ws2801_data_t* ws2801;
    ws2801 = ledmatrix.values.ws2801;

    t++;

    for (i = 0; i < ws2801->chain[0].leds; i++) {
        uint8_t *r = &ws2801->chain[0].led[i].c.argb.r;
        uint8_t *g = &ws2801->chain[0].led[i].c.argb.g;
        uint8_t *b = &ws2801->chain[0].led[i].c.argb.b;
        uint8_t *a = &ws2801->chain[0].led[i].c.argb.a;
        uint8_t h = (i*4 + t/2) & 0xff;
        hsvtorgb(r, g, b, h, 255, 255);
/*
        uint8_t c = t % 128;
        switch ((t/128)%3) {
            case 0:
                *r = c;
                *g = 0;
                *b = 0;
                break;
            case 1:
                *r = 0;
                *g = c;
                *b = 0;
                break;
            case 2:
                *r = 0;
                *g = 0;
                *b = c;
                break;
        }
*/
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
    os_timer_arm(&ledmatrix.timer, POLL_TIME, 0);
    //system_os_post(LEDMATRIX_TASK_PRIO, 0, (os_param_t)NULL);
}

static void ICACHE_FLASH_ATTR setup_ws2801(void)
{

    int i;
    int j;
    int chains = 1;
    ws2801_data_t*  ws2801;

    ws2801 = (void*)os_malloc(sizeof(*ws2801) + sizeof(ws2801->chain[0]) * chains);
    ws2801->chains = chains;

    for (i = 0; i < ws2801->chains; i++) {
        ws2801->chain[i].leds = 100;
        ws2801->chain[i].led = (void*)os_malloc(sizeof(ws2801->chain[i].led[0]) * ws2801->chain[i].leds);
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
    ws2801_write(ledmatrix.values.ws2801);}

void ICACHE_FLASH_ATTR user_init(void)
{
    os_memset(&ledmatrix, 0, sizeof(ledmatrix));

    // task for ledmatrix
    system_os_task(ledmatrix_task, LEDMATRIX_TASK_PRIO, ledmatrix.queue, LEDMATRIX_TASK_QUEUE_SIZE);
    system_os_post(LEDMATRIX_TASK_PRIO, 0, (os_param_t)NULL);

    setup_ws2801();

    // timer for ledmatrix
    os_timer_setfn(&ledmatrix.timer, (os_timer_func_t *)ledmatrix_timer, NULL);
    os_timer_arm(&ledmatrix.timer, POLL_TIME, 0);
}
