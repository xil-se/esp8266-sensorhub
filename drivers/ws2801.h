/*
"THE BEER/MATE-WARE LICENSE":
<xil@xil.se> wrote this file. As long as you retain this notice you
can do whatever you want with this stuff. If we meet some day, and you think
this stuff is worth it, you can buy us a ( > 0 ) beer/mate in return - The Xil TEAM
*/

#ifndef WS2801_H
#define WS2801_H

#include <stdbool.h>

typedef struct {
    union   {
        uint32_t    c;
        struct {
            uint8_t     b;
            uint8_t     g;
            uint8_t     r;
            uint8_t     a;
        }           argb;
    }           c;
} ws2801_color_t;

typedef struct {
    uint8_t         pin_clock;
    uint8_t         pin_data;
    uint8_t         leds;
    ws2801_color_t* led;
} ws2801_chain_t;

typedef struct {
    uint8_t         chains;
    ws2801_chain_t  chain[];
} ws2801_data_t;

bool ws2801_write(ws2801_data_t* ws2801);
bool ws2801_init(ws2801_data_t* ws2801);

#endif // WS2801_H
