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

u16 EraseFlashSector_MX(u16 sectorNum)
{
    u16 numTries;
    u16 result;
    u8 *addr;
    u16 readFlash1Buffer[0x20];

    if (sectorNum >= gFlash->sector.count)
        return 0x80FF;

    SwitchFlashBank(sectorNum / SECTORS_PER_BANK);
    sectorNum %= SECTORS_PER_BANK;

    numTries = 0;

try_erase:
    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | gFlash->wait[0];

    addr = FLASH_BASE + (sectorNum << gFlash->sector.shift);

    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0x80);
    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    *addr = 0x30;

    SetReadFlash1(readFlash1Buffer);

    result = WaitForFlashWrite(2, addr, 0xFF);

    if (!(result & 0xA000) || numTries > 3)
        goto done;

    numTries++;

    goto try_erase;

done:
    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

    return result;
}

u16 ProgramFlashByte_MX(u16 sectorNum, u32 offset, u8 data)
{
    u8 *addr;
    u16 readFlash1Buffer[0x20];

    if (offset >= gFlash->sector.size)
        return 0x8000;

    SwitchFlashBank(sectorNum / SECTORS_PER_BANK);
    sectorNum %= SECTORS_PER_BANK;

    addr = FLASH_BASE + (sectorNum << gFlash->sector.shift) + offset;

    SetReadFlash1(readFlash1Buffer);

    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | gFlash->wait[0];

    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0xA0);
    *addr = data;

    return WaitForFlashWrite(1, addr, data);
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

#define FLASH_SAVE_OFFSET 0x0f00000
#define AGB_ROM 0x8000000
#define SRAM_SIZE 0x1000
#define _FLASH_WRITE(pa, pd) { *(((u16 *)AGB_ROM)+((pa)/2)) = pd; __asm("nop"); }

u16 _CopySramToFlash(u16 slotNum) {
    u32 dst = FLASH_SAVE_OFFSET + slotNum * FLASH_SECTOR_SIZE;
	u16 ie = REG_IE;
    int i;
	REG_IE = ie & 0xFFFE;
    // Erase flash sector
    _FLASH_WRITE(dst, 0xF0);
    _FLASH_WRITE(0xAAA, 0xA9);
    _FLASH_WRITE(0x555, 0x56);
    _FLASH_WRITE(0xAAA, 0x80);
    _FLASH_WRITE(0xAAA, 0xA9);
    _FLASH_WRITE(0x555, 0x56);
    _FLASH_WRITE(dst, 0x30);
    *(vu16 *)BG_PLTT = RGB_RED; // Set the backdrop to white on startup
    while (1) {
        __asm("nop");
        if (*(((vu16 *)AGB_ROM)+(dst/2)) == 0xFFFF) {
            break;
        }
    }
    _FLASH_WRITE(dst, 0xF0);
    *(vu16 *)BG_PLTT = RGB_GREEN; // Set the backdrop to white on startup
    // Write data
    for (i=0; i<SRAM_SIZE / 2; i+=2) {
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
    *(vu16 *)BG_PLTT = RGB_CYAN; // Set the backdrop to white on startup
    _FLASH_WRITE(dst, 0xF0);
	REG_IE = ie;
    return 0;
}

u16 CopySramToFlash(u16 slotNum) {
    u32 i;
    vu16 inner_func_buffer[0x100];
    vu16 *funcSrc;
    vu16 *funcDst;
    u16 (*innerFunc)(u16);
    *(vu16 *)BG_PLTT = RGB_YELLOW; // Set the backdrop to white on startup
    funcSrc = (vu16 *)_CopySramToFlash;
    funcSrc = (vu16 *)((s32)funcSrc ^ 1);
    funcDst = inner_func_buffer;

    i = ((s32)CopySramToFlash - (s32)_CopySramToFlash) >> 1;
    while (i != 0)
    {
        *funcDst++ = *funcSrc++;
        i--;
    }
    innerFunc = (u16 (*)(u16))((s32)inner_func_buffer + 1);
    *(vu16 *)BG_PLTT = RGB_BLUE; // Set the backdrop to white on startup
    return innerFunc(slotNum);
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
    u16 readFlash1Buffer[0x20];

    if (sectorNum < SECTOR_ID_HOF_1) {
        // Just write to SRAM, caller copies back to flash
        CopySectorToSram(sectorNum, src);
        return 0;
    }
    // Special sector, write directly to flash
    // TODO

    if (sectorNum >= SECTORS_COUNT)
        return 0x80FF;

    result = EraseFlashSector_MX(sectorNum);

    if (result != 0)
        return result;

    SetReadFlash1(readFlash1Buffer);

    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | gFlash->wait[0];

    gFlashNumRemainingBytes = gFlash->sector.size;
    dest = FLASH_BASE + (sectorNum << gFlash->sector.shift);

    while (gFlashNumRemainingBytes > 0)
    {
        result = ProgramByte(src, dest);

        if (result != 0)
            break;

        gFlashNumRemainingBytes--;
        src++;
        dest++;
    }

    return result;
}
