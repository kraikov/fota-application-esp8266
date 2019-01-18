//----------------------------------------------------------------------------------------------------------------------
// Included files to resolve specific definitions in this file
//----------------------------------------------------------------------------------------------------------------------
#include <osapi.h>
#include <user_interface.h>
#include "main.h"
#include "OTA_Manager.h"
#include "../drivers/UART_APP.h"
#include "user_config.h"

//----------------------------------------------------------------------------------------------------------------------
// Local macros
//----------------------------------------------------------------------------------------------------------------------
#define debug

#ifdef debug
#define WriteLine UART0_Send
#else
#define WriteLine
#endif

//----------------------------------------------------------------------------------------------------------------------
// Local types
//----------------------------------------------------------------------------------------------------------------------
typedef struct ip_info IPInfo;
typedef os_timer_t Timer;
typedef struct station_config StationConfig;

//----------------------------------------------------------------------------------------------------------------------
// Local data
//----------------------------------------------------------------------------------------------------------------------
static Timer NetworkTimer;
static Timer DisconnectTimer;
static bool IsConnected = false;

//----------------------------------------------------------------------------------------------------------------------
// Local function prototypes
//----------------------------------------------------------------------------------------------------------------------
static void ICACHE_FLASH_ATTR RetryConnection(void);
static void ICACHE_FLASH_ATTR OnConnectionLost(void);
static uint8 ICACHE_FLASH_ATTR GetConnectionStatus(IPInfo* IPConfiguration, char* message);
//======================================================================================================================
// EXPORTED FUNCTIONS
//======================================================================================================================

//======================================================================================================================
// DESCRIPTION:         Calling the user callback to indicate completion. Clean up at the end of the update.
//
// PARAMETERS:          Callback callback
//
// RETURN VALUE:        void
//
//======================================================================================================================
void ICACHE_FLASH_ATTR WiFi_Connect()
{
    StationConfig stationConfiguration;

    wifi_set_opmode(STATION_MODE);

    stationConfiguration.bssid_set = 0;
    os_strcpy(&stationConfiguration.ssid, WIFI_SSID, os_strlen(WIFI_SSID));
    os_strcpy(&stationConfiguration.password, WIFI_PWD, os_strlen(WIFI_PWD));

    wifi_station_set_config(&stationConfiguration);

    WriteLine("Trying to establish connection with Wi-Fi...\r\n");

    wifi_station_connect();

    os_timer_disarm(&NetworkTimer);
    os_timer_setfn(&NetworkTimer, (os_timer_func_t *) RetryConnection, NULL);
    os_timer_arm(&NetworkTimer, 1000, 0);

    // On connection lost callback
    os_timer_disarm(&DisconnectTimer);
    os_timer_setfn(&DisconnectTimer, (os_timer_func_t *) OnConnectionLost, NULL);
    os_timer_arm(&DisconnectTimer, 10000, 1);

}

//======================================================================================================================
// DESCRIPTION:         Calling the user callback to indicate completion. Clean up at the end of the update.
//
// PARAMETERS:          Callback callback
//
// RETURN VALUE:        void
//
//======================================================================================================================
void ICACHE_FLASH_ATTR GetIPAddress()
{
    char message[100];
    IPInfo IPConfiguration;
    wifi_get_ip_info(STATION_IF, &IPConfiguration);
    GetConnectionStatus(&IPConfiguration, message);
    WriteLine(message);
}

//======================================================================================================================
// LOCAL FUNCTIONS
//======================================================================================================================

//======================================================================================================================
// DESCRIPTION:         Calling the user callback to indicate completion. Clean up at the end of the update.
//
// PARAMETERS:          Callback callback
//
// RETURN VALUE:        void
//
//======================================================================================================================
static void ICACHE_FLASH_ATTR OnConnectionLost(void)
{
    uint8 status = wifi_station_get_connect_status();
    IPInfo IPConfiguration;
    wifi_get_ip_info(STATION_IF, &IPConfiguration);

    if (true == IsConnected)
    {
        if ((0 == IPConfiguration.ip.addr) || (STATION_GOT_IP != status))
        {
            WriteLine("Connection lost!\r\n");
            IsConnected = false;
        }
    }
}

//======================================================================================================================
// DESCRIPTION:         Calling the user callback to indicate completion. Clean up at the end of the update.
//
// PARAMETERS:          Callback callback
//
// RETURN VALUE:        void
//
//======================================================================================================================
static void ICACHE_FLASH_ATTR RetryConnection(void)
{
    uint8 status;
    char message[200];
    IPInfo IPConfiguration;

    wifi_get_ip_info(STATION_IF, &IPConfiguration);

    status = GetConnectionStatus(&IPConfiguration, message);
    WriteLine(message);

    // Disable the callback
    os_timer_disarm(&NetworkTimer);

    if (status == STATION_GOT_IP && false == IsConnected)
    {
        MQTT_WiFiConnectCallback(status);
        IsConnected = true;
    }

    if ((0 == IPConfiguration.ip.addr) || (STATION_GOT_IP != status))
    {
        if (STATION_CONNECTING != status)
        {
            wifi_station_connect();
        }

        // Enable 1 second callback
        os_timer_setfn(&NetworkTimer, (os_timer_func_t *) RetryConnection, NULL);
        os_timer_arm(&NetworkTimer, 1000, 0);
    }
}

//======================================================================================================================
// DESCRIPTION:         Calling the user callback to indicate completion. Clean up at the end of the update.
//
// PARAMETERS:          Callback callback
//
// RETURN VALUE:        void
//
//======================================================================================================================
static uint8 ICACHE_FLASH_ATTR GetConnectionStatus(IPInfo* IPConfiguration, char* message)
{
    uint8 status = wifi_station_get_connect_status();

    switch (status)
    {
        case STATION_IDLE:
        {
            os_sprintf(message, "Station is in idle state\r\n");
            break;
        }
        case STATION_CONNECTING:
        {
            os_sprintf(message, "Trying to connect\r\n");
            break;
        }
        case STATION_WRONG_PASSWORD:
        {
            os_sprintf(message, "Wrong password\r\n");
            break;
        }
        case STATION_NO_AP_FOUND:
        {
            os_sprintf(message, "Network not found\r\n");
            break;
        }
        case STATION_CONNECT_FAIL:
        {
            os_sprintf(message, "Cannot connect to the network\r\n");
            break;
        }
        case STATION_GOT_IP:
        {
            if (0 != IPConfiguration->ip.addr)
            {
                os_sprintf(message, "IP: %d.%d.%d.%d\r\nMASK: %d.%d.%d.%d\r\nGATEWAY: %d.%d.%d.%d\r\n", IP2STR(&IPConfiguration->ip),
                        IP2STR(&IPConfiguration->netmask), IP2STR(&IPConfiguration->gw));
            }
            break;
        }
        default:
        {
            os_sprintf(message, "Unknown network error!\r\n");
            break;
        }
    }

    return status;
}
