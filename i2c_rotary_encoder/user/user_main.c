#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "driver/uart.h"

#include "ip_addr.h"
#include "espconn.h"
#include "user_interface.h"
#include "driver/i2c_master.h"  // Provided in examples/IoT sample
#include "pcf8754.h"  // Provided in examples/IoT sample
/* System task priority */
#define user_procTaskPrio        0


#include "i2c_rotary_encoder.h"

/* System Task */
static void user_procTask(os_event_t *events);
/* System Task Message Queue */
#define user_procTaskQueueLen    1
os_event_t    user_procTaskQueue[user_procTaskQueueLen];

volatile os_timer_t     timer;
void    post_user_init_func();

static void wifi_handle_event_cb(System_Event_t *evt);

/* Wrote a simple I2C bus scanner to find connected i2C devices */
void i2c_bus_scan()
{
    int index = 0;
    uint8_t result;

    os_printf("Scanning I2C bus for devices\r\n");
    for (index=1; index <=127; index ++)
    {
        i2c_master_start();                     // Start I2C request

        i2c_master_writeByte(index<<1| 0b00000001);     //DS1307 address + W
        result = i2c_master_checkAck();

        if (result)
        {
            os_printf("\tI2C device present at 0x%x\r\n", index);
        }

        i2c_master_stop();
    }

}

/* Setup the Acess point settings */
void ICACHE_FLASH_ATTR user_set_station_config(void)
{
    struct station_config stationConf;

    wifi_set_opmode(0x01);      // Set AP, Station or Both mode

    stationConf.bssid_set = 0;
    //need not check MAC address of AP

    os_memcpy(&stationConf.ssid, SSID, 32);
    os_memcpy(&stationConf.password, PASSWORD, 64);
    wifi_station_set_config(&stationConf);

    wifi_set_event_handler_cb(wifi_handle_event_cb);
}

/* Callback called when the connection state of the module with an Access Point changes */
void wifi_handle_event_cb(System_Event_t *evt)
{
  //  os_printf("event %x\n", evt->event);

    os_printf("wifi_handle_event_cb: ");
    switch (evt->event)
    {
    case EVENT_STAMODE_CONNECTED:
        os_printf("Connected to ssid %s, channel %d\n", evt->event_info.connected.ssid, evt->event_info.connected.channel);
        break;

    case EVENT_STAMODE_DISCONNECTED:
        os_printf("Disconnected from ssid %s, reason %d\n", evt->event_info.disconnected.ssid, evt->event_info.disconnected.reason);
        break;

    case EVENT_STAMODE_AUTHMODE_CHANGE:
        os_printf("Mode changed : %d -> %d\n", evt->event_info.auth_change.old_mode, evt->event_info.auth_change.new_mode);
        break;

    case EVENT_STAMODE_GOT_IP:
        os_printf("IP Assigned (station mode):" IPSTR ",mask:" IPSTR ",gw:" IPSTR, IP2STR(&evt->event_info.got_ip.ip), IP2STR(&evt->event_info.got_ip.mask), IP2STR(&evt->event_info.got_ip.gw));
        os_printf("\r\n");
        break;

    default:
        break;
    }
}


//Do nothing function
static void ICACHE_FLASH_ATTR user_procTask(os_event_t *events)
{
	uint8_t buffer[80];
	int recv_len = -1;
	int index;

    switch(events->sig)
    {

    default:
        os_delay_us(10);
    }

}


// Called once the system intialization is complete
void post_user_init_func()
{
    uint8_t byte;
    /* Simple Debug I2C scanner, not needed for the operation */
    i2c_bus_scan();

//    byte = pcf8754_i2c_read_byte();
//    os_printf("0x%x\r\n", byte);

}

// interrupt handler: this function will be executed on any edge of GPIO0
LOCAL void  gpio_intr_handler(int *dummy)
{
// clear gpio status. Say ESP8266EX SDK Programming Guide in  5.1.6. GPIO interrupt handler

    uint32 gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);

    // if the interrupt was by GPIO0
    if (gpio_status & BIT(3))
    {
        uint8_t byte, button, encRet;
        encoder_direction_t dir;

        // disable interrupt for GPIO0
        gpio_pin_intr_state_set(GPIO_ID_PIN(3), GPIO_PIN_INTR_DISABLE);

        // Print a debug message
		//os_printf("PCF Indicates a pin change : ");
        pcf8754_i2c_read_byte(I2C_INPUT_ADDRESS, &byte);
        encRet = decode_rotary_encoder(byte, &dir, &button);
        if (encRet > 0)
        {
            if (dir == NO_CHANGE)
                os_printf("Button Pressed\r\n");
            else
            {
                switch(dir)
                {
                case ENCODER_CCW:
                    os_printf("Counter Clockwise rotation ");
                    break;

                case ENCODER_CW:
                    os_printf("Clockwise rotation ");
                    break;
                }
                if (button)
                    os_printf(" [Button Pressed]");

                os_printf("\r\n");
            }
        }

        //os_printf("0x%x\r\n", byte);

        //clear interrupt status for GPIO0
        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(3));

        // Reactivate interrupts for GPIO0
        gpio_pin_intr_state_set(GPIO_ID_PIN(3), GPIO_PIN_INTR_ANYEDGE);
    }
}


void ICACHE_FLASH_ATTR user_init()
{
    // Initialize the GPIO subsystem.
    //UART_init(BIT_RATE_115200, BIT_RATE_115200, 0);
    //UART_SetPrintPort(UART0);
    stdout_init();

    i2c_master_gpio_init();

    user_set_station_config();

    pcf8754_i2c_write_byte(I2C_INPUT_ADDRESS, 0);
    // =================================================
    // Initialize GPIO2 and GPIO0 as GPIO
    // =================================================
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_GPIO3);
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0RXD_U);

    gpio_output_set(0, 0, 0, GPIO_ID_PIN(3)); // set set gpio 0 as input

    ETS_GPIO_INTR_DISABLE();
    // Attach interrupt handle to gpio interrupts.
    ETS_GPIO_INTR_ATTACH(gpio_intr_handler, NULL);
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(3)); // clear gpio status
    gpio_pin_intr_state_set(GPIO_ID_PIN(3), GPIO_PIN_INTR_ANYEDGE); // clear gpio status.
    ETS_GPIO_INTR_ENABLE(); // Enable interrupts by GPIO


	// register a callback function to let user code know that system
	// initialization is complete
	system_init_done_cb(&post_user_init_func);

    //Start os task
    system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
}
