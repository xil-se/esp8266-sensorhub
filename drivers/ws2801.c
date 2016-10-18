/*
"THE BEER/MATE-WARE LICENSE":
<xil@xil.se> wrote this file. As long as you retain this notice you
can do whatever you want with this stuff. If we meet some day, and you think
this stuff is worth it, you can buy us a ( > 0 ) beer/mate in return - The Xil TEAM
*/

#include <stdint.h>

#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"

#include "drivers/ws2801.h"

static uint32_t ICACHE_FLASH_ATTR gpio_mux(uint8_t gpio)
{
    switch (gpio) {
        case 0:  return PERIPHS_IO_MUX_GPIO0_U;
        case 1:  return PERIPHS_IO_MUX_U0TXD_U;
        case 2:  return PERIPHS_IO_MUX_GPIO2_U;
        case 3:  return PERIPHS_IO_MUX_U0RXD_U;
        case 4:  return PERIPHS_IO_MUX_GPIO4_U;
        case 5:  return PERIPHS_IO_MUX_GPIO5_U;
//        case 6:  return PERIPHS_IO_MUX_SD_CLK_U;
//        case 7:  return PERIPHS_IO_MUX_SD_DATA0_U;
//        case 8:  return PERIPHS_IO_MUX_SD_DATA1_U;
//        case 9:  return PERIPHS_IO_MUX_SD_DATA2_U;
//        case 10: return PERIPHS_IO_MUX_SD_DATA3_U;
//        case 11: return PERIPHS_IO_MUX_SD_CMD_U;
        case 12: return PERIPHS_IO_MUX_MTDI_U;
        case 13: return PERIPHS_IO_MUX_MTCK_U;
        case 14: return PERIPHS_IO_MUX_MTMS_U;
        case 15: return PERIPHS_IO_MUX_MTDO_U;
//        case 16: return PERIPHS_IO_MUX_XPD_DCDC;
        default: return 0x0;
    }
}

static uint32_t ICACHE_FLASH_ATTR gpio_func(uint8_t gpio)
{
    switch (gpio) {
        // CLKOUT
        case 0:  return FUNC_GPIO0;
        // UART0 TX
        case 1:  return FUNC_GPIO1;
        case 2:  return FUNC_GPIO2;
        // UART0 RX
        case 3:  return FUNC_GPIO3;
        case 4:  return FUNC_GPIO4;
        case 5:  return FUNC_GPIO5;
        // Typically used to interface flash memory IC on most esp8266 modules
//        case 6:  return FUNC_GPIO6;
//        case 7:  return FUNC_GPIO7;
//        case 8:  return FUNC_GPIO8;
//        case 9:  return FUNC_GPIO9;
//        case 10: return FUNC_GPIO10;
//        case 11: return FUNC_GPIO11;
        // SPI MISO
        case 12: return FUNC_GPIO12;
        // SPI MOSI
        case 13: return FUNC_GPIO13;
        // SPI CLK
        case 14: return FUNC_GPIO14;
        // SPI CS
        case 15: return FUNC_GPIO15;
        // Not available as gpio
        case 16:
        default: return 0x0;
    }
}

static void ICACHE_FLASH_ATTR write_clock(int pin)
{
    GPIO_OUTPUT_SET(pin, 1);
//    os_delay_us(10);
    GPIO_OUTPUT_SET(pin, 0);
}

static void ICACHE_FLASH_ATTR write_bit(ws2801_color_t* led, uint32_t bit, uint32_t pin)
{
    int data;

    data = led->c.c & (1<<(3*8-1-bit));
    if (data == 0) {
        GPIO_OUTPUT_SET(pin, 0);
    } else {
        GPIO_OUTPUT_SET(pin, 1);
    }
}

bool ICACHE_FLASH_ATTR ws2801_write(ws2801_data_t* ws2801)
{
    int     chain;
    int     leds_max = 0;
    int     led;

    for (chain = 0; chain < ws2801->chains; chain++) {
        if (leds_max < ws2801->chain[chain].leds) {
            leds_max = ws2801->chain[chain].leds;
        }
    }

    for (led = 0; led < leds_max; led++) {
        uint32_t    bit;

        for (bit = 0; bit < 3*8; bit++) {
            for (chain = 0; chain < ws2801->chains; chain++) {
                if (ws2801->chain[chain].leds >= led) {
                    write_bit(&ws2801->chain[chain].led[led], bit, ws2801->chain[chain].pin_data);
                }
            }
            for (chain = 0; chain < ws2801->chains; chain++) {
                if (chain == 0) {
                    write_clock(ws2801->chain[chain].pin_clock);
#if 0
                } else {
                    int     c;
                    bool    written = false;
                    for (c = chain-1; c >= 0; c--) {
                        if (ws2801->chain[chain].pin_clock ==
                            ws2801->chain[c].pin_clock)
                        {
                            written = true;
                        }
                    }
                    if (!written) {
                        write_clock(ws2801->chain[chain].pin_clock);
                    }
#endif
                }
            }
        }
    }

    return true;
}

bool ICACHE_FLASH_ATTR ws2801_init(ws2801_data_t* ws2801)
{
    int i;

    ETS_GPIO_INTR_DISABLE();

    for (i = 0; i < ws2801->chains; i++) {
        int pin_clock = ws2801->chain[i].pin_clock;
        int pin_data = ws2801->chain[i].pin_data;

        PIN_FUNC_SELECT(gpio_mux(pin_clock),
                        gpio_func(pin_clock));
        PIN_FUNC_SELECT(gpio_mux(pin_data),
                        gpio_func(pin_data));
        // set gpios to open drain
//        GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_clock)), GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_clock))) | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE));
//        GPIO_REG_WRITE(GPIO_ENABLE_ADDRESS, GPIO_REG_READ(GPIO_ENABLE_ADDRESS) | (1 << pin_clock));
//        GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_data)), GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(pin_data))) | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE));
//        GPIO_REG_WRITE(GPIO_ENABLE_ADDRESS, GPIO_REG_READ(GPIO_ENABLE_ADDRESS) | (1 << pin_data));

        GPIO_OUTPUT_SET(pin_clock, 0);
        GPIO_OUTPUT_SET(pin_data, 0);
    }

    ETS_GPIO_INTR_ENABLE();

    os_delay_us(1000);

    return true;
}
