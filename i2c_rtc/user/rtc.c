#include "rtc.h"

/* Internal Functions */
uint8_t RTC_Read(uint8_t address, uint8_t n_bytes, uint8_t *data);
uint8_t RTC_Write(uint8_t address, uint8_t n_bytes, uint8_t *data);

char *convert_epoc_to_str(uint32_t current_stamp)
{
    char *str = (char *)sntp_get_real_time(current_stamp);
    *(str+strlen(str)-1) = 0;
    return (str);
}

// Returns the current time from EPOC (using RTC)
uint32_t rtc_get_current_timestamp()
{
    uint8_t     rtc_bytes[8];
    uint8_t     bytes_read;
    uint32_t    timestamp = 0;
    struct tm   ts;

    //os_printf("Corrected time: %d:%d:%d %d/%d/%d\r\n", ts->tm_hour, ts->tm_min, ts->tm_sec, ts->tm_mday, ts->tm_mon, ts->tm_year);
    os_memset(rtc_bytes, 0, 7);
    bytes_read = RTC_Read(0x00, 7, rtc_bytes);
    if (bytes_read < 7) return timestamp;

    /* unload contents in to the TS structure */
    ts.tm_sec  = (((rtc_bytes[0] & 0b01110000)>>4)*10)+(rtc_bytes[0] & 0b00001111);
    ts.tm_min  = (((rtc_bytes[1] & 0b01110000)>>4)*10)+(rtc_bytes[1] & 0b00001111);


    /* AM/PM Logic not present */
    ts.tm_hour = (((rtc_bytes[2] & 0b00010000)>>4)*10)+(rtc_bytes[2] & 0b00001111);

    ts.tm_mday = (((rtc_bytes[4] & 0b00110000)>>4) *10)+ (rtc_bytes[4] & 0b00001111);
    ts.tm_mon  = (((rtc_bytes[5] & 0b00110000)>>4) *10)+ (rtc_bytes[5] & 0b00001111);

    /* TS month is from 0-11, whereas RTC stores from 1-12 hence the decrement */
    ts.tm_mon  --;

    ts.tm_year = (2000 - 1900) + (((rtc_bytes[6] & 0b11110000)>>4) *10)+ (rtc_bytes[6] & 0b00001111);

    timestamp = mktime(&ts);

    return (timestamp);
}

uint8_t rtc_set_current_timestamp(uint32_t timestamp)
{
    struct tm   *ts;
    uint8_t     rtc_bytes[8];
    uint16_t     year;
    uint8_t     bytes_written;

    ts = localtime((time_t *) &timestamp);

    //os_printf("rtc setting: %d, %s\r\n",timestamp, convert_epoc_to_str(timestamp));

    rtc_bytes[0] = ((ts->tm_sec/10)<<4)|(ts->tm_sec%10);
    rtc_bytes[1] = ((ts->tm_min/10)<<4)|(ts->tm_min%10);

    /* AM/PM Logic not present */
    rtc_bytes[2] = ((ts->tm_hour/10)<<4)|(ts->tm_hour%10);
    rtc_bytes[3] = 0;
    rtc_bytes[4] = ((ts->tm_mday/10)<<4)|(ts->tm_mday%10);

    ts->tm_mon ++;
    rtc_bytes[5] = ((ts->tm_mon/10)<<4)|(ts->tm_mon%10);


    year = ts->tm_year + 1900;
    if (year > 2000)
    {
        year = year - 2000;
    }

    rtc_bytes[6] = ((year/10)<<4)|(year%10);
    bytes_written = RTC_Write(0x00, 7, rtc_bytes);
}



/*----------------------------------------------------------------------------*/
/*                        Internal Functions                                  */
/*----------------------------------------------------------------------------*/


/* ---------------------------------------------------------------------------
    Internal Function To Read Internal Registers of DS1307

    address     [IN] : Address of the register (refer datasheet)
    data        [OUT]: Value of register is copied to this.

    Returns:
        0 = Failure
        n = Success (bytes read)
 --------------------------------------------------------------------------- */
uint8_t RTC_Read(uint8_t address, uint8_t n_bytes, uint8_t *data)
{
    uint8_t result;
    uint8_t i;
    uint8_t  *tmp = data;
    i2c_master_start();                     // Start I2C request

    i2c_master_writeByte(RTC_WRITE_BYTE);     //DS1307 address + W
    result = i2c_master_checkAck();
    if (!result) return 0;

    //Now send the address of required register
    i2c_master_writeByte(address);
    result = i2c_master_checkAck();
    if (!result) return 0;

    i2c_master_start();

    i2c_master_writeByte(RTC_READ_BYTE);    //DS1307 Address + R
    result = i2c_master_checkAck();
    if (!result) return 0;

    //Now read the value with NACK
    for(i=0; i <= n_bytes; i++, tmp++)
    {
        *(tmp) = i2c_master_readByte();


        if (i < n_bytes)
        {
            i2c_master_send_ack();
        }
        else
        {
            i2c_master_send_nack();
        }
    }

    i2c_master_stop();

    return i;
}

/* ---------------------------------------------------------------------------
    Internal Function To write Internal Registers of DS1307

    address     [IN] : Address of the register (refer datasheet)
    data        [IN]: Value of register is copied to this.

    Returns:
        0 = Failure
        n = Success (bytes written)
 --------------------------------------------------------------------------- */
uint8_t RTC_Write(uint8_t address, uint8_t n_bytes, uint8_t *data)
{
    uint8_t result;
    uint8_t i;
    i2c_master_start();                     // Start I2C request

    i2c_master_writeByte(RTC_WRITE_BYTE);     //DS1307 address + W
    result = i2c_master_checkAck();
    if (!result) return 0;

    //Now send the address of required register
    i2c_master_writeByte(address);
    result = i2c_master_checkAck();
    if (!result) return 0;

    //Now read the value with NACK
    for(i=0; i<n_bytes; i++)
    {
        i2c_master_writeByte(*(data+i));
        result = i2c_master_checkAck();
        if (!result) break;
    }

    i2c_master_stop();

    return i;
}
