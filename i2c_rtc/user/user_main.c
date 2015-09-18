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

#include "rtc.h"

/* System task priority */
#define user_procTaskPrio        0

/* System Task */
static void user_procTask(os_event_t *events);
/* System Task Message Queue */
#define user_procTaskQueueLen    1
os_event_t    user_procTaskQueue[user_procTaskQueueLen];

volatile os_timer_t     timer;
void    post_user_init_func();

static void wifi_handle_event_cb(System_Event_t *evt);
uint32_t sntp_lib_init();
uint32_t get_sntp_time();

/*
   Intializes the SNTP library with servers to contact. I am having issues
   setting time zone to India (+5.5 hours), hence I get the GMT value and manually
   offset it (Dirty Hack :-).
 */
uint32_t sntp_lib_init()
{
    sntp_setservername(0, "in.pool.ntp.org"); // set server 0 by domain name
    sntp_setservername(1, "asia.pool.ntp.org"); // set server 1 by domain name
    sntp_set_timezone(0);
    sntp_init();
}

/*
   Gets the SNTP time from the server, there is a hack for offsetting the time
   to match india time zone (+5.5 hours)
 */
uint32_t sntp_get_time_timestamp()
{
    //struct tm  *ts;

    uint32 current_stamp;
    char buf[100];

    current_stamp = sntp_get_current_timestamp();

    /* HACK: Compensate for India offset of GMT +5.5 */
    current_stamp += 19800;

    return current_stamp;
}

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
        os_printf("\n");
        sntp_lib_init();
        /* Get time from NTP servers and set the RTC */
        os_timer_arm(&timer, 10000, 1);
        break;

    case EVENT_SOFTAPMODE_STACONNECTED: // Called when in Ap mode
        os_printf("Station: " MACSTR "joined, AID = %d\n", MAC2STR(evt->event_info.sta_connected.mac),
        evt->event_info.sta_connected.aid);
        break;

    case EVENT_SOFTAPMODE_STADISCONNECTED: // Called when in Ap mode
        os_printf("Station: " MACSTR "left, AID = %d\n", MAC2STR(evt->event_info.sta_disconnected.mac),
        evt->event_info.sta_disconnected.aid);
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
void timer_sync_func(void *arg)
{
    uint32_t rtc, sntp, delta;

    sntp = sntp_get_time_timestamp();
    rtc  = rtc_get_current_timestamp();

    if ((rtc == 0) && (sntp ==0)) return;

    delta = sntp - rtc;

    delta = (delta > 0 ? delta : -delta);
    os_printf("SNTP: %s RTC: %s\r\n", convert_epoc_to_str(sntp), convert_epoc_to_str(rtc));
    if (delta > 1)
    {
        os_printf("Divergence of clocks [%d seconds]: Setting RTC to NTP time\r\n", delta);
        rtc_set_current_timestamp(sntp);
    }
}

// Called once the system intialization is complete
void post_user_init_func()
{
    uint32_t rtc ;
    rtc  = rtc_get_current_timestamp();
	os_printf("[%s] post_user_init_func() called\r\n", convert_epoc_to_str(rtc));

    /* Simple Debug I2C scanner, not needed for the operation */
    i2c_bus_scan();

	// Create a timer which repeats iself and fires every 1 second.
    os_timer_disarm(&timer);

	// Assign a callback function and arm it
    os_timer_setfn(&timer, (os_timer_func_t *) timer_sync_func, NULL);
}


void ICACHE_FLASH_ATTR user_init()
{
    // Initialize the GPIO subsystem.
    UART_init(BIT_RATE_115200, BIT_RATE_115200, 0);
    UART_SetPrintPort(UART0);

    i2c_master_gpio_init();

    user_set_station_config();
	// register a callback function to let user code know that system
	// initialization is complete
	system_init_done_cb(&post_user_init_func);

    //Start os task
    system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
}
