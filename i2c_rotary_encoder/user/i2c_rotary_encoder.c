#include "i2c_rotary_encoder.h"

static uint8_t  encoder_input_data[4];
static uint8_t  counter = 0;

uint8_t decode_rotary_encoder(uint8_t byte, encoder_direction_t *direction, uint8_t *button)
{
    uint8_t ret = 0, temp;
    int input = ROTARY_ENCODER_MASK & byte;
    uint8_t bit1, bit2;

    /* Check if need to process the data in first place */
    *button = 0;
    *direction = NO_CHANGE;

    *button = (input & (1<< ROTARY_ENCODER_BUTTON)) >> ROTARY_ENCODER_BUTTON;
    if (*button) ret += 1;

    /* Sequence CW: 1320, CCW:2310 */
    bit1 = (input & (1<< ROTARY_ENCODER_PIN1)) >> ROTARY_ENCODER_PIN1;
    bit2 = (input & (1<< ROTARY_ENCODER_PIN2)) >> ROTARY_ENCODER_PIN2;

    temp = (bit2)<<1 | bit1;
    if ((encoder_input_data[counter] != temp))
    {
        if (temp == 0)
            counter = 0;
        else
            counter ++;

        encoder_input_data[counter] = temp;
    }

    if (counter == 3)
    {
        //os_printf("Rotary Encode: %x %x %x %x ", encoder_input_data[0] , encoder_input_data[1] , encoder_input_data[2] , encoder_input_data[3] );

        if ((encoder_input_data[1] == 1) && (encoder_input_data[3] == 2))
            *direction = ENCODER_CW;

        if ((encoder_input_data[1] == 2) && (encoder_input_data[3] == 1))
            *direction = ENCODER_CCW;

        ret += 1;
        //counter = 0;
    }



    return (ret);
}
