#include "gba/gba.h"
#include "gba/flash_internal.h"
#include "save.h"
#include "constants/rgb.h"

const u16 mxMaxTime[] =
{
      10, 65469, TIMER_ENABLE | TIMER_INTR_ENABLE | TIMER_256CLK,
      10, 65469, TIMER_ENABLE | TIMER_INTR_ENABLE | TIMER_256CLK,
    2000, 65469, TIMER_ENABLE | TIMER_INTR_ENABLE | TIMER_256CLK,
    2000, 65469, TIMER_ENABLE | TIMER_INTR_ENABLE | TIMER_256CLK,
};

const struct FlashSetupInfo MX29L010 =
{
    ProgramFlashByte_MX,
    ProgramFlashSector_MX,
    EraseFlashChip_MX,
    EraseFlashSector_MX,
    WaitForFlashWrite_Common,
    mxMaxTime,
    {
        131072, // ROM size
        {
            4096, // sector size
              12, // bit shift to multiply by sector size (4096 == 1 << 12)
              32, // number of sectors
               0  // appears to be unused
        },
        { 3, 1 }, // wait state setup data
        { { 0xC2, 0x09 } } // ID
    }
};

const struct FlashSetupInfo DefaultFlash =
{
    ProgramFlashByte_MX,
    ProgramFlashSector_MX,
    EraseFlashChip_MX,
    EraseFlashSector_MX,
    WaitForFlashWrite_Common,
    mxMaxTime,
    {
        131072, // ROM size
        {
            4096, // sector size
              12, // bit shift to multiply by sector size (4096 == 1 << 12)
              32, // number of sectors
               0  // appears to be unused
        },
        { 3, 1 }, // wait state setup data
        { { 0x00, 0x00 } } // ID of 0
    }
};

u16 EraseFlashChip_MX(void)
{
    u16 result;
    u16 readFlash1Buffer[0x20];

    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | gFlash->wait[0];

    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0x80);
    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0x10);

    SetReadFlash1(readFlash1Buffer);

    result = WaitForFlashWrite(3, FLASH_BASE, 0xFF);

    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

    return result;
}

#define FLASH_SAVE_OFFSET 0x0f00000
#define AGB_ROM 0x8000000
#define SRAM_SIZE 0x10000
#define _FLASH_WRITE(pa, pd) { *(((u16 *)AGB_ROM)+((pa)/2)) = pd; __asm("nop"); }

u16 _EraseFlashSector_MX(u16 sectorNum)
{
    u32 dst = FLASH_SAVE_OFFSET + sectorNum * FLASH_SECTOR_SIZE;
    int i;
    // Erase flash sector
    _FLASH_WRITE(dst, 0xF0);
    _FLASH_WRITE(0xAAA, 0xA9);
    _FLASH_WRITE(0x555, 0x56);
    _FLASH_WRITE(0xAAA, 0x80);
    _FLASH_WRITE(0xAAA, 0xA9);
    _FLASH_WRITE(0x555, 0x56);
    _FLASH_WRITE(dst, 0x30);
    while (1) {
        __asm("nop");
        if (*(((vu16 *)AGB_ROM)+(dst/2)) == 0xFFFF) {
            break;
        }
    }
    _FLASH_WRITE(dst, 0xF0);

    return 0;
}

u16 EraseFlashSector_MX(u16 sectorNum)
{
    u32 i;
    vu16 inner_func_buffer[0x80];
    vu16 *funcSrc;
    vu16 *funcDst;
    u16 (*innerFunc)(u16);
    u16 result;

    u16 ie = REG_IE;
    u16 ime = REG_IME;
    REG_IE = 0;
    REG_IME = 0;

    funcSrc = (vu16 *)_EraseFlashSector_MX;
    funcSrc = (vu16 *)((s32)funcSrc ^ 1);
    funcDst = inner_func_buffer;

    i = ((s32)EraseFlashSector_MX - (s32)_EraseFlashSector_MX) >> 1;
    while (i != 0)
    {
        *funcDst++ = *funcSrc++;
        i--;
    }
    innerFunc = (u16 (*)(u16))((s32)inner_func_buffer + 1);
    result =  innerFunc(sectorNum);
    REG_IE = ie;
    REG_IME = ime;
    return result;
}

u16 _ProgramFlashByte_MX(u32 offset, u8 data)
{
    u32 dst = FLASH_SAVE_OFFSET + offset;

}

u16 ProgramFlashByte_MX(u16 sectorNum, u32 offset, u8 data)
{
    u32 i;
    vu16 inner_func_buffer[0x80];
    vu16 *funcSrc;
    vu16 *funcDst;
    u16 (*innerFunc)(u16, u32, u8);
    u16 result;
    u16 ie = REG_IE;
    u16 ime = REG_IME;
    REG_IE = 0;
    REG_IME = 0;

    // XXX noah
    // Figure out the actual offset in save space, or just write to SRAM

    funcSrc = (vu16 *)_ProgramFlashByte_MX;
    funcSrc = (vu16 *)((s32)funcSrc ^ 1);
    funcDst = inner_func_buffer;

    i = ((s32)ProgramFlashByte_MX - (s32)_ProgramFlashByte_MX) >> 1;
    while (i != 0)
    {
        *funcDst++ = *funcSrc++;
        i--;
    }
    innerFunc = (u16 (*)(u16, u32, u8))((s32)inner_func_buffer + 1);
    result =  innerFunc(sectorNum, offset, data);
    REG_IE = ie;
    REG_IME = ime;
    return result;
}

static u16 ProgramByte(u8 *src, u8 *dest)
{
    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0xA0);
    *dest = *src;

    return WaitForFlashWrite(1, dest, *src);
}

void CopyFlashToSram(u16 slotNum) {
    u16 i;
    u8 *src = (u8 *) FLASH_SAVE_START + slotNum * FLASH_SECTOR_SIZE;
    u8 *dst = SRAM_BASE;
    for (i = 0; i < SECTOR_SIZE * NUM_SECTORS_PER_SLOT; i++) {
        *dst = *src;
        dst++;
        src++;
    }
}

u16 _CopySramToFlash(u16 slotNum) {
    u32 dst = FLASH_SAVE_OFFSET + slotNum * FLASH_SECTOR_SIZE;
    int i;
    // Erase flash sector
    _FLASH_WRITE(dst, 0xF0);
    _FLASH_WRITE(0xAAA, 0xA9);
    _FLASH_WRITE(0x555, 0x56);
    _FLASH_WRITE(0xAAA, 0x80);
    _FLASH_WRITE(0xAAA, 0xA9);
    _FLASH_WRITE(0x555, 0x56);
    _FLASH_WRITE(dst, 0x30);
    while (1) {
        __asm("nop");
        if (*(((vu16 *)AGB_ROM)+(dst/2)) == 0xFFFF) {
            break;
        }
    }
    _FLASH_WRITE(dst, 0xF0);
    // Write data
    for (i=0; i<SRAM_SIZE; i+=2) {
        u16 half = (*(u8 *)(SRAM_BASE+i+1)) << 8 | (*(u8 *)(SRAM_BASE+i));
        _FLASH_WRITE(0xAAA, 0xA9);
        _FLASH_WRITE(0x555, 0x56);
        _FLASH_WRITE(0xAAA, 0xA0);
        _FLASH_WRITE(dst+i, half);
        while (1) {
            __asm("nop");
            if (*(((vu16 *)AGB_ROM)+((dst+i)/2)) == half) {
                break;
            }
        }
    }
    _FLASH_WRITE(dst, 0xF0);
    return 0;
}

u16 CopySramToFlash(u16 slotNum) {
    u16 result;
    result = EraseFlashSector_MX(slotNum);

    if (result != 0)
        return result;

    result = CopyToFlash(slotNum, (u8 *)SRAM_BASE, SRAM_SIZE);

    return result;
}

u16 _CopyToFlash(u16 sectorNum, u8 *src, u32 size) {
    u32 dst = FLASH_SAVE_OFFSET + sectorNum * FLASH_SECTOR_SIZE;
    int i;
    for (i=0; i<size; i+=2) {
        u16 half = (*(src+i+1)) << 8 | (*(src+i));
        _FLASH_WRITE(0xAAA, 0xA9);
        _FLASH_WRITE(0x555, 0x56);
        _FLASH_WRITE(0xAAA, 0xA0);
        _FLASH_WRITE(dst+i, half);
        while (1) {
            __asm("nop");
            if (*(((vu16 *)AGB_ROM)+((dst+i)/2)) == half) {
                break;
            }
        }
    }
    _FLASH_WRITE(dst, 0xF0);
    return 0;
}

u16 CopyToFlash(u16 sectorNum, u8 *src, u32 size) {
    u32 i;
    vu16 inner_func_buffer[0x80];
    vu16 *funcSrc;
    vu16 *funcDst;
    u16 (*innerFunc)(u16, u8 *, u32);
    u16 result;
    u16 ie = REG_IE;
    u16 ime = REG_IME;
    REG_IE = 0;
    REG_IME = 0;

    funcSrc = (vu16 *)_CopyToFlash;
    funcSrc = (vu16 *)((s32)funcSrc ^ 1);
    funcDst = inner_func_buffer;

    i = ((s32)CopyToFlash - (s32)_CopyToFlash) >> 1;
    while (i != 0)
    {
        *funcDst++ = *funcSrc++;
        i--;
    }
    innerFunc = (u16 (*)(u16, u8 *, u32))((s32)inner_func_buffer + 1);
    result =  innerFunc(sectorNum, src, size);
    REG_IE = ie;
    REG_IME = ime;
    return result;
}

void CopySectorToSram(u16 sectorNum, u8 *src) {
    u8 *dst;
    u16 i;
    sectorNum %= NUM_SECTORS_PER_SLOT;
    dst = SRAM_BASE + sectorNum * SECTOR_SIZE;
    for (i = 0; i < SECTOR_SIZE; i++) {
        *dst = *src;
        dst++;
        src++;
    }
}

u16 ProgramFlashSector_MX(u16 sectorNum, u8 *src)
{
    u16 result;
    u8 *dest;
    u16 remaining_bytes;

    if (sectorNum < SECTOR_ID_HOF_1) {
        // Just write to SRAM, caller copies back to flash
        CopySectorToSram(sectorNum, src);
        return 0;
    }

    if (sectorNum >= SECTORS_COUNT)
        return 0x80FF;

    // Special sector, write directly to flash

    sectorNum -= SECTOR_ID_HOF_1;
    sectorNum += NUM_SAVE_SLOTS;

    result = EraseFlashSector_MX(sectorNum);

    if (result != 0)
        return result;

    result = CopyToFlash(sectorNum, src, SECTOR_SIZE);

    return result;
}
