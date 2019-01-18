#ifndef __OTA_MANAGER_H__
#define __OTA_MANAGER_H__

//----------------------------------------------------------------------------------------------------------------------
// Included files to resolve specific definitions in this file
//----------------------------------------------------------------------------------------------------------------------
#include "../drivers/Bootloader.h"

//----------------------------------------------------------------------------------------------------------------------
// Constant data
//----------------------------------------------------------------------------------------------------------------------
// ota server details
#define OTA_HOST "192.168.43.1"
#define OTA_PORT 12345
#define OTA_ROM0 "user_0.bin"
#define OTA_ROM1 "user_1.bin"

// general http header
#define HTTP_HEADER "Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
User-Agent: esp8266/1.0\r\n\
Accept: */*\r\n\r\n"
/* this comment to keep notepad++ happy */

// timeout for the initial connect and each recv (in ms)
#define OTA_NETWORK_TIMEOUT  10000

//----------------------------------------------------------------------------------------------------------------------
// Exported type
//----------------------------------------------------------------------------------------------------------------------
// callback method should take this format
typedef void (*Callback)(bool result, uint8 rom_slot);

//======================================================================================================================
// EXPORTED FUNCTIONS
//======================================================================================================================
// function to perform the ota update
bool ICACHE_FLASH_ATTR ActivateOTA(Callback callback);
void ICACHE_FLASH_ATTR DeactivateOTA(void);

#endif
