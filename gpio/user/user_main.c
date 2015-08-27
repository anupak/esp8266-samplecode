#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "mem.h"
#include "ip_addr.h"
#include "espconn.h"
#include "user_interface.h"

#include "driver/uart.h"
#include "user_config.h"

#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
os_event_t    user_procTaskQueue[user_procTaskQueueLen];
static void loop(os_event_t *events);

/* Variable to hold the number of times the key has been pressed */
volatile int            whatyouwant;


// gpio interrupt handler. See below
LOCAL void  gpio_intr_handler(int * dummy);

//-------------------------------------------------------------------------------------------------
// loop function will be execute by "os" periodically
static void ICACHE_FLASH_ATTR  loop(os_event_t *events)
{
    os_delay_us(10);
}

// interrupt handler: this function will be executed on any edge of GPIO0
LOCAL void  gpio_intr_handler(int *dummy)
{
// clear gpio status. Say ESP8266EX SDK Programming Guide in  5.1.6. GPIO interrupt handler

    uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);


    // if the interrupt was by GPIO0
    if (gpio_status & BIT(0))
    {
        // disable interrupt for GPIO0
        gpio_pin_intr_state_set(GPIO_ID_PIN(0), GPIO_PIN_INTR_DISABLE);

        // Do something, for example, increment whatyouwant indirectly
        // Currently holds the number of times the button was pressed
        (*dummy)++;

        // Print a debug message
		os_printf("Button Pushed %d times\r\n", *dummy);

        //clear interrupt status for GPIO0
        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(0));

        // Reactivate interrupts for GPIO0
        gpio_pin_intr_state_set(GPIO_ID_PIN(0), GPIO_PIN_INTR_ANYEDGE);
    }
}


//-------------------------------------------------------------------------------------------------
//Init function
void ICACHE_FLASH_ATTR  user_init()
{
    // Initialize UART0 to use as debug
    UART_init(BIT_RATE_115200, BIT_RATE_115200);

    // Initialize the GPIO subsystem.
    gpio_init();

    whatyouwant = 0;

    // =================================================
    // Initialize GPIO2 and GPIO0 as GPIO
    // =================================================
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
    //PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);


    PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO0_U);
    PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO2_U);

    gpio_output_set(0, 0, 0, GPIO_ID_PIN(0)); // set set gpio 0 as input

    // =================================================
    // Activate gpio interrupt for gpio2
    // =================================================

    // Disable interrupts by GPIO
    ETS_GPIO_INTR_DISABLE();

    // Attach interrupt handle to gpio interrupts.
    ETS_GPIO_INTR_ATTACH(gpio_intr_handler, &whatyouwant);

    // clear gpio status. Say ESP8266EX SDK Programming Guide in  5.1.6. GPIO interrupt handler
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(0));

    // clear gpio status. Say ESP8266EX SDK Programming Guide in  5.1.6. GPIO interrupt handler
    gpio_pin_intr_state_set(GPIO_ID_PIN(0), GPIO_PIN_INTR_ANYEDGE);


    // Enable interrupts by GPIO
    ETS_GPIO_INTR_ENABLE();


    //Start os task
    system_os_task(loop, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
}

