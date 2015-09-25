#include "pcf8754.h"


uint8_t pcf8754_i2c_read_byte(uint8_t device, uint8_t *byte)
{
    uint8_t result;
    uint8_t device_byte = ((device& 0x07f)<<1) | 0x01;

    i2c_master_start();                     // Start I2C request

    i2c_master_writeByte(device_byte);     // PCF8750 address + W bit
    result = i2c_master_checkAck();
    if (!result) return 0;

    //Now send the address of required register
    *byte = i2c_master_readByte();
    i2c_master_send_nack();

    i2c_master_stop();
    return 1;
}

uint8_t pcf8754_i2c_write_byte(uint8_t device, uint8_t byte)
{
    uint8_t result;
    uint8_t device_byte = (device&0x07f)<<1;

    i2c_master_start();                     // Start I2C request

    i2c_master_writeByte(device_byte);     // PCF8750 address + W bit
    result = i2c_master_checkAck();
    if (!result) return 0;

    //Now send the address of required register
    i2c_master_writeByte(byte);
    result = i2c_master_checkAck();
    if (!result) return 0;

    i2c_master_stop();
    return 1;
}

