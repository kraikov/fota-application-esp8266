#ifndef __BOOTLOADER_DRIVER__
#define __BOOTLOADER_DRIVER__

//----------------------------------------------------------------------------------------------------------------------
// Included files to resolve specific definitions in this file
//----------------------------------------------------------------------------------------------------------------------
#include <c_types.h>

//----------------------------------------------------------------------------------------------------------------------
// Constant data
//----------------------------------------------------------------------------------------------------------------------
#define DEFAULT_CHECKSUM 0xEF

#define SECTOR_SIZE 0x1000

#define BOOT_CONFIG_SECTOR 1

#define BOOT_CONFIG_MAGIC 0xA5EAF1C3

#define BOOT_CONFIG_VERSION 0x01

#define RTC_ADDRESS 0x40

#define MAX_ROMS 2

//----------------------------------------------------------------------------------------------------------------------
// Exported type
//----------------------------------------------------------------------------------------------------------------------

// Structure containing boot configuration
// ROM addresses must be multiples of 0x1000 (flash sector aligned).
typedef struct
{
    uint32 MagicNumber;         // Our magic, identifies rBoot configuration - should be BOOT_CONFIG_MAGIC
    uint8 Version;            // Version of configuration structure - should be BOOT_CONFIG_VERSION
    uint8 CurrentROM;        // Currently selected ROM (will be used for next standard boot)
    uint8 Count;            // Quantity of ROMs available to boot
    uint32 ROMS[MAX_ROMS];  // Flash addresses of each ROM
} BootConfiguration;

// Structure containing rBoot status/control data.
// This structure is used to, communicate between bootloader and
// the user app. It is stored in the ESP RTC data area.
typedef struct
{
    uint32 MagicNumber;      // Magic, identifies rBoot RTC data - should be RBOOT_RTC_MAGIC
    uint8 CheckSum;         // Checksum of this structure this will be updated for you passed to the API
} RTCData;

#endif
