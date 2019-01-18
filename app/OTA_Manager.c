//----------------------------------------------------------------------------------------------------------------------
// Included files to resolve specific definitions in this file
//----------------------------------------------------------------------------------------------------------------------
#include <c_types.h>
#include <user_interface.h>
#include <espconn.h>
#include <mem.h>
#include <osapi.h>
#include "OTA_Manager.h"

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
// Constant data
//----------------------------------------------------------------------------------------------------------------------
#define UPGRADE_FLAG_IDLE       0x00

#define UPGRADE_FLAG_START      0x01

#define UPGRADE_FLAG_FINISH     0x02

//----------------------------------------------------------------------------------------------------------------------
// Local types
//----------------------------------------------------------------------------------------------------------------------
typedef struct espconn ESPConnection;

typedef ip_addr_t IPAddress;

typedef err_t ErrorType;

typedef struct
{
    Callback UserCallback;  // user callback when completed
    ESPConnection* Connection;
    IPAddress IPAddress;
    WriteStatus WriteStatus;
    uint8 ROMSlot;   // rom slot to update, or FLASH_BY_ADDR
    uint32 Length;
    uint32 ContentLength;
} UpgradeStatus;

//----------------------------------------------------------------------------------------------------------------------
// Local function prototypes
//----------------------------------------------------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR OnDataReceived(void *arg, char *pusrdata, unsigned short length);

static void ICACHE_FLASH_ATTR OnDisconnect(void *arg);

static void ICACHE_FLASH_ATTR OnConnectionReceived(void *arg);

static void ICACHE_FLASH_ATTR OnConnectionTimeOut();

static void ICACHE_FLASH_ATTR OnConnectionLost(void *arg, ErrorType errorMessage);

static void ICACHE_FLASH_ATTR OnDNSFound(const char *name, IPAddress* IP, void *arg);

static const char* ICACHE_FLASH_ATTR GetErrorMessage(const ErrorType errorMessage);

//----------------------------------------------------------------------------------------------------------------------
// Local data
//----------------------------------------------------------------------------------------------------------------------
static UpgradeStatus* Upgrade;

static os_timer_t Timer;

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
bool ICACHE_FLASH_ATTR ActivateOTA(Callback callback)
{
    BootConfiguration bootconf;
    ErrorType errorMessage;

    // Check if there is an ongoing update
    if (UPGRADE_FLAG_START == system_upgrade_flag_check())
    {
        WriteLine("Ongoing update\r\n");
        return false;
    }

    // Create upgrade status structure
    Upgrade = (UpgradeStatus*) os_zalloc(sizeof(UpgradeStatus));
    if (NULL == Upgrade)
    {
        WriteLine("No ram!\r\n");
        return false;
    }

    // Store the callback
    Upgrade->UserCallback = callback;

    // Get the bootloader configuration
    bootconf = GetConfiguration();

    // Get details of rom slot to update
    Upgrade->ROMSlot = (bootconf.CurrentROM == 0 ? 1 : 0);

    // Initialize the flash write to the desired ROM
    Upgrade->WriteStatus = WriteStatusInit(bootconf.ROMS[Upgrade->ROMSlot]);

    // create connection
    Upgrade->Connection = (ESPConnection*) os_zalloc(sizeof(ESPConnection));
    if (NULL == Upgrade->Connection)
    {
        WriteLine("No ram!\r\n");
        os_free(Upgrade);
        return false;
    }

    Upgrade->Connection->proto.tcp = (esp_tcp *) os_zalloc(sizeof(esp_tcp));
    if (NULL == Upgrade->Connection->proto.tcp)
    {
        WriteLine("No ram!\r\n");
        os_free(Upgrade->Connection);
        os_free(Upgrade);
        return false;
    }

    // Set update flag
    system_upgrade_flag_set(UPGRADE_FLAG_START);

    // DNS lookup
    errorMessage = espconn_gethostbyname(Upgrade->Connection, OTA_HOST, &Upgrade->IPAddress, OnDNSFound);
    if (ESPCONN_OK == errorMessage)
    {
        WriteLine("ESPCONN_OK\r\n");
        // Hostname is a valid IP address string or the host name is already in the local names table.
        OnDNSFound(0, &Upgrade->IPAddress, Upgrade->Connection);
    }
    else if (ESPCONN_INPROGRESS == errorMessage)
    {
        // Enqueue a request to be sent to the DNS server for resolution if no errors are present.
    }
    else
    {
        // DNS client not initialized or invalid hostname
        WriteLine("DNS error!\r\n");
        os_free(Upgrade->Connection->proto.tcp);
        os_free(Upgrade->Connection);
        os_free(Upgrade);
        return false;
    }

    return true;
}

//======================================================================================================================
// LOCAL FUNCTIONS
//======================================================================================================================

//======================================================================================================================
// DESCRIPTION:         Calling the user callback to indicate completion. Clean up at the end of the update.
//
// PARAMETERS:          void
//
// RETURN VALUE:        void
//
//======================================================================================================================
void ICACHE_FLASH_ATTR DeactivateOTA(void)
{
    bool result;
    uint8 romSlot;
    Callback callback;
    ESPConnection* connection;

    os_timer_disarm(&Timer);

    // save only remaining bits of interest from upgrade struct
    // then we can clean it up early, so disconnect callback
    // can distinguish between us calling it after update finished
    // or being called earlier in the update process
    connection = Upgrade->Connection;
    romSlot = Upgrade->ROMSlot;
    callback = Upgrade->UserCallback;

    os_free(Upgrade);
    Upgrade = NULL;

    // If we have a connection, disconnect and clean up connection.
    if (NULL != connection)
    {
        espconn_disconnect(connection);
    }

    // Check if upgrade is completed.
    if (UPGRADE_FLAG_FINISH == system_upgrade_flag_check())
    {
        result = true;
    }
    else
    {
        system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
        result = false;
    }

    // Invoke the user callback function
    if (NULL != callback)
    {
        callback(result, romSlot);
    }
}

//======================================================================================================================
// DESCRIPTION:         Called when connection receives data
//
// PARAMETERS:
//
//
// RETURN VALUE:
//
//======================================================================================================================
static void ICACHE_FLASH_ATTR OnDataReceived(void *arg, char *pusrdata, unsigned short length)
{
    char* ptrData;
    char* ptrLen;
    char* ptr;

    // disarm the timer
    os_timer_disarm(&Timer);

    // first reply?
    if (0 == Upgrade->ContentLength)
    {
        // valid http response?
        if ((ptrLen = (char*) os_strstr(pusrdata, "Content-Length: ")) && (ptrData = (char*) os_strstr(ptrLen, "\r\n\r\n"))
                && (os_strncmp(pusrdata + 9, "200", 3) == 0))
        {

            // end of header/start of data
            ptrData += 4;
            // length of data after header in this chunk
            length -= (ptrData - pusrdata);
            // running total of download length
            Upgrade->Length += length;
            // process current chunk
            if (!WriteFlash(&Upgrade->WriteStatus, (uint8*) ptrData, length))
            {
                // write error
                DeactivateOTA();
                return;
            }
            // work out total download size
            ptrLen += 16;
            ptr = (char *) os_strstr(ptrLen, "\r\n");
            *ptr = '\0'; // destructive
            Upgrade->ContentLength = atoi(ptrLen);
        }
        else
        {
            // Invalid http header
            DeactivateOTA();
            return;
        }
    }
    else
    {
        // not the first chunk, process it
        Upgrade->Length += length;
        if (false == WriteFlash(&Upgrade->WriteStatus, (uint8*) pusrdata, length))
        {
            DeactivateOTA();
            return;
        }
    }

    // check if we are finished
    if (Upgrade->Length == Upgrade->ContentLength)
    {
        system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
        DeactivateOTA();
    }
    else if (ESPCONN_READ != Upgrade->Connection->state)
    {
        DeactivateOTA();
    }
    else
    {
        os_timer_setfn(&Timer, (os_timer_func_t *) DeactivateOTA, 0);
        os_timer_arm(&Timer, OTA_NETWORK_TIMEOUT, 0);
    }
}

//======================================================================================================================
// DESCRIPTION:         Disconnect callback, clean up the connection
//
// PARAMETERS:
//
//
// RETURN VALUE:
//
//======================================================================================================================
static void ICACHE_FLASH_ATTR OnDisconnect(void* arg)
{
    // use passed ptr, as upgrade struct may have gone by now
    ESPConnection* connection = (ESPConnection*) arg;

    os_timer_disarm(&Timer);
    if (NULL != connection)
    {
        if (NULL != connection->proto.tcp)
        {
            os_free(connection->proto.tcp);
        }
        os_free(connection);
    }

    // is upgrade struct still around?
    // if so disconnect was from remote end, or we called
    // ourselves to cleanup a failed connection attempt
    // must ensure disconnect was for this upgrade attempt,
    // not a previous one! this call back is async so another
    // upgrade struct may have been created already
    if ((NULL != Upgrade) && (Upgrade->Connection == connection))
    {
        Upgrade->Connection = NULL;

        DeactivateOTA();
    }
}

//======================================================================================================================
// DESCRIPTION:         Successfully connected to update server, send the request
//
// PARAMETERS:
//
//
// RETURN VALUE:
//
//======================================================================================================================
static void ICACHE_FLASH_ATTR OnConnectionReceived(void* arg)
{
    uint8* request;

    // disable the timeout
    os_timer_disarm(&Timer);

    // register connection callbacks
    espconn_regist_disconcb(Upgrade->Connection, OnDisconnect);

    espconn_regist_recvcb(Upgrade->Connection, OnDataReceived);

    // http request string
    request = (uint8*) os_malloc(512);
    if (NULL == request)
    {
        WriteLine("Not enough RAM available!\r\n");

        DeactivateOTA();

        return;
    }

    os_sprintf((char*) request, "GET /%s HTTP/1.1\r\nHost: " OTA_HOST "\r\n" HTTP_HEADER, (Upgrade->ROMSlot == 0 ? OTA_ROM0 : OTA_ROM1));
    WriteLine(request);

    // send the http request, with timeout for reply
    os_timer_setfn(&Timer, (os_timer_func_t *) DeactivateOTA, 0);

    os_timer_arm(&Timer, OTA_NETWORK_TIMEOUT, 0);

    espconn_sent(Upgrade->Connection, request, os_strlen((char*) request));

    os_free(request);
}

//======================================================================================================================
// DESCRIPTION:         Connection attempt timed out.
//
// PARAMETERS:          void
//
//
// RETURN VALUE:        void
//
//======================================================================================================================
static void ICACHE_FLASH_ATTR OnConnectionTimeOut(void)
{
    WriteLine("Connection time out!\r\n");

    OnDisconnect(Upgrade->Connection);
}

//======================================================================================================================
// DESCRIPTION:         Called when connection is lost.
//
// PARAMETERS:          sint8 errorType - type of the error
//
//
// RETURN VALUE:        void
//
//======================================================================================================================
static void ICACHE_FLASH_ATTR OnConnectionLost(void *arg, sint8 errorMessage)
{
    WriteLine("Connection lost! ");

    WriteLine(GetErrorMessage(errorMessage));

    WriteLine("\r\n");

    OnDisconnect(Upgrade->Connection);
}

//======================================================================================================================
// DESCRIPTION:         Called for DNS lookup.
//
// PARAMETERS:
//
//
// RETURN VALUE:        void
//
//======================================================================================================================
static void ICACHE_FLASH_ATTR OnDNSFound(const char* name, ip_addr_t* IP, void* arg)
{

    if (NULL == IP)
    {
        WriteLine("Cannot connect to the server: ");

        WriteLine(OTA_HOST);

        WriteLine("\r\n");

        OnDisconnect(Upgrade->Connection);

        return;
    }

    // set up connection
    Upgrade->Connection->type = ESPCONN_TCP;

    Upgrade->Connection->state = ESPCONN_NONE;

    Upgrade->Connection->proto.tcp->local_port = espconn_port();

    Upgrade->Connection->proto.tcp->remote_port = OTA_PORT;

    *(IPAddress*) Upgrade->Connection->proto.tcp->remote_ip = *IP;

    // set connection call backs
    espconn_regist_connectcb(Upgrade->Connection, OnConnectionReceived);

    espconn_regist_reconcb(Upgrade->Connection, OnConnectionLost);

    // try to connect
    espconn_connect(Upgrade->Connection);

    // set connection timeout timer
    os_timer_disarm(&Timer);

    os_timer_setfn(&Timer, (os_timer_func_t *) OnConnectionTimeOut, 0);

    os_timer_arm(&Timer, OTA_NETWORK_TIMEOUT, 0);
}

//======================================================================================================================
// DESCRIPTION:         Function that should be called when connection because of disconnect.
//
// PARAMETERS:          sint8 errorType - type of the error
//
//
// RETURN VALUE:        void
//
//======================================================================================================================
static const char* ICACHE_FLASH_ATTR GetErrorMessage(const sint8 errorMessage)
{
    switch (errorMessage)
    {
        case ESPCONN_OK:
            return "No error, everything OK.";
        case ESPCONN_MEM:
            return "Out of memory error.";
        case ESPCONN_TIMEOUT:
            return "Timeout.";
        case ESPCONN_RTE:
            return "Routing problem.";
        case ESPCONN_INPROGRESS:
            return "Operation in progress.";
        case ESPCONN_ABRT:
            return "Connection aborted.";
        case ESPCONN_RST:
            return "Connection reset.";
        case ESPCONN_CLSD:
            return "Connection closed.";
        case ESPCONN_CONN:
            return "Not connected.";
        case ESPCONN_ARG:
            return "Illegal argument.";
        case ESPCONN_ISCONN:
            return "Already connected.";
    }
}
