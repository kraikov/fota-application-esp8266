//----------------------------------------------------------------------------------------------------------------------
// Included files to resolve specific definitions in this file
//----------------------------------------------------------------------------------------------------------------------
#include <osapi.h>
#include <user_interface.h>
#include "main.h"
#include "OTA_Manager.h"
#include "../drivers/UART_APP.h"
#include "MQTT_Wrapper.h"

//----------------------------------------------------------------------------------------------------------------------
// Local macros
//----------------------------------------------------------------------------------------------------------------------
#define debug
#define MQTT

#ifdef debug
#define WriteLine UART0_Send
#else
#define WriteLine
#endif

//----------------------------------------------------------------------------------------------------------------------
// Local function prototypes
//----------------------------------------------------------------------------------------------------------------------
static void ICACHE_FLASH_ATTR PrintSystemInfo();

static void ICACHE_FLASH_ATTR SwitchROM();

static void ICACHE_FLASH_ATTR OTA_UpdateCallBack(bool result, uint8 ROM);

static void ICACHE_FLASH_ATTR OTA_InvokeUpdate();
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
void ICACHE_FLASH_ATTR user_init(void)
{
    char message[50];
    UART_Init(BIT_RATE_74880, BIT_RATE_74880);
    WriteLine("\r\n\r\n================Firmware Over the Air================\r\n");
    os_sprintf(message, "\r\n====================Loading rom %d====================\r\n", GetCurrentROM());
    WriteLine(message);
    PrintSystemInfo();

#ifdef MQTT
    WiFi_Connect();
    MQTT_Init();
#endif
}

//======================================================================================================================
// DESCRIPTION:         Calling the user callback to indicate completion. Clean up at the end of the update.
//
// PARAMETERS:          Callback callback
//
// RETURN VALUE:        void
//
//======================================================================================================================
uint32 user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map)
    {
        case FLASH_SIZE_4M_MAP_256_256:
        {
            rf_cal_sec = 128 - 5;
            break;
        }
        case FLASH_SIZE_8M_MAP_512_512:
        {
            rf_cal_sec = 256 - 5;
            break;
        }
        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
        {
            rf_cal_sec = 512 - 5;
            break;
        }
        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
        {
            rf_cal_sec = 512 - 5;
            break;
        }
        default:
        {
            rf_cal_sec = 0;
            break;
        }
    }
    return rf_cal_sec;
}

//======================================================================================================================
// DESCRIPTION:         Calling the user callback to indicate completion. Clean up at the end of the update.
//
// PARAMETERS:          Callback callback
//
// RETURN VALUE:        void
//
//======================================================================================================================
void ICACHE_FLASH_ATTR user_rf_pre_init()
{
    // void user_rf_pre_init (void) is to be added in user_main.c since
    // ESP8266_NONOS_SDK_ V1.1.0 and is provided for RF initialization. User can call
    // system_phy_set_rfoption to set RF option in user_rf_pre_init, or call
    // system_deep_sleep_set_option before Deep-sleep. If RF is disabled, ESP8266 Station
    // and SoftAP will both be disabled, so the related APIs must not be called, and Wi-Fi function
    // can not be used either.
}

//======================================================================================================================
// DESCRIPTION:         Calling the user callback to indicate completion. Clean up at the end of the update.
//
// PARAMETERS:          Callback callback
//
// RETURN VALUE:        void
//
//======================================================================================================================
void ICACHE_FLASH_ATTR ParseCommand(char* command)
{
    if (0 == strcmp(command, "help"))
    {
        WriteLine("===========Available options===========\r\n");
        WriteLine("  help      - Displays this message\r\n");
        WriteLine("  connect   - Connect to WiFi with specified network name and password\r\n");
        WriteLine("  ip        - Show current connection's IP address\r\n");
        WriteLine("  restart   - restarts the device\r\n");
        WriteLine("  revert    - runs the other ROM image\r\n");
        WriteLine("  fota      - perform ota update, switch rom and reboot\r\n");
        WriteLine("  info      - show device information\r\n");
        WriteLine("\r\n");
    }
    else if (0 == strcmp(command, "connect"))
    {
        WiFi_Connect();
    }
    else if (0 == strcmp(command, "ip"))
    {
        GetIPAddress();
    }
    else if (0 == strcmp(command, "restart"))
    {
        WriteLine("Restarting...\r\n\r\n");
        system_restart();
    }
    else if (0 == strcmp(command, "revert"))
    {
        SwitchROM();
    }
    else if (0 == strcmp(command, "fota"))
    {
        OTA_InvokeUpdate();
    }
    else if (0 == strcmp(command, "info"))
    {
        PrintSystemInfo();
    }
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
static void ICACHE_FLASH_ATTR PrintSystemInfo()
{
    char message[50];

    os_sprintf(message, "System Chip ID:      0x%x\r\n", system_get_chip_id());
    WriteLine(message);

    os_sprintf(message, "SPI Flash ID:        0x%x\r\n", spi_flash_get_id());
    WriteLine(message);

    os_sprintf(message, "SPI Flash Size:      %d\r\n", (1 << ((spi_flash_get_id() >> 16) & 0xff)));
    WriteLine(message);

    os_sprintf(message, "CPU Frequency:       %d MHz\r\n", system_get_cpu_freq());
    WriteLine(message);

    os_sprintf(message, "SDK version:         %s\r\n", system_get_sdk_version());
    WriteLine(message);

    os_sprintf(message, "Current ROM:         %d\r\n", GetCurrentROM());
    WriteLine(message);
}

//======================================================================================================================
// DESCRIPTION:         Calling the user callback to indicate completion. Clean up at the end of the update.
//
// PARAMETERS:          Callback callback
//
// RETURN VALUE:        void
//
//======================================================================================================================
static void ICACHE_FLASH_ATTR SwitchROM()
{
    char message[50];
    MQTT_Client* Client = Get_MQTTClient();
    uint8 currentRom = GetCurrentROM();
    os_sprintf(message, "Switch ROM %d to ROM %d\r\n", currentRom, !currentRom);
    WriteLine(message);

    if (false == SetCurrentROM(!currentRom))
    {
        WriteLine("Unable to switch ROM...\r\n\r\n");
    }

    MQTT_PublishTopic("esp/revertdone", "Revert is completed", 19);
}

//======================================================================================================================
// DESCRIPTION:         Calling the user callback to indicate completion. Clean up at the end of the update.
//
// PARAMETERS:          Callback callback
//
// RETURN VALUE:        void
//
//======================================================================================================================
static void ICACHE_FLASH_ATTR OTA_UpdateCallBack(bool result, uint8 ROM)
{
    if (true == result)
    {
        char message[50];
        os_sprintf(message, "Software has been updated\r\nRebooting to ROM %d...\r\n", ROM);
        WriteLine(message);
        SetCurrentROM(ROM);
        DeactivateOTA();
        system_restart();

    }
    else
    {
        WriteLine("Software update has failed!\r\n");
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
static void ICACHE_FLASH_ATTR OTA_InvokeUpdate()
{
    if (ActivateOTA((Callback) OTA_UpdateCallBack))
    {
        WriteLine("Updating...\r\n");
    }
    else
    {
        WriteLine("Update has failed!\r\n\r\n");
    }
}
