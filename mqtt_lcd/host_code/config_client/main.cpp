#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#else
#include <process.h>
#include <winsock2.h>
#define snprintf sprintf_s
#endif

#include <mosquitto.h>
#include <jansson.h>

#define BROKER_ADDRESS	"192.168.1.15"
#define BROKER_PORT		1883

void my_message_callback(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	json_t *root;
    json_error_t error;

	if (message->payloadlen)
	{
		fprintf(stderr, "In my_message_callback [Topic: %s] %s\n", message->topic, (char *)message->payload);
		root = json_loads((char *) message->payload, 0, &error);
		if (!root)
		{
			return;
		}

		if (json_is_object(root))
		{
			fprintf(stderr,"Received data is a objecti from %s\n",json_string_value(json_object_get(root,"id" )));
			
		}
		json_decref(root);
	}
	else
		fprintf(stderr, "In my_message_callback [Topic: %s] Empty Message\n", message->topic);

}

void my_connect_callback(struct mosquitto *mosq, void *obj, int result)
{
	fprintf(stderr, "In my_connect_callback: %d\n", result);
	if (!result)
	{
		mosquitto_subscribe(mosq, NULL, "/announce", 0);
	}
}

int main()
{
	struct mosquitto *mosq = NULL;
	int rc;

	mosquitto_lib_init();

	mosq = mosquitto_new("announce_server" , 1, NULL);
    if(!mosq)
	{
        switch(errno)
		{
            case ENOMEM:
                fprintf(stderr, "Error: Out of memory.\n");
                break;
            case EINVAL:
                fprintf(stderr, "Error: Invalid id and/or clean_session.\n");
                break;
        } 
        mosquitto_lib_cleanup();


        return 1;
    }   

	mosquitto_connect_callback_set(mosq, my_connect_callback);
    mosquitto_message_callback_set(mosq, my_message_callback);

	rc = mosquitto_connect(mosq, BROKER_ADDRESS, BROKER_PORT, 2);
	if(rc == MOSQ_ERR_ERRNO)
	{	
		fprintf(stderr,"Could not connect to broker\n");
		return 1;
	}

	rc = mosquitto_loop_forever(mosq, -1, 1);

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
}
