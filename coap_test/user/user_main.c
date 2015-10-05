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
#include "coap.h"

#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
os_event_t    user_procTaskQueue[user_procTaskQueueLen];
static void user_procTask(os_event_t *events);

static char hwaddr[6];

static void ICACHE_FLASH_ATTR udpserver_recv_cb(void *arg,
                                                char *pusrdata,
                                                unsigned short len)
{
    struct espconn *pespconn = (struct espconn *)arg;

    coap_packet_t pkt;
    int8_t rc;
    uint8_t buf[256];
    uint8_t scratch_raw[256];
    coap_rw_buffer_t scratch_buf = {scratch_raw, sizeof(scratch_raw)};

#ifdef DEBUG
    os_printf("Received: ");
    coap_dump(pusrdata, len, true);
    os_printf("\n");
#endif
    if (0 != (rc = coap_parse(&pkt, pusrdata, len)))
        os_printf("Bad packet rc=%d\n", rc);
    else
    {
        size_t rsplen = sizeof(buf);
        coap_packet_t rsppkt;
#ifdef DEBUG
        coap_dumpPacket(&pkt);
#endif
        coap_handle_req(&scratch_buf, &pkt, &rsppkt);

        if (0 != (rc = coap_build(buf, &rsplen, &rsppkt)))
            os_printf("coap_build failed rc=%d\n", rc);
        else
        {
#ifdef DEBUG
            os_printf("Sending: ");
            coap_dump(buf, rsplen, true);
            os_printf("\n");

            coap_dumpPacket(&rsppkt);
#endif

            espconn_sent((struct espconn *) pespconn, buf, rsplen);

        }
    }

}

//Priority 0 Task
static void ICACHE_FLASH_ATTR user_procTask(os_event_t *events)
{
    struct ip_info ipconfig;
    sint8 retCode;

    switch(events->sig)
    {
    case SIG_START_SERVER:
        os_printf("Starting COAP Server on %d port\r\n", COAP_PORT);
        struct espconn *pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
        if (pCon == NULL)
        {
            os_printf("CONNECT FAIL\r\n");
            return;
        }

        pCon->type = ESPCONN_UDP;
        pCon->state = ESPCONN_NONE;

        pCon->proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));

        wifi_get_ip_info(STATION_IF, &ipconfig);
        os_memcpy(pCon->proto.udp->local_ip, &ipconfig.ip, 4);
        pCon->proto.udp->local_port = COAP_PORT;

        espconn_create(pCon);
        endpoint_setup();
        espconn_regist_recvcb(pCon, udpserver_recv_cb);

        retCode = espconn_accept(pCon);
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

    //Set GPIO2 to output mode
    user_set_station_config();

    // Initialize the GPIO subsystem.
    UART_init(BIT_RATE_115200, BIT_RATE_115200);

    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);

    //Set GPIO2 low
    gpio_output_set(0, BIT2, BIT2, 0);
    gpio_output_set(0, BIT0, BIT0, 0);

    //Start task
    system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
}
