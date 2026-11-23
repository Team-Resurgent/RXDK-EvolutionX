// The Chameleon's ports (0x1F300..0x1F44A in the original); see include/evox/chameleon.h.
#include "evox/chameleon.h"

static BYTE g_ChameleonOut = 0x04; // 0x95BB8 what was last written to port 0xFF

// 0x1F300
BYTE ChameleonRead(int reg)
{
    BYTE v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"((unsigned short)(0xFE + reg)));
    return v;
}

// 0x1F320
static void ChameleonWrite(BYTE v)
{
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"((unsigned short)0xFF));
}

// 0x1F330: 1 (4 x 256 KB banks), 3 (1 x 1 MB), 2 (2 x 512 KB) or 4 (2 x 256 KB + 512 KB)
int ChameleonMode()
{
    static const int modes[4] = {1, 3, 2, 4};
    return modes[(ChameleonRead(1) >> 2) & 3];
}

// 0x1F380: bit 2 (the low two bits are cleared either way)
void ChameleonSetFlash(int on)
{
    if (on == 1)
        g_ChameleonOut = (g_ChameleonOut | 4) & 0xFC;
    else
        g_ChameleonOut = g_ChameleonOut & 0xFB & 0xFC;
    ChameleonWrite(g_ChameleonOut);
}

// 0x1F3C0: bit 4
void ChameleonSetLed(int on)
{
    if (on == 1)
        g_ChameleonOut |= 0x10;
    else
        g_ChameleonOut &= 0xEF;
    ChameleonWrite(g_ChameleonOut);
}

// 0x1F400: the colour in the low two bits with 0x40; mode 2 has only colours 0 and 2, mode 3
// none
void ChameleonSetColour(int colour)
{
    g_ChameleonOut &= 0xFC;
    BYTE c = colour & 3;
    switch (ChameleonMode()) {
    case 2:
        if (c)
            c = 2;
        [[fallthrough]];
    case 1:
    case 4:
        g_ChameleonOut |= c | 0x40;
        break;
    }
    ChameleonWrite(g_ChameleonOut);
}
