#ifndef _CONFIG_FLASH_H_
#define _CONFIG_FLASH_H_

#include "c_types.h"
#include "mem.h"
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "spi_flash.h"

#define MAGIC_NUMBER    0xf0f0f0f0

typedef struct
{
    uint32_t    magic_number;
    uint8_t     length;
    uint8_t     ssid[32];
    uint8_t     password[64];
    uint8_t     auto_connect;
    uint8_t     dummy[68];
} sysconfig_t, *sysconfig_p;

int config_load(int version, sysconfig_p config);
void config_save(int version, sysconfig_p config);

#endif
