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


#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
os_event_t    user_procTaskQueue[user_procTaskQueueLen];
static void user_procTask(os_event_t *events);

/* Hold the sytem wise configuration */
sysconfig_t config;

MQTT_Client mqttClient;

//Priority 0 Task
static void ICACHE_FLASH_ATTR user_procTask(os_event_t *events)
{
    switch(events->sig)
    {
    case SIG_IP_GOT:
        /* Establish the MQTT connection */
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
        // Post a Server Start message as the IP has been acquired to Task with priority 0
        system_os_post(user_procTaskPrio, SIG_IP_GOT, 0 );
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
    os_sprintf(stationConf.ssid, "%s", config.ssid);
    os_sprintf(stationConf.password, "%s", config.password);
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

	MQTT_Subscribe(client, "/esp8266/alarms", 0);
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

struct alarm_event
{
    uint16_t    alarmid;
    char        desc[100];
    uint8_t     timesout_in_secs;
    uint8_t     needs_ack;
    char        alarm_ack_topic[100];
    MQTT_Client* client;
    os_timer_t  timer;  //  Used to control timeout.
};

struct alarm_event global_alarm;

int alarm_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser);

LOCAL struct jsontree_callback alarm_callback = JSONTREE_CALLBACK(NULL, alarm_set);

JSONTREE_OBJECT(alarm_tree,
                JSONTREE_PAIR("alarmid", &alarm_callback),
                JSONTREE_PAIR("acknowledgement", &alarm_callback),
                JSONTREE_PAIR("description", &alarm_callback),
                JSONTREE_PAIR("timeout", &alarm_callback),
               );

int alarm_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
    int type;

    while ((type = jsonparse_next(parser)) != 0)
    {
        if (type == JSON_TYPE_PAIR_NAME)
        {
            if (jsonparse_strcmp_value(parser, "alarmid") == 0)
            {
                jsonparse_next(parser);
                jsonparse_next(parser);
                //os_printf("Alarm present");
                global_alarm.alarmid = jsonparse_get_value_as_int(parser);
            }

            if (jsonparse_strcmp_value(parser, "description") == 0)
            {
                jsonparse_next(parser);
                jsonparse_next(parser);
                //os_printf("Description present");
                jsonparse_copy_value(parser, global_alarm.desc, 100);
            }

            if (jsonparse_strcmp_value(parser, "acktopic") == 0)
            {
                jsonparse_next(parser);
                jsonparse_next(parser);
                //os_printf("Ack Topic present");
                jsonparse_copy_value(parser, global_alarm.alarm_ack_topic, 100);
                global_alarm.needs_ack = 1;
            }

            if (jsonparse_strcmp_value(parser, "timeout") == 0)
            {
                jsonparse_next(parser);
                jsonparse_next(parser);
                //os_printf("Description present");
                global_alarm.timesout_in_secs = jsonparse_get_value_as_int(parser);
            }

        }

    }
    return 0;
}

void alarm_timeout(void *arg)
{
    global_alarm.needs_ack = 0;
    global_alarm.desc[0] = 0;
    global_alarm.alarm_ack_topic[0] = 0;
    gpio_output_set(0, BIT2, BIT2, 0);  //Set GPIO2 low
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

        // Print a debug message
		os_printf("Button Pushed\r\n");

        //clear interrupt status for GPIO0
        GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(0));

        // Reactivate interrupts for GPIO0
        //gpio_pin_intr_state_set(GPIO_ID_PIN(0), GPIO_PIN_INTR_ANYEDGE);
        gpio_output_set(0, BIT2, BIT2, 0);  //Set GPIO2 low

        if (global_alarm.needs_ack)
        {
            os_printf("Sending ACK\r\n");
        	MQTT_Publish(global_alarm.client, global_alarm.alarm_ack_topic, "hello0", 6, 0, 0);
        }
    }
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

    global_alarm.needs_ack = 0;
    global_alarm.desc[0] = 0;
    global_alarm.alarm_ack_topic[0] = 0;
    global_alarm.timesout_in_secs = 10;
    global_alarm.client = NULL;
    os_timer_disarm(&global_alarm.timer);
    os_timer_setfn(&global_alarm.timer, (os_timer_func_t *) alarm_timeout, NULL);

	jsontree_setup(&js, (struct jsontree_value *)&alarm_tree, json_putchar);
	json_parse(&js, dataBuf);

    gpio_output_set(BIT2, 0, BIT2, 0);

    // clear gpio status. Say ESP8266EX SDK Programming Guide in  5.1.6. GPIO interrupt handler
    gpio_pin_intr_state_set(GPIO_ID_PIN(0), GPIO_PIN_INTR_ANYEDGE);
    ETS_GPIO_INTR_ENABLE();     // Enable interrupts by GPIO

    if (global_alarm.needs_ack)
    {
        os_printf("Alarm: %s (needing acknowledgement) on %s\n", global_alarm.desc, global_alarm.alarm_ack_topic);
        global_alarm.client = client;
    }
    else
    {
        os_printf("Alarm: %s [%d seconds]\n", global_alarm.desc, global_alarm.timesout_in_secs);
        os_timer_arm(&global_alarm.timer, global_alarm.timesout_in_secs * 1000, 0);  // Intiailization Function
    }

	os_free(topicBuf);
	os_free(dataBuf);
}

void ICACHE_FLASH_ATTR user_init()
{
    gpio_init();
    // Initialize the GPIO subsystem.
    UART_init(BIT_RATE_115200, BIT_RATE_115200);
    os_printf("SDK version:%s\n", system_get_sdk_version());

    /* GPIO2 as the output pin */
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    gpio_output_set(0, BIT2, BIT2, 0);  //Set GPIO2 low

    /* GPIO-0 as input */
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO0_U);
    PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO2_U);
    gpio_output_set(0, 0, 0, GPIO_ID_PIN(0)); // set set gpio 0 as input

    // Disable interrupts by GPIO
    ETS_GPIO_INTR_DISABLE();
    // Attach interrupt handle to gpio interrupts.
    ETS_GPIO_INTR_ATTACH(gpio_intr_handler, NULL);
    // clear gpio status. Say ESP8266EX SDK Programming Guide in  5.1.6. GPIO interrupt handler
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(0));

    // clear gpio status. Say ESP8266EX SDK Programming Guide in  5.1.6. GPIO interrupt handler
    gpio_pin_intr_state_set(GPIO_ID_PIN(0), GPIO_PIN_INTR_ANYEDGE);


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
