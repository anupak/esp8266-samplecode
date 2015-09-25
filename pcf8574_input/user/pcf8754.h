#ifndef _PCF8754_H_
#define _PCF8754_H_

#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"

#include "driver/i2c_master.h"

uint8_t pcf8754_i2c_read_byte(uint8_t device,uint8_t *byte);
uint8_t pcf8754_i2c_write_byte(uint8_t device, uint8_t byte);

#endif
