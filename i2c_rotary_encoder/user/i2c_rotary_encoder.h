#ifndef _I2C_ROTARY_ENCODER_
#define _I2C_ROTARY_ENCODER_

#include "pcf8754.h"

/* These are the pinmaps on pcf8758 */
#define ROTARY_ENCODER_MASK         0b00000111
#define ROTARY_ENCODER_PIN1         0
#define ROTARY_ENCODER_PIN2         1
#define ROTARY_ENCODER_BUTTON       2

typedef enum {NO_CHANGE=0, ENCODER_CW, ENCODER_CCW} encoder_direction_t;
#endif
