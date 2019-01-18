//----------------------------------------------------------------------------------------------------------------------
// Included files to resolve specific definitions in this file
//----------------------------------------------------------------------------------------------------------------------
#include <string.h>
#include <c_types.h>
#include <spi_flash.h>
#include <mem.h>
#include "Bootloader.h"

//----------------------------------------------------------------------------------------------------------------------
// Local function prototypes
//----------------------------------------------------------------------------------------------------------------------

static uint8 GetCheckSum(uint8 const *start, uint8 const * const end);

//======================================================================================================================
// LOCAL FUNCTIONS
//======================================================================================================================

//======================================================================================================================
// DESCRIPTION:         Calculate checksum for block of data from start up to (but excluding) end
//
// PARAMETERS:          uint8 const *start - from here to end
//                      uint8 const * const end - from start up to (but excluding) end
//
// RETURN VALUE:        Calculated checksum
//
//======================================================================================================================
static uint8 GetCheckSum(uint8 const *start, uint8 const * const end)
{
    uint8 chksum = DEFAULT_CHECKSUM;
    while (start < end)
    {
        chksum ^= *start;
        start++;
    }
    return chksum;
}

//======================================================================================================================
// EXPORTED FUNCTIONS
//======================================================================================================================

//======================================================================================================================
// DESCRIPTION:         Gets the configuration written in FLASH
//
// PARAMETERS:          void
//
// RETURN VALUE:        BootConfiguration - the configuration written in FLASH
//
//======================================================================================================================
BootConfiguration ICACHE_FLASH_ATTR GetConfiguration(void)
{
    BootConfiguration configuration;

    spi_flash_read(BOOT_CONFIG_SECTOR * SECTOR_SIZE, (uint32*) &configuration, sizeof(BootConfiguration));

    return configuration;
}

//======================================================================================================================
// DESCRIPTION:         Write the boot configuration.
//
// PARAMETERS:          BootConfiguration - the configuration to be written in FLASH
//
// RETURN VALUE:        bool - true if writing is successfull
//                             false if writing is not successfull
//
//======================================================================================================================
bool ICACHE_FLASH_ATTR SetConfiguration(BootConfiguration* configuration)
{
    uint8* buffer = (uint8*) os_malloc(SECTOR_SIZE);

    if (!buffer)
    {
        // Not enough RAM memory available
        return false;
    }

    spi_flash_read(BOOT_CONFIG_SECTOR * SECTOR_SIZE, (uint32*) ((void*) buffer), SECTOR_SIZE);

    memcpy(buffer, configuration, sizeof(BootConfiguration));

    spi_flash_erase_sector(BOOT_CONFIG_SECTOR);

    spi_flash_write(BOOT_CONFIG_SECTOR * SECTOR_SIZE, (uint32*) ((void*) buffer), SECTOR_SIZE);

    os_free(buffer);

    return true;
}

//======================================================================================================================
// DESCRIPTION:         Get current boot rom
//
// PARAMETERS:          void
//
// RETURN VALUE:        uint8 - number of currently used ROM
//
//======================================================================================================================
uint8 ICACHE_FLASH_ATTR GetCurrentROM(void)
{
    BootConfiguration configuration;

    configuration = GetConfiguration();

    return configuration.CurrentROM;
}

//======================================================================================================================
// DESCRIPTION:         uint8 rom - ROM to be used
//
// PARAMETERS:          void
//
// RETURN VALUE:        bool - true if modification was successfull
//
//======================================================================================================================
bool ICACHE_FLASH_ATTR SetCurrentROM(uint8 rom)
{
    BootConfiguration configuration;

    configuration = GetConfiguration();

    if (rom >= configuration.Count)
    {
        return false;
    }

    configuration.CurrentROM = rom;

    return SetConfiguration(&configuration);
}

//======================================================================================================================
// DESCRIPTION:         Create the write status struct, based on supplied start address.
//                      Call once before starting to pass data to write to flash memory with WriteFlash function.
//
// PARAMETERS:          uint32 startAddress - Address on the SPI flash to begin write to
//
// RETURN VALUE:        WriteStatus
//
//======================================================================================================================
WriteStatus ICACHE_FLASH_ATTR WriteStatusInit(uint32 startAddress)
{
    WriteStatus status = { 0 };

    status.StartAddress = startAddress;

    status.StartSector = startAddress / SECTOR_SIZE;

    status.LastErasedSector = status.StartSector - 1;

    return status;
}

//======================================================================================================================
// DESCRIPTION:         Complete flash write process. Call at the completion of flash writing.
//                      This ensures any remaning bytes get written (needed for files not a multiple of 4 bytes).
//
// PARAMETERS:          WriteStatus *status
//
// RETURN VALUE:        void
//
//======================================================================================================================
bool ICACHE_FLASH_ATTR WriteRemainingBytes(WriteStatus *status)
{
    uint8 loopIndex;
    if (0 != status->ExtraCount)
    {
        for (loopIndex = status->ExtraCount; loopIndex < 4; loopIndex++)
        {
            status->ExtraBytes[loopIndex] = 0xFF;
        }
        return WriteFlash(status, status->ExtraBytes, 4);
    }
    return true;
}

//======================================================================================================================
// DESCRIPTION:         Function to do the actual writing to flash.
//                      Call repeatedly with more data (max len per write is the flash sector size (4k))
//                      Call WriteStatusInit before calling this function to get the WriteStatus structure.
//                      Current write position is tracked automatically.
//                      This method is likely to be called each time a packet of OTA data is received over the network.
//
// PARAMETERS:          WriteStatus *status - Pointer to structure defining the write status
//                      uint8 *data - Pointer to a block of uint8 data elements to be written to flash
//                      uint16 length - Quantity of uint8 data elements to write to flash
//
//
// RETURN VALUE:        void
//
//======================================================================================================================
bool ICACHE_FLASH_ATTR WriteFlash(WriteStatus *status, uint8* data, uint16 length)
{
    bool isOK = false;
    uint8* buffer;
    int32 lastSector;

    if ((NULL == data) || (0 == length))
    {
        return true;
    }

    // get a buffer
    buffer = (uint8 *) os_malloc(length + status->ExtraCount);
    if (!buffer)
    {
        // Not enough RAM available
        return false;
    }

    // copy in any remaining bytes from last chunk
    if (status->ExtraCount != 0)
    {
        memcpy(buffer, status->ExtraBytes, status->ExtraCount);
    }

    // copy in new data
    memcpy(buffer + status->ExtraCount, data, length);

    // calculate length, must be multiple of 4
    // save any remaining bytes for next go
    length += status->ExtraCount;

    status->ExtraCount = length % 4;

    length -= status->ExtraCount;

    memcpy(status->ExtraBytes, buffer + length, status->ExtraCount);

    // erase any additional sectors needed by this chunk
    lastSector = ((status->StartAddress + length) - 1) / SECTOR_SIZE;

    while (lastSector > status->LastErasedSector)
    {
        status->LastErasedSector++;
        spi_flash_erase_sector(status->LastErasedSector);
    }

    // write current chunk
    if (spi_flash_write(status->StartAddress, (uint32 *) ((void*) buffer), length) == SPI_FLASH_RESULT_OK)
    {
        isOK = true;
        status->StartAddress += length;
    }

    os_free(buffer);

    return isOK;
}

//======================================================================================================================
// DESCRIPTION:         Get boot status/control data from RTC data area
//
// PARAMETERS:          RTCData* rtc - Pointer to a structure to be populated
//
// RETURN VALUE:        void
//
//======================================================================================================================
bool ICACHE_FLASH_ATTR GetRTCData(RTCData* rtc)
{
    if (system_rtc_mem_read(RTC_ADDRESS, rtc, sizeof(RTCData)))
    {
        return (rtc->CheckSum == GetCheckSum((uint8*) rtc, (uint8*) &rtc->CheckSum));
    }

    return false;
}

//======================================================================================================================
// DESCRIPTION:         Set boot status/control data in RTC data area
//
// PARAMETERS:          RTCData* rtc - pointer to a rboot_rtc_data structure
//
// RETURN VALUE:        bool True on success
//
//======================================================================================================================
bool ICACHE_FLASH_ATTR SetRTCData(RTCData* rtc)
{
    // calculate checksum
    rtc->CheckSum = GetCheckSum((uint8*) rtc, (uint8*) &rtc->CheckSum);

    return system_rtc_mem_write(RTC_ADDRESS, rtc, sizeof(RTCData));
}
