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
#include "config_flash.h"
#include "mqtt.h"

#include "user_json.h"

#ifndef INFO
#define INFO os_printf
#endif

#include "driver/i2c_master.h"  // Provided in examples/IoT sample
#include "i2c_lcd.h"


#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
os_event_t    user_procTaskQueue[user_procTaskQueueLen];
static void user_procTask(os_event_t *events);

/* Hold the sytem wise configuration */
sysconfig_t config;

#define MAX_DISPLAY_LEN (LCD_LINES*LCD_DISP_LENGTH+1)
struct DisplayInfo
{
    char    display_info[MAX_DISPLAY_LEN];
    uint8_t gpio;
} global_display;

MQTT_Client mqttClient;

//Priority 0 Task
static void ICACHE_FLASH_ATTR user_procTask(os_event_t *events)
{
    switch(events->sig)
    {
    case SIG_IP_GOT:
        /* Establish the MQTT connection */
        lcd_clrscr();
        lcd_gotoxy(0,0);
        lcd_puts("Connecting to MQTT broker");
        MQTT_Connect(&mqttClient);
        break;

    case SIG_IP_LOST:
        /* Clean up the MQTT connection */
        MQTT_Disconnect(&mqttClient);
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
        lcd_clrscr();
        lcd_gotoxy(0,0);
        lcd_puts("Connected: ");
        lcd_puts(evt->event_info.connected.ssid);
        break;

    case EVENT_STAMODE_DISCONNECTED:
        os_printf("disconnect from ssid %s, reason %d\n", evt->event_info.disconnected.ssid, evt->event_info.disconnected.reason);
        system_os_post(user_procTaskPrio, SIG_IP_LOST, 0 );
        break;

    case EVENT_STAMODE_AUTHMODE_CHANGE:
        os_printf("mode: %d -> %d\n", evt->event_info.auth_change.old_mode, evt->event_info.auth_change.new_mode);
        break;

    case EVENT_STAMODE_GOT_IP:
        os_printf("ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR, IP2STR(&evt->event_info.got_ip.ip), IP2STR(&evt->event_info.got_ip.mask), IP2STR(&evt->event_info.got_ip.gw));
        lcd_clrscr();
        lcd_gotoxy(0,0);
        lcd_puts("Got IP Address");
        system_os_post(user_procTaskPrio, SIG_IP_GOT, 0 );
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
    os_sprintf(stationConf.ssid,        "%s", config.ssid);
    os_sprintf(stationConf.password,    "%s", config.password);
    wifi_station_set_config(&stationConf);

    wifi_set_event_handler_cb(wifi_handle_event_cb);

    /* Configure the module to Auto Connect to AP*/
    if (config.auto_connect)
        wifi_station_set_auto_connect(1);
    else
        wifi_station_set_auto_connect(0);
}

void mqttConnectedCb(uint32_t *args)
{
    char announce_message[255];
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Connected\r\n");

    lcd_clrscr();
    lcd_gotoxy(0,0);
    lcd_puts("Connected to MQTT broker");

	MQTT_Subscribe(client, "/esp8266/display", 0);
}

void mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Disconnected\r\n");
}

void mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Published\r\n");
}

int display_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser);

LOCAL struct jsontree_callback display_callback = JSONTREE_CALLBACK(NULL, display_set);

JSONTREE_OBJECT(alarm_tree,
                JSONTREE_PAIR("information", &display_callback),
                JSONTREE_PAIR("gpio", &display_callback),
               );

int display_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
    int type;

    while ((type = jsonparse_next(parser)) != 0)
    {
        if (type == JSON_TYPE_PAIR_NAME)
        {
            if (jsonparse_strcmp_value(parser, "information") == 0)
            {
                jsonparse_next(parser);
                jsonparse_next(parser);
                //os_printf("Display Information present");
                jsonparse_copy_value(parser, global_display.display_info, MAX_DISPLAY_LEN);   // LCD
            }

            if (jsonparse_strcmp_value(parser, "gpio-1") == 0)
            {
                jsonparse_next(parser);
                jsonparse_next(parser);
                //os_printf("GPIO Output present");
                global_display.gpio = jsonparse_get_value_as_int(parser);
            }
        }

    }
    return 0;
}

void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
    struct jsontree_context js;
	char *topicBuf = (char*)os_zalloc(topic_len+1),
         *dataBuf = (char*)os_zalloc(data_len+1);

	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;


	INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);
    global_display.display_info[0] = 0;
	jsontree_setup(&js, (struct jsontree_value *)&alarm_tree, json_putchar);
	json_parse(&js, dataBuf);

    /* Add code here */
    lcd_clrscr();
    lcd_gotoxy(0,0);
    lcd_puts(global_display.display_info);


	os_free(topicBuf);
	os_free(dataBuf);
}

void ICACHE_FLASH_ATTR user_init()
{
    UART_init(BIT_RATE_115200, BIT_RATE_115200);
    os_printf("SDK version:%s\n", system_get_sdk_version());

    i2c_master_gpio_init();
    lcd_init(LCD_DISP_ON);

    lcd_clrscr();
    lcd_gotoxy(0,0);
    lcd_puts("Intializing ...\nConnecting to router...");


    /* Load the system Configuration */
    config_load(0, &config);
    user_set_station_config();

	MQTT_InitConnection(&mqttClient, config.mqtt_host, config.mqtt_port, config.security);
	MQTT_InitClient(&mqttClient, config.device_id, config.mqtt_user, config.mqtt_pass, config.mqtt_keepalive, 1);

	MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
	MQTT_OnConnected(&mqttClient,       mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient,    mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient,       mqttPublishedCb);
	MQTT_OnData(&mqttClient,            mqttDataCb);

    //Start task
    system_os_task(user_procTask, user_procTaskPrio,user_procTaskQueue, user_procTaskQueueLen);
}
