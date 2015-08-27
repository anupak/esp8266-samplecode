#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "driver/uart.h"

#include "ip_addr.h"
#include "espconn.h"
#include "user_interface.h"

/* System task priority */
#define user_procTaskPrio        0

/* System Task */
static void user_procTask(os_event_t *events);

/* System Task Message Queue */
#define user_procTaskQueueLen    1
os_event_t    user_procTaskQueue[user_procTaskQueueLen];

volatile os_timer_t     timer;
static char *test_str = "Test Timer code";
void post_user_init_func();

/* Setup the Acess point settings */
void ICACHE_FLASH_ATTR user_set_station_config(void)
{
    struct station_config stationConf;
    stationConf.bssid_set = 0;
    //need not check MAC address of AP

    os_memcpy(&stationConf.ssid, SSID, 32);
    os_memcpy(&stationConf.password, PASSWORD, 64);
    wifi_station_set_config(&stationConf);

    //  wifi_set_event_handler_cb(wifi_handle_event_cb);
}


//Do nothing function
static void ICACHE_FLASH_ATTR user_procTask(os_event_t *events)
{
	uint8_t buffer[80];
	int recv_len = -1;
	int index;

    switch(events->sig)
    {
    case UART0_SIGNAL:  // Signal from uart stating data is available
        recv_len = UART_Recv(0, buffer, sizeof(buffer));

        if (recv_len>0)
        {
            buffer[recv_len] = 0;
            os_printf("Received: [%d] %s\r\n", recv_len, buffer);

            for (index = 0; index< recv_len; index ++)
                os_printf(" 0x%x ", buffer[index]);
            os_printf("\n");

        }
        break;

    default:
        os_delay_us(10);
    }

}



// Timer callback function, called when the timer expires
void timer_func(void *arg)
{
	char *str = (char *) arg;
	os_printf("\r\nTimer Callback func called with [%s] argument\r\n", str);


}

// Called once the system intialization is complete
void post_user_init_func()
{
	os_printf("post_user_init_func() called\r\n");

	// Create a timer which repeats iself and fires every 1 second.
    os_timer_disarm(&timer);

	// Assign a callback function and arm it
    os_timer_setfn(&timer, (os_timer_func_t *) timer_func, test_str);
    //os_timer_arm(&timer, 1500, 1);
}


void ICACHE_FLASH_ATTR user_init()
{
    // Initialize the GPIO subsystem.
    UART_init(BIT_RATE_115200, BIT_RATE_115200, 0);
    UART_SetPrintPort(UART0);

    user_set_station_config();
	// register a callback function to let user code know that system
	// initialization is complete
	system_init_done_cb(&post_user_init_func);

    //Start os task
    system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
}
