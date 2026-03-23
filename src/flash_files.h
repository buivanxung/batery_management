#pragma once

#include <Arduino.h>
#include "SPIMemory.h"

/// Simple flash file system layout (for small embedded audio clips / data blobs).
/// - Table is stored starting at address 0x0000.
/// - Audio/data content stored from FLASH_FS_DATA_START onward.
/// - Filename max length is 15 (null-terminated).

#define FLASH_FS_MAGIC        0xA5A5A5A5UL
#define FLASH_FS_TABLE_ADDR   0x0000UL
#define FLASH_FS_DATA_START   0x1000UL  // start at 4KiB (one sector)
#define FLASH_FS_SECTOR_SIZE  0x1000UL  // 4KiB
#define FLASH_FS_MAX_FILES    16
#define FLASH_FS_NAME_LEN     16

struct FlashFileEntry
{
  char     name[FLASH_FS_NAME_LEN];
  uint32_t addr;
  uint32_t length;
};

struct FlashFsHeader
{
  uint32_t        magic;
  uint8_t         fileCount;
  uint8_t         reserved[3];
  FlashFileEntry  files[FLASH_FS_MAX_FILES];
};

/// Initialize the flash filesystem. If the header is invalid, it formats the FS.
bool flashFsInit(SPIFlash *flash);

/// Format (erase) the filesystem header and clear the file index.
bool flashFsFormat(SPIFlash *flash);

/// List files (prints to Serial1). Returns true if filesystem is valid.
bool flashFsListFiles(SPIFlash *flash);

/// Save a file into flash (overwrites if name exists).
/// Returns true on success.
bool flashFsWriteFile(SPIFlash *flash, const char *name, const uint8_t *data, uint32_t length);

/// Receive a stream of bytes from Serial1 and save it into flash.
/// Command syntax example: "STORE name length" then send `length` raw bytes.
bool flashFsWriteFileFromSerial(SPIFlash *flash, const char *name, uint32_t length);

/// Read a file from flash into buffer. outLength is set to the file length.
bool flashFsReadFile(SPIFlash *flash, const char *name, uint8_t *buffer, uint32_t bufferSize, uint32_t *outLength);

/// Play an audio file (8-bit PCM) through a PWM pin (AUDIO_PWM_PIN).
/// This is a blocking call and will return after playback completes.
bool flashFsPlayAudio(SPIFlash *flash, const char *name);

/// Get file information from filesystem (for DAC playback)
/// Used internally by audio_dac module
bool flashFsGetFileInfo(SPIFlash *flash, const char *name, uint32_t &outAddr, uint32_t &outLength);
