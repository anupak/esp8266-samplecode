#ifndef _RTC_H_
#define _RTC_H_

#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include <time.h>

#include "driver/i2c_master.h"

#define I2C_RTC_ADDRESS 0x68

/* The following bytes are used in very transaction, hence they are
   preformed for code optimization. The I2C specifies the 1st byte is
   device address [7 bits] followed by a READ/~WRITE [1 bit]
 */
#define RTC_READ_BYTE   (I2C_RTC_ADDRESS<<1 | 0x01)
#define RTC_WRITE_BYTE  (I2C_RTC_ADDRESS<<1)


uint32_t rtc_get_current_timestamp();   // Returns the current time from EPOC (using RTC)
uint8_t rtc_set_current_timestamp(uint32_t timestamp);
char *convert_epoc_to_str(uint32_t);
#endif
