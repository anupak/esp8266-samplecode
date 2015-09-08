#include "config_flash.h"


/*     From the document 99A-SDK-Espressif IOT Flash RW Operation_v0.2      *
 * -------------------------------------------------------------------------*
 * Flash is erased sector by sector, which means it has to erase 4Kbytes one
 * time at least. When you want to change some data in flash, you have to
 * erase the whole sector, and then write it back with the new data.
 *--------------------------------------------------------------------------*/
void config_load_default(sysconfig_p config)
{
    os_memset(config, 0, sizeof(sysconfig_t));
    os_printf("Loading default configuration\r\n");
    config->magic_number                = MAGIC_NUMBER;
    config->length                      = sizeof(sysconfig_t);
    os_sprintf(config->ssid,"%s",       WIFI_SSID);
    os_sprintf(config->password,"%s",   WIFI_PASSWORD);
    config->auto_connect                = 1;
}

int config_load(int version, sysconfig_p config)
{
    if (config == NULL) return -1;
    uint16_t base_address = 0x3c + version;

    spi_flash_read(base_address* SPI_FLASH_SEC_SIZE, &config->magic_number, 4);

    if((config->magic_number != MAGIC_NUMBER))
    {
        os_printf("\r\nMagic Number not present, saving default configuration in flash\r\n");
        config_load_default(config);
        config_save(version, config);
        return -1;
    }

    os_printf("Magic Number is present configuration is loaded");
    spi_flash_read(base_address * SPI_FLASH_SEC_SIZE, (uint32 *) config, sizeof(sysconfig_t));
    os_printf("\r\n Length:%d  [%d]\r\n", config->length, sizeof(sysconfig_t));
    if (config->length != sizeof(sysconfig_t))
    {
        os_printf("Length Mismatch, old version of config probably, loading defaults\r\n");
        config_load_default(config);
        config_save(version, config);
    }
}

void config_save(int version, sysconfig_p config)
{
    uint16_t base_address = 0x3c + version;
    os_printf("Saving configuration\r\n");
    spi_flash_erase_sector(base_address);
    spi_flash_write(base_address * SPI_FLASH_SEC_SIZE, (uint32 *)config, sizeof(sysconfig_t));
}
