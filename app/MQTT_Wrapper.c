//----------------------------------------------------------------------------------------------------------------------
// Included files to resolve specific definitions in this file
//----------------------------------------------------------------------------------------------------------------------
#include <user_interface.h>
#include "osapi.h"
#include "../mqtt/mqtt.h"
#include "../mqtt/debug.h"
#include "gpio.h"
#include "mem.h"
#include "user_config.h"
#include "../drivers/UART_APP.h"
#include "MQTT_Wrapper.h"

//----------------------------------------------------------------------------------------------------------------------
// Local macros
//----------------------------------------------------------------------------------------------------------------------
#define debug

#ifdef debug
#define WriteLine UART0_Send
#else
#define WriteLine
#endif

typedef void (*PublishCallback)();

//----------------------------------------------------------------------------------------------------------------------
// Local function prototypes
//----------------------------------------------------------------------------------------------------------------------
static void ICACHE_FLASH_ATTR MQTT_ConnectedCallback(uint32* args);
static void ICACHE_FLASH_ATTR MQTT_DisconnectedCallback(uint32* args);
static void ICACHE_FLASH_ATTR MQTT_PublishCallback(uint32* args);
static void ICACHE_FLASH_ATTR MQTT_DataCallback(uint32* args, const char* topic, uint32 topic_len, const char* data, uint32 data_len);

static MQTT_Client Client;

MQTT_Client* Get_MQTTClient(void)
{
    return &Client;
}

void ICACHE_FLASH_ATTR MQTT_WiFiConnectCallback(uint8 status)
{
    if (STATION_GOT_IP == status)
    {
        WriteLine("MQTT connecting...");

        MQTT_Connect(&Client);
    }
    else
    {
        WriteLine("MQTT disconnecting...");
        MQTT_Disconnect(&Client);
    }
}

static void ICACHE_FLASH_ATTR MQTT_ConnectedCallback(uint32* args)
{
    MQTT_Client* client = (MQTT_Client*) args;

    WriteLine("MQTT: Connected, subscribing and publishing\r\n");

    MQTT_Subscribe(client, UpdateTopic, 2);

    MQTT_Subscribe(client, RevertTopic, 2);
}

static void ICACHE_FLASH_ATTR MQTT_DisconnectedCallback(uint32* args)
{
    MQTT_Client* client = (MQTT_Client*) args;

    WriteLine("MQTT: Disconnected\r\n");
}

static void ICACHE_FLASH_ATTR MQTT_PublishCallback(uint32* args)
{
    MQTT_Client* client = (MQTT_Client*) args;

    WriteLine("MQTT: Published...");
}

static void ICACHE_FLASH_ATTR MQTT_DataCallback(uint32* args, const char* topic, uint32 topic_len, const char* data, uint32 data_len)
{
    char* topicBuf = (char*) os_zalloc(topic_len + 1), *dataBuf = (char*) os_zalloc(data_len + 1);

    MQTT_Client* client = (MQTT_Client*) args;

    os_memcpy(topicBuf, topic, topic_len);

    topicBuf[topic_len] = 0;

    os_memcpy(dataBuf, data, data_len);

    dataBuf[data_len] = 0;

    if (0 == strcmp(topicBuf, UpdateTopic))
    {
        ParseCommand("fota");
    }

    if (0 == strcmp(topicBuf, RevertTopic))
    {
        ParseCommand("revert");
    }

    os_free(topicBuf);

    os_free(dataBuf);
}

void MQTT_PublishTopic(const char* topic, const char* data, int data_length)
{
    MQTT_Publish(&Client, topic, data, data_length, 0, 0);
}

void ICACHE_FLASH_ATTR MQTT_Init(void)
{
    MQTT_InitConnection(&Client, MQTT_HOST, MQTT_PORT, DEFAULT_SECURITY);

    MQTT_InitLWT(&Client, "/lwt", "offline", 0, 0);

    MQTT_OnConnected(&Client, MQTT_ConnectedCallback);

    MQTT_OnDisconnected(&Client, MQTT_DisconnectedCallback);

    MQTT_OnPublished(&Client, MQTT_PublishCallback);

    MQTT_OnData(&Client, MQTT_DataCallback);

    MQTT_InitClient(&Client, MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS, MQTT_KEEPALIVE, MQTT_CLEAN_SESSION);
}

