#ifndef _USER_CONFIG_
#define _USER_CONFIG_

typedef enum {SIG_DO_NOTHING=0, SIG_START_SERVER=1, SIG_SEND_DATA, SIG_UART0, SIG_CONSOLE_RX, SIG_CONSOLE_TX } USER_SIGNALS;

#define WIFI_SSID            "YourSSID"
#define WIFI_PASSWORD        "YourPassword"
//#define SERVER_IP		"192.168.1.15"
#define SERVER_PORT		7777


#endif
