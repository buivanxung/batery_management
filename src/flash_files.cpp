#include "flash_files.h"
#include "pinout.h"
#include <string.h>

static bool _eraseSectorRange(SPIFlash *flash, uint32_t start, uint32_t length)
{
  uint32_t end = start + length;
  uint32_t sectorStart = start & ~(FLASH_FS_SECTOR_SIZE - 1);

  while (sectorStart < end)
  {
    if (!flash->eraseSector(sectorStart))
      return false;
    sectorStart += FLASH_FS_SECTOR_SIZE;
  }
  return true;
}

static bool _readHeader(SPIFlash *flash, FlashFsHeader &header)
{
  if (!flash->readByteArray(FLASH_FS_TABLE_ADDR, (uint8_t *)&header, sizeof(header), true))
    return false;
  return (header.magic == FLASH_FS_MAGIC);
}

static bool _writeHeader(SPIFlash *flash, const FlashFsHeader &header)
{
  // Erase the sector(s) containing the header before writing.
  if (!_eraseSectorRange(flash, FLASH_FS_TABLE_ADDR, sizeof(header)))
    return false;
  return flash->writeByteArray(FLASH_FS_TABLE_ADDR, (uint8_t *)&header, sizeof(header), true);
}

bool flashFsInit(SPIFlash *flash)
{
  FlashFsHeader header;
  if (!_readHeader(flash, header))
  {
    // Format if header invalid.
    return flashFsFormat(flash);
  }
  return true;
}

bool flashFsFormat(SPIFlash *flash)
{
  FlashFsHeader header;
  memset(&header, 0xFF, sizeof(header));
  header.magic = FLASH_FS_MAGIC;
  header.fileCount = 0;
  return _writeHeader(flash, header);
}

bool flashFsListFiles(SPIFlash *flash)
{
  FlashFsHeader header;
  if (!_readHeader(flash, header))
    return false;

  Serial1.println(F("Flash files:"));
  for (uint8_t i = 0; i < header.fileCount; ++i)
  {
    Serial1.print(i);
    Serial1.print(F(": "));
    Serial1.print(header.files[i].name);
    Serial1.print(F(" @0x"));
    Serial1.print(header.files[i].addr, HEX);
    Serial1.print(F(" len="));
    Serial1.println(header.files[i].length);
  }
  return true;
}

static bool _findFileEntry(const FlashFsHeader &header, const char *name, uint8_t &index)
{
  for (uint8_t i = 0; i < header.fileCount; ++i)
  {
    if (strncmp(header.files[i].name, name, FLASH_FS_NAME_LEN) == 0)
    {
      index = i;
      return true;
    }
  }
  return false;
}

static bool _getFileInfo(SPIFlash *flash, const char *name, uint32_t &outAddr, uint32_t &outLength)
{
  FlashFsHeader header;
  if (!_readHeader(flash, header))
    return false;

  uint8_t idx;
  if (!_findFileEntry(header, name, idx))
    return false;

  outAddr = header.files[idx].addr;
  outLength = header.files[idx].length;
  return true;
}

bool flashFsWriteFile(SPIFlash *flash, const char *name, const uint8_t *data, uint32_t length)
{
  if (name == nullptr || strlen(name) == 0 || strlen(name) >= FLASH_FS_NAME_LEN)
    return false;

  FlashFsHeader header;
  if (!_readHeader(flash, header))
    return false;

  // Find existing entry or append.
  uint8_t entryIndex;
  bool exists = _findFileEntry(header, name, entryIndex);
  if (!exists)
  {
    if (header.fileCount >= FLASH_FS_MAX_FILES)
      return false; // no space in index
    entryIndex = header.fileCount;
  }

  // Determine where to place data.
  uint32_t nextAddr = FLASH_FS_DATA_START;
  for (uint8_t i = 0; i < header.fileCount; ++i)
  {
    const auto &f = header.files[i];
    uint32_t end = f.addr + f.length;
    if (end > nextAddr)
      nextAddr = end;
  }

  // Align to 256-byte boundary for nicer access (optional).
  nextAddr = (nextAddr + 0xFF) & ~0xFF;

  uint32_t capacity = flash->getCapacity();
  if (nextAddr + length > capacity)
    return false;

  // Erase all sectors we will write into.
  if (!_eraseSectorRange(flash, nextAddr, length))
    return false;

  if (!flash->writeByteArray(nextAddr, (uint8_t *)data, length, true))
    return false;

  // Update (or add) table entry.
  strncpy(header.files[entryIndex].name, name, FLASH_FS_NAME_LEN);
  header.files[entryIndex].name[FLASH_FS_NAME_LEN - 1] = '\0';
  header.files[entryIndex].addr = nextAddr;
  header.files[entryIndex].length = length;

  if (!exists)
    header.fileCount++;

  return _writeHeader(flash, header);
}

bool flashFsWriteFileFromSerial(SPIFlash *flash, const char *name, uint32_t length)
{
  if (name == nullptr || strlen(name) == 0 || strlen(name) >= FLASH_FS_NAME_LEN)
    return false;

  FlashFsHeader header;
  if (!_readHeader(flash, header))
    return false;

  uint8_t entryIndex;
  bool exists = _findFileEntry(header, name, entryIndex);
  if (!exists)
  {
    if (header.fileCount >= FLASH_FS_MAX_FILES)
      return false;
    entryIndex = header.fileCount;
  }

  // Determine where to place data.
  uint32_t nextAddr = FLASH_FS_DATA_START;
  for (uint8_t i = 0; i < header.fileCount; ++i)
  {
    const auto &f = header.files[i];
    uint32_t end = f.addr + f.length;
    if (end > nextAddr)
      nextAddr = end;
  }

  // Align to 256-byte boundary.
  nextAddr = (nextAddr + 0xFF) & ~0xFF;

  uint32_t capacity = flash->getCapacity();
  if (nextAddr + length > capacity)
    return false;

  if (!_eraseSectorRange(flash, nextAddr, length))
    return false;

  const uint32_t bufSize = 256;
  uint8_t buffer[bufSize];
  uint32_t remaining = length;
  uint32_t offset = 0;

  // Ensure we can wait long enough for all bytes to arrive.
  uint32_t originalTimeout = Serial1.getTimeout();
  Serial1.setTimeout(15000);

  while (remaining > 0)
  {
    uint32_t chunkSize = (remaining < bufSize) ? remaining : bufSize;
    size_t got = Serial1.readBytes(buffer, chunkSize);
    if (got != chunkSize)
    {
      Serial1.setTimeout(originalTimeout);  // Restore timeout before returning
      return false;
    }

    if (!flash->writeByteArray(nextAddr + offset, buffer, chunkSize, true))
    {
      Serial1.setTimeout(originalTimeout);  // Restore timeout before returning
      return false;
    }

    offset += chunkSize;
    remaining -= chunkSize;
  }

  // Restore original timeout
  Serial1.setTimeout(originalTimeout);

  strncpy(header.files[entryIndex].name, name, FLASH_FS_NAME_LEN);
  header.files[entryIndex].name[FLASH_FS_NAME_LEN - 1] = '\0';
  header.files[entryIndex].addr = nextAddr;
  header.files[entryIndex].length = length;

  if (!exists)
    header.fileCount++;

  return _writeHeader(flash, header);
}

bool flashFsReadFile(SPIFlash *flash, const char *name, uint8_t *buffer, uint32_t bufferSize, uint32_t *outLength)
{
  if (buffer == nullptr || outLength == nullptr)
    return false;

  FlashFsHeader header;
  if (!_readHeader(flash, header))
    return false;

  uint8_t idx;
  if (!_findFileEntry(header, name, idx))
    return false;

  const auto &entry = header.files[idx];
  if (entry.length > bufferSize)
    return false;

  if (!flash->readByteArray(entry.addr, buffer, entry.length, true))
    return false;

  *outLength = entry.length;
  return true;
}

bool flashFsPlayAudio(SPIFlash *flash, const char *name)
{
  uint8_t buffer[256];
  uint32_t addr;
  uint32_t length;

  if (!_getFileInfo(flash, name, addr, length))
    return false;

  pinMode(AUDIO_PWM_PIN, OUTPUT);

  const uint32_t periodUs = 1000000UL / AUDIO_SAMPLE_RATE;

  uint32_t remaining = length;
  uint32_t offset = 0;

  while (remaining > 0)
  {
    uint32_t chunkSize = (remaining < sizeof(buffer)) ? remaining : sizeof(buffer);
    if (!flash->readByteArray(addr + offset, buffer, chunkSize, true))
      return false;

    for (uint32_t i = 0; i < chunkSize; ++i)
    {
      analogWrite(AUDIO_PWM_PIN, buffer[i]);
      delayMicroseconds(periodUs);
    }

    offset += chunkSize;
    remaining -= chunkSize;
  }

  // Stop PWM output.
  analogWrite(AUDIO_PWM_PIN, 0);
  return true;
}

bool flashFsGetFileInfo(SPIFlash *flash, const char *name, uint32_t &outAddr, uint32_t &outLength)
{
  return _getFileInfo(flash, name, outAddr, outLength);
}
