#include "flash_files.h"
#include "logger.h"
#include <string.h>

#define FS_MAGIC 0xABCD1234
#define FS_VERSION 1

#define FS_MAX_FILES 32
#define FS_NAME_LEN 16

#define FS_HEADER_ADDR 0x0000
#define FS_TABLE_ADDR 0x1000 // 👉 tách hẳn sector cho an toàn
#define FS_DATA_START 0x2000

#define SECTOR_SIZE 4096

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t fileCount;
  uint32_t writePtr;
} FsHeader;

typedef struct
{
  char name[FS_NAME_LEN];
  uint32_t addr;
  uint32_t length;
  uint8_t valid;
} FsEntry;

/* ================= CRC ================= */
uint16_t fs_crc16(const uint8_t *data, uint32_t len)
{
  uint16_t crc = 0xFFFF;
  for (uint32_t i = 0; i < len; i++)
  {
    crc ^= data[i];
    for (int j = 0; j < 8; j++)
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
  }
  return crc;
}

/* ================= LOW LEVEL ================= */

bool fs_readHeader(SPIFlash *flash, FsHeader &h)
{
  if (!flash->readByteArray(FS_HEADER_ADDR, (uint8_t *)&h, sizeof(h), true))
    return false;

  if (h.magic != FS_MAGIC)
    return false;

  if (h.writePtr < FS_DATA_START || h.writePtr > flash->getCapacity())
    return false;

  return true;
}

bool fs_writeHeader(SPIFlash *flash, FsHeader &h)
{
  flash->eraseSector(FS_HEADER_ADDR);
  return flash->writeByteArray(FS_HEADER_ADDR, (uint8_t *)&h, sizeof(h), true);
}

bool fs_readTable(SPIFlash *flash, FsEntry *table)
{
  if (!flash->readByteArray(FS_TABLE_ADDR,
                            (uint8_t *)table,
                            sizeof(FsEntry) * FS_MAX_FILES,
                            true))
    return false;

  // sanitize
  for (int i = 0; i < FS_MAX_FILES; i++)
  {
    if (table[i].valid != 0 && table[i].valid != 1)
      table[i].valid = 0;
  }

  return true;
}

bool fs_writeTable(SPIFlash *flash, FsEntry *table)
{
  flash->eraseSector(FS_TABLE_ADDR);
  return flash->writeByteArray(FS_TABLE_ADDR,
                               (uint8_t *)table,
                               sizeof(FsEntry) * FS_MAX_FILES,
                               true);
}

/* ================= INIT ================= */

bool fsFormat(SPIFlash *flash)
{
  FsHeader h = {0};
  h.magic = FS_MAGIC;
  h.version = FS_VERSION;
  h.fileCount = 0;
  h.writePtr = FS_DATA_START;

  FsEntry table[FS_MAX_FILES] = {0};

  flash->eraseSector(FS_HEADER_ADDR);
  flash->eraseSector(FS_TABLE_ADDR);

  fs_writeHeader(flash, h);
  fs_writeTable(flash, table);

  return true;
}

bool fsInit(SPIFlash *flash)
{
  FsHeader h;
  if (!fs_readHeader(flash, h))
    return fsFormat(flash);

  return true;
}

/* ================= UTILS ================= */

void eraseRange(SPIFlash *flash, uint32_t addr, uint32_t len)
{
  uint32_t start = addr & ~(SECTOR_SIZE - 1);
  uint32_t end = (addr + len + SECTOR_SIZE - 1) & ~(SECTOR_SIZE - 1);

  for (uint32_t a = start; a < end; a += SECTOR_SIZE)
    flash->eraseSector(a);
}

/* ================= LIST ================= */

void fsList(SPIFlash *flash)
{
  FsEntry table[FS_MAX_FILES];
  fs_readTable(flash, table);

  logPrintln("Files:");

  for (int i = 0; i < FS_MAX_FILES; i++)
  {
    if (table[i].valid)
    {
      logPrintf("%d: %s len=%lu addr=0x%X\r\n",
                i,
                table[i].name,
                table[i].length,
                table[i].addr);
    }
  }
}

/* ================= DELETE ================= */

bool fsDelete(SPIFlash *flash, const char *name)
{
  FsEntry table[FS_MAX_FILES];
  fs_readTable(flash, table);

  FsEntry tmp[FS_MAX_FILES];
  memcpy(tmp, table, sizeof(tmp));

  for (int i = 0; i < FS_MAX_FILES; i++)
  {
    if (tmp[i].valid && strcmp(tmp[i].name, name) == 0)
      tmp[i].valid = 0;
  }

  return fs_writeTable(flash, tmp);
}

/* ================= GC ================= */

bool fsGC(SPIFlash *flash)
{
  FsHeader h;
  FsEntry table[FS_MAX_FILES];

  fs_readHeader(flash, h);
  fs_readTable(flash, table);

  // erase all data region
  for (uint32_t a = FS_DATA_START; a < flash->getCapacity(); a += SECTOR_SIZE)
    flash->eraseSector(a);

  uint32_t newAddr = FS_DATA_START;

  for (int i = 0; i < FS_MAX_FILES; i++)
  {
    if (table[i].valid)
    {
      newAddr = (newAddr + 0xFF) & ~0xFF;

      uint8_t buf[256];
      uint32_t remaining = table[i].length;
      uint32_t offset = 0;

      while (remaining)
      {
        uint32_t sz = remaining > 256 ? 256 : remaining;

        flash->readByteArray(table[i].addr + offset, buf, sz, true);
        flash->writeByteArray(newAddr + offset, buf, sz, true);

        offset += sz;
        remaining -= sz;
      }

      table[i].addr = newAddr;
      newAddr += table[i].length;
    }
  }

  h.writePtr = newAddr;

  fs_writeTable(flash, table);
  fs_writeHeader(flash, h);

  return true;
}

/* ================= WRITE ================= */

bool fsWrite(SPIFlash *flash,
             const char *name,
             const uint8_t *data,
             uint32_t len)
{
  FsHeader h;
  FsEntry table[FS_MAX_FILES];

  fs_readHeader(flash, h);
  fs_readTable(flash, table);

  // delete old
  for (int i = 0; i < FS_MAX_FILES; i++)
    if (table[i].valid && strcmp(table[i].name, name) == 0)
      table[i].valid = 0;

  uint32_t addr = (h.writePtr + 0xFF) & ~0xFF;

  if (addr + len > flash->getCapacity())
  {
    fsGC(flash);
    fs_readHeader(flash, h);
    addr = (h.writePtr + 0xFF) & ~0xFF;
  }

  eraseRange(flash, addr, len);

  if (!flash->writeByteArray(addr, (uint8_t *)data, len, true))
    return false;

  bool added = false;
  for (int i = 0; i < FS_MAX_FILES; i++)
  {
    if (!table[i].valid)
    {
      strcpy(table[i].name, name);
      table[i].addr = addr;
      table[i].length = len;
      table[i].valid = 1;
      added = true;
      break;
    }
  }

  if (!added)
    return false;

  h.writePtr = addr + len;

  fs_writeTable(flash, table);
  fs_writeHeader(flash, h);

  return true;
}

bool uartReadBytes(HardwareSerial &ser, uint8_t *buf, uint32_t len, uint32_t timeout = 2000)
{
  uint32_t start = millis();
  uint32_t got = 0;
  while (got < len)
  {
    if (ser.available())
    {
      buf[got++] = ser.read();
    }
    else if (millis() - start > timeout)
    {
      return false; // timeout
    }
  }
  return true;
}

/* ================= UART UPLOAD ================= */

bool flashFsWriteFileFromSerial_PRO(SPIFlash *flash,
                                    const char *name,
                                    uint32_t length)
{
  const uint8_t SOF = 0xAA;

  FsHeader h;
  fs_readHeader(flash, h);

  uint32_t addr = (h.writePtr + 0xFF) & ~0xFF;

  if (addr + length > flash->getCapacity())
  {
    logPrintln("NO SPACE");
    return false;
  }

  eraseRange(flash, addr, length);

  uint8_t seqExpected = 0;
  uint32_t received = 0;
  uint32_t startTime = millis();

  while (received < length)
  {
    if (millis() - startTime > 10000)
    {
      logPrintln("TIMEOUT");
      return false;
    }

    uint8_t b;
    do
    {
      if (!uartReadBytes(Serial1, &b, 1))
        return false;
    } while (b != SOF);

    uint8_t header[3];
    if (!uartReadBytes(Serial1, header, 3))
      return false;

    uint8_t seq = header[0];
    uint16_t len = header[1] | (header[2] << 8);

    uint8_t buf[256], crcBuf[2];

    if (!uartReadBytes(Serial1, buf, len))
      return false;
    if (!uartReadBytes(Serial1, crcBuf, 2))
      return false;

    uint16_t crcRx = crcBuf[0] | (crcBuf[1] << 8);

    if (crcRx != fs_crc16(buf, len) || seq != seqExpected)
    {
      Serial1.write(0x15);
      continue;
    }

    flash->writeByteArray(addr + received, buf, len, true);

    startTime = millis();
    Serial1.write(0x06);

    received += len;
    seqExpected = (seqExpected + 1) & 0xFF;
  }

  FsEntry table[FS_MAX_FILES];
  fs_readTable(flash, table);

  for (int i = 0; i < FS_MAX_FILES; i++)
    if (table[i].valid && strcmp(table[i].name, name) == 0)
      table[i].valid = 0;

  bool added = false;

  for (int i = 0; i < FS_MAX_FILES; i++)
  {
    if (!table[i].valid)
    {
      strcpy(table[i].name, name);
      table[i].addr = addr;
      table[i].length = length;
      table[i].valid = 1;
      added = true;
      break;
    }
  }

  if (!added)
  {
    logPrintln("TABLE FULL");
    return false;
  }

  h.writePtr = addr + length;

  fs_writeTable(flash, table);
  fs_writeHeader(flash, h);

  logPrintln("DONE");
  return true;
}

bool flashFsInit(SPIFlash *flash) { return fsInit(flash); }
bool flashFsFormat(SPIFlash *flash) { return fsFormat(flash); }
bool flashFsListFiles(SPIFlash *flash)
{
  fsList(flash);
  return true;
}
bool flashFsGetFileInfo(SPIFlash *flash, const char *name, uint32_t &addr, uint32_t &len)
{
  FsEntry table[FS_MAX_FILES];
  fs_readTable(flash, table);
  for (int i = 0; i < FS_MAX_FILES; i++)
  {
    if (table[i].valid && strcmp(table[i].name, name) == 0)
    {
      addr = table[i].addr;
      len = table[i].length;
      return true;
    }
  }
  return false;
}