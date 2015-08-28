#include "c_types.h"
#include "mem.h"
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "ip_addr.h"
#include "espconn.h"

#include "user_interface.h"
#include "string.h"
#include "driver/uart.h"

#include "user_config.h"

#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
os_event_t    user_procTaskQueue[user_procTaskQueueLen];
static void user_procTask(os_event_t *events);

static char hwaddr[6];

static void ICACHE_FLASH_ATTR tcp_client_recv_cb(void *arg, char *data, unsigned short length)
{
    struct espconn *pespconn = (struct espconn *)arg;

    *(data+length) = 0;
    os_printf("tcp_client_recv_cb: Client says [%s] \n", data);

    espconn_disconnect(pespconn);
}

static void ICACHE_FLASH_ATTR tcp_client_sent_cb(void *arg)
{
    struct espconn *pespconn = (struct espconn *)arg;
    os_printf("tcp_client_sent_cb(): Data sent to client\n");
}

static void ICACHE_FLASH_ATTR tcp_client_discon_cb(void *arg)
{
    os_printf("tcp_client_discon_cb(): client disconnected\n");
    struct espconn *pespconn = (struct espconn *)arg;
}

/* Called wen a client connects to server */
static void ICACHE_FLASH_ATTR tcp_client_connected_cb(void *arg)
{
    char payload[128];
    struct espconn *pespconn = (struct espconn *)arg;

    os_printf("tcp_client_connected_cb(): Client connected\r\n");

	wifi_get_macaddr(0, hwaddr);

    espconn_regist_sentcb(pespconn, tcp_client_sent_cb);
    espconn_regist_disconcb(pespconn, tcp_client_discon_cb);
    espconn_regist_recvcb(pespconn, tcp_client_recv_cb);

    os_sprintf(payload, MACSTR" Says Hello!", MAC2STR(hwaddr));
    os_printf("%s\n", payload);

    //sent?!
    espconn_sent(pespconn, payload, os_strlen(payload));
}

//Priority 0 Task
static void ICACHE_FLASH_ATTR user_procTask(os_event_t *events)
{
    switch(events->sig)
    {
    case SIG_START_SERVER:
        os_printf("Starting TCP Server on %d port\r\n", SERVER_PORT);
        struct espconn *pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
        if (pCon == NULL)
        {
            os_printf("CONNECT FAIL\r\n");
            return;
        }

        /* Equivalent to bind */
        pCon->type  = ESPCONN_TCP;
        pCon->state = ESPCONN_NONE;
        pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
        pCon->proto.tcp->local_port = SERVER_PORT;

        /* Register callback when clients connect to the server */
        espconn_regist_connectcb(pCon, tcp_client_connected_cb);

        /* Put the connection in accept mode */
        espconn_accept(pCon);
        break;


    case SIG_UART0:
    case SIG_DO_NOTHING:
    default:
        // Intentionally ignoring these signals
        break;

    }
}

/* Callback called when the connection state of the module with an Access Point changes */
void wifi_handle_event_cb(System_Event_t *evt)
{
    os_printf("wifi_handle_event_cb: ");
    switch (evt->event)
    {
    case EVENT_STAMODE_CONNECTED:
        os_printf("connect to ssid %s, channel %d\n", evt->event_info.connected.ssid, evt->event_info.connected.channel);
        break;

    case EVENT_STAMODE_DISCONNECTED:
        os_printf("disconnect from ssid %s, reason %d\n", evt->event_info.disconnected.ssid, evt->event_info.disconnected.reason);
        break;

    case EVENT_STAMODE_AUTHMODE_CHANGE:
        os_printf("mode: %d -> %d\n", evt->event_info.auth_change.old_mode, evt->event_info.auth_change.new_mode);
        break;

    case EVENT_STAMODE_GOT_IP:
        os_printf("ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR, IP2STR(&evt->event_info.got_ip.ip), IP2STR(&evt->event_info.got_ip.mask), IP2STR(&evt->event_info.got_ip.gw));

        // Post a Server Start message as the IP has been acquired to Task with priority 0
        system_os_post(user_procTaskPrio, SIG_START_SERVER, 0 );
        break;

    case EVENT_SOFTAPMODE_STACONNECTED:
        os_printf("station: " MACSTR "join, AID = %d\n", MAC2STR(evt->event_info.sta_connected.mac),
        evt->event_info.sta_connected.aid);
        break;

    case EVENT_SOFTAPMODE_STADISCONNECTED:
        os_printf("station: " MACSTR "leave, AID = %d\n", MAC2STR(evt->event_info.sta_disconnected.mac),
        evt->event_info.sta_disconnected.aid);
        break;

    default:
        break;
    }
}

/* Setup the Acess point settings */
void ICACHE_FLASH_ATTR user_set_station_config(void)
{
    struct station_config stationConf;

    wifi_set_opmode(0x01);                      // Station Mode

    /* Setup AP credentials */
    stationConf.bssid_set = 0;
    //need not check MAC address of AP
    os_memcpy(&stationConf.ssid, SSID, 32);
    os_memcpy(&stationConf.password, PASSWORD, 64);
    wifi_station_set_config(&stationConf);

    wifi_set_event_handler_cb(wifi_handle_event_cb);

    /* Configure the module to Auto Connect to AP*/
	wifi_station_set_auto_connect(1);

}

void ICACHE_FLASH_ATTR user_init()
{
    gpio_init();

    user_set_station_config();

    // Initialize the GPIO subsystem.
    UART_init(BIT_RATE_115200, BIT_RATE_115200);

    //Start task
    system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
}
