#ifndef _USER_CONFIG_
#define _USER_CONFIG_


typedef enum {SIG_DO_NOTHING=0, SIG_UART0, SIG_IP_GOT, SIG_IP_LOST} USER_SIGNALS;

/* Default Configurations */
#define WIFI_SSID            "SSID"
#define WIFI_PASSWORD        "PASSWORD"

#define MQTT_HOST           "192.168.1.15" //or "mqtt.yourdomain.com"
#define MQTT_PORT           1883
#define MQTT_BUF_SIZE       1024
#define MQTT_KEEPALIVE      120  /*second*/

#define MQTT_CLIENT_ID      "DVES_%08X"
#define MQTT_USER           "DVES_USER"
#define MQTT_PASS           "DVES_PASS"

#define MQTT_RECONNECT_TIMEOUT  5   /*second*/

#define DEFAULT_SECURITY    0
#define QUEUE_BUFFER_SIZE               2048

#define PROTOCOL_NAMEv31    /*MQTT version 3.1 compatible with Mosquitto v0.15*/
//PROTOCOL_NAMEv311         /*MQTT version 3.11 compatible with https://eclipse.org/paho/clients/testing/*/

#endif
