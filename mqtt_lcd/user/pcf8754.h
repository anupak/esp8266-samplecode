#ifndef _PCF8754_H_
#define _PCF8754_H_

#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"

#include "driver/i2c_master.h"

#define I2C_LCD_ADDRESS 0x20


/* The following bytes are used in very transaction, hence they are
   preformed for code optimization. The I2C specifies the 1st byte is
   device address [7 bits] followed by a READ/~WRITE [1 bit]
 */
#define LCD_READ_BYTE   (I2C_LCD_ADDRESS<<1 | 0x01)
#define LCD_WRITE_BYTE  (I2C_LCD_ADDRESS<<1)

uint8_t pcf8754_i2c_read_byte(uint8_t *byte);
uint8_t pcf8754_i2c_write_byte(uint8_t byte);

#endif
