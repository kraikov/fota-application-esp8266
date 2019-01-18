#ifndef __RBOOT_API_H__
#define __RBOOT_API_H__

//----------------------------------------------------------------------------------------------------------------------
// Included files to resolve specific definitions in this file
//----------------------------------------------------------------------------------------------------------------------
#include "BootloaderDriver.h"

//----------------------------------------------------------------------------------------------------------------------
// Exported type
//----------------------------------------------------------------------------------------------------------------------

typedef struct
{
    uint32 StartAddress;
    uint32 StartSector;
    int32 LastErasedSector;
    uint8 ExtraCount;
    uint8 ExtraBytes[4];
} WriteStatus;

//======================================================================================================================
// EXPORTED FUNCTIONS
//======================================================================================================================

BootConfiguration ICACHE_FLASH_ATTR GetConfiguration(void);

bool ICACHE_FLASH_ATTR SetConfiguration(BootConfiguration *conf);

uint8 ICACHE_FLASH_ATTR GetCurrentROM(void);

bool ICACHE_FLASH_ATTR SetCurrentROM(uint8 rom);

WriteStatus ICACHE_FLASH_ATTR WriteStatusInit(uint32 start_addr);

bool ICACHE_FLASH_ATTR WriteRemainingBytes(WriteStatus *status);

bool ICACHE_FLASH_ATTR WriteFlash(WriteStatus *status, uint8 *data, uint16 len);

bool ICACHE_FLASH_ATTR GetRTCData(RTCData *rtc);

bool ICACHE_FLASH_ATTR SetRTCData(RTCData *rtc);

bool ICACHE_FLASH_ATTR SetTempROM(uint8 rom);

bool ICACHE_FLASH_ATTR GetLastBootROM(uint8 *rom);

bool ICACHE_FLASH_ATTR GetLastBootMode(uint8 *mode);

#endif
