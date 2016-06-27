/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: i2c_master.c
 *
 * Description: i2c master API
 *
 * Modification history:
 *     2014/3/12, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"

#include "drivers/i2c_master.h"

#define I2C_MASTER_SDA_HIGH_SCL_HIGH(x)  \
    gpio_output_set(1<<(x->gpio_sda) | 1<<(x->gpio_scl), 0, 1<<(x->gpio_sda) | 1<<(x->gpio_scl), 0)

#define I2C_MASTER_SDA_HIGH_SCL_LOW(x)  \
    gpio_output_set(1<<(x->gpio_sda), 1<<(x->gpio_scl), 1<<(x->gpio_sda) | 1<<(x->gpio_scl), 0)

#define I2C_MASTER_SDA_LOW_SCL_HIGH(x)  \
    gpio_output_set(1<<(x->gpio_scl), 1<<(x->gpio_sda), 1<<(x->gpio_sda) | 1<<(x->gpio_scl), 0)

#define I2C_MASTER_SDA_LOW_SCL_LOW(x)  \
    gpio_output_set(0, 1<<(x->gpio_sda) | 1<<(x->gpio_scl), 1<<(x->gpio_sda) | 1<<(x->gpio_scl), 0)

static ICACHE_FLASH_ATTR uint32_t i2c_master_getGpioMux(uint8_t gpio)
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

static ICACHE_FLASH_ATTR uint32_t i2c_master_getGpioFunc(uint8_t gpio)
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

/******************************************************************************
 * FunctionName : i2c_master_setDC
 * Description  : Internal used function -
 *                    set i2c SDA and SCL bit value for half clk cycle
 * Parameters   : uint8_t SDA
 *                uint8_t SCL
 * Returns      : NONE
*******************************************************************************/
static void ICACHE_FLASH_ATTR
i2c_master_setDC(i2c_data* i2c, uint8_t SDA, uint8_t SCL)
{
    SDA	&= 0x01;
    SCL	&= 0x01;
    i2c->sda_last = SDA;
    i2c->scl_last = SCL;

    if ((0 == SDA) && (0 == SCL)) {
        I2C_MASTER_SDA_LOW_SCL_LOW(i2c);
    } else if ((0 == SDA) && (1 == SCL)) {
        I2C_MASTER_SDA_LOW_SCL_HIGH(i2c);
    } else if ((1 == SDA) && (0 == SCL)) {
        I2C_MASTER_SDA_HIGH_SCL_LOW(i2c);
    } else {
        I2C_MASTER_SDA_HIGH_SCL_HIGH(i2c);
    }
}

/******************************************************************************
 * FunctionName : i2c_master_getDC
 * Description  : Internal used function -
 *                    get i2c SDA bit value
 * Parameters   : NONE
 * Returns      : uint8_t - SDA bit value
*******************************************************************************/
static uint8_t ICACHE_FLASH_ATTR
i2c_master_getDC(i2c_data* i2c)
{
    uint8_t sda_out;
    sda_out = GPIO_INPUT_GET(GPIO_ID_PIN(i2c->gpio_sda));
    return sda_out;
}

/******************************************************************************
 * FunctionName : i2c_master_init
 * Description  : initilize I2C bus to enable i2c operations
 * Parameters   : NONE
 * Returns      : NONE
*******************************************************************************/
static void ICACHE_FLASH_ATTR
i2c_master_init(i2c_data* i2c)
{
    uint8_t i;

    i2c_master_setDC(i2c, 1, 0);
    i2c_master_wait(5);

    // when SCL = 0, toggle SDA to clear up
    i2c_master_setDC(i2c, 0, 0) ;
    i2c_master_wait(5);
    i2c_master_setDC(i2c, 1, 0) ;
    i2c_master_wait(5);

    // set data_cnt to max value
    for (i = 0; i < 28; i++) {
        i2c_master_setDC(i2c, 1, 0);
        i2c_master_wait(5);	// sda 1, scl 0
        i2c_master_setDC(i2c, 1, 1);
        i2c_master_wait(5);	// sda 1, scl 1
    }

    // reset all
    i2c_master_stop(i2c);
    return;
}

/******************************************************************************
 * FunctionName : i2c_master_gpio_init
 * Description  : config SDA and SCL gpio to open-drain output mode,
 *                mux and gpio num defined in i2c_master.h
 * Parameters   : NONE
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_gpio_init(i2c_data* i2c, uint8_t sda, uint8_t scl)
{
    i2c->gpio_sda = sda;
    i2c->gpio_scl = scl;

    ETS_GPIO_INTR_DISABLE() ;
//    ETS_INTR_LOCK();

    PIN_FUNC_SELECT(i2c_master_getGpioMux(i2c->gpio_sda),
                    i2c_master_getGpioFunc(i2c->gpio_sda));
    PIN_FUNC_SELECT(i2c_master_getGpioMux(i2c->gpio_scl),
                    i2c_master_getGpioFunc(i2c->gpio_scl));

    GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(i2c->gpio_sda)), GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(i2c->gpio_sda))) | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)); //open drain;
    GPIO_REG_WRITE(GPIO_ENABLE_ADDRESS, GPIO_REG_READ(GPIO_ENABLE_ADDRESS) | (1 << i2c->gpio_sda));
    GPIO_REG_WRITE(GPIO_PIN_ADDR(GPIO_ID_PIN(i2c->gpio_scl)), GPIO_REG_READ(GPIO_PIN_ADDR(GPIO_ID_PIN(i2c->gpio_scl))) | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_ENABLE)); //open drain;
    GPIO_REG_WRITE(GPIO_ENABLE_ADDRESS, GPIO_REG_READ(GPIO_ENABLE_ADDRESS) | (1 << i2c->gpio_scl));

    I2C_MASTER_SDA_HIGH_SCL_HIGH(i2c);

    ETS_GPIO_INTR_ENABLE() ;
//    ETS_INTR_UNLOCK();

    i2c_master_init(i2c);
}

/******************************************************************************
 * FunctionName : i2c_master_start
 * Description  : set i2c to send state
 * Parameters   : NONE
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_start(i2c_data* i2c)
{
    i2c_master_setDC(i2c, 1, i2c->scl_last);
    i2c_master_wait(5);
    i2c_master_setDC(i2c, 1, 1);
    i2c_master_wait(5);	// sda 1, scl 1
    i2c_master_setDC(i2c, 0, 1);
    i2c_master_wait(5);	// sda 0, scl 1
}

/******************************************************************************
 * FunctionName : i2c_master_stop
 * Description  : set i2c to stop sending state
 * Parameters   : NONE
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_stop(i2c_data* i2c)
{
    i2c_master_wait(5);

    i2c_master_setDC(i2c, 0, i2c->scl_last);
    i2c_master_wait(5);	// sda 0
    i2c_master_setDC(i2c, 0, 1);
    i2c_master_wait(5);	// sda 0, scl 1
    i2c_master_setDC(i2c, 1, 1);
    i2c_master_wait(5);	// sda 1, scl 1
}

/******************************************************************************
 * FunctionName : i2c_master_setAck
 * Description  : set ack to i2c bus as level value
 * Parameters   : uint8_t level - 0 or 1
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_setAck(i2c_data* i2c, uint8_t level)
{
    i2c_master_setDC(i2c, i2c->sda_last, 0);
    i2c_master_wait(5);
    i2c_master_setDC(i2c, level, 0);
    i2c_master_wait(5);	// sda level, scl 0
    i2c_master_setDC(i2c, level, 1);
    i2c_master_wait(8);	// sda level, scl 1
    i2c_master_setDC(i2c, level, 0);
    i2c_master_wait(5);	// sda level, scl 0
    i2c_master_setDC(i2c, 1, 0);
    i2c_master_wait(5);
}

/******************************************************************************
 * FunctionName : i2c_master_getAck
 * Description  : confirm if peer send ack
 * Parameters   : NONE
 * Returns      : uint8_t - ack value, 0 or 1
*******************************************************************************/
uint8_t ICACHE_FLASH_ATTR
i2c_master_getAck(i2c_data* i2c)
{
    uint8_t retVal;
    i2c_master_setDC(i2c, i2c->sda_last, 0);
    i2c_master_wait(5);
    i2c_master_setDC(i2c, 1, 0);
    i2c_master_wait(5);
    i2c_master_setDC(i2c, 1, 1);
    i2c_master_wait(5);

    retVal = i2c_master_getDC(i2c);
    i2c_master_wait(5);
    i2c_master_setDC(i2c, 1, 0);
    i2c_master_wait(5);

    return retVal;
}

/******************************************************************************
* FunctionName : i2c_master_checkAck
* Description  : get dev response
* Parameters   : NONE
* Returns      : true : get ack ; false : get nack
*******************************************************************************/
bool ICACHE_FLASH_ATTR
i2c_master_checkAck(i2c_data* i2c)
{
    if(i2c_master_getAck(i2c)){
        return FALSE;
    }else{
        return TRUE;
    }
}

/******************************************************************************
* FunctionName : i2c_master_send_ack
* Description  : response ack
* Parameters   : NONE
* Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_send_ack(i2c_data* i2c)
{
    i2c_master_setAck(i2c, 0x0);
}
/******************************************************************************
* FunctionName : i2c_master_send_nack
* Description  : response nack
* Parameters   : NONE
* Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_send_nack(i2c_data* i2c)
{
    i2c_master_setAck(i2c, 0x1);
}

/******************************************************************************
 * FunctionName : i2c_master_readByte
 * Description  : read Byte from i2c bus
 * Parameters   : NONE
 * Returns      : uint8_t - readed value
*******************************************************************************/
uint8_t ICACHE_FLASH_ATTR
i2c_master_readByte(i2c_data* i2c)
{
    uint8_t retVal = 0;
    uint8_t k, i;

    i2c_master_wait(5);
    i2c_master_setDC(i2c, i2c->sda_last, 0);
    i2c_master_wait(5);	// sda 1, scl 0

    for (i = 0; i < 8; i++) {
        i2c_master_wait(5);
        i2c_master_setDC(i2c, 1, 0);
        i2c_master_wait(5);	// sda 1, scl 0
        i2c_master_setDC(i2c, 1, 1);
        i2c_master_wait(5);	// sda 1, scl 1

        k = i2c_master_getDC(i2c);
        i2c_master_wait(5);

        if (i == 7) {
            i2c_master_wait(3);   ////
        }

        k <<= (7 - i);
        retVal |= k;
    }

    i2c_master_setDC(i2c, 1, 0);
    i2c_master_wait(5);	// sda 1, scl 0

    return retVal;
}

/******************************************************************************
 * FunctionName : i2c_master_writeByte
 * Description  : write wrdata value(one byte) into i2c
 * Parameters   : uint8_t wrdata - write value
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
i2c_master_writeByte(i2c_data* i2c, uint8_t wrdata)
{
    uint8_t dat;
    int8_t i;

    i2c_master_wait(5);

    i2c_master_setDC(i2c, i2c->sda_last, 0);
    i2c_master_wait(5);

    for (i = 7; i >= 0; i--) {
        dat = wrdata >> i;
        i2c_master_setDC(i2c, dat, 0);
        i2c_master_wait(5);
        i2c_master_setDC(i2c, dat, 1);
        i2c_master_wait(5);

        if (i == 0) {
            i2c_master_wait(3);   ////
        }

        i2c_master_setDC(i2c, dat, 0);
        i2c_master_wait(5);
    }
}
