// The Chameleon (a modchip with a coloured LED and switchable flash banks) on I/O ports
// 0xFE..0xFF (0x1F300..0x1F44A in the original). Port 0xFE reads 0xAA when one is fitted.
#pragma once
#include <xtl.h>

BYTE ChameleonRead(int reg);          // 0x1F300 port 0xFE + reg (0: 0xAA if fitted, 1: switches)
int ChameleonMode();                  // 0x1F330 1..4 from the mode switches
void ChameleonSetFlash(int on);       // 0x1F380
void ChameleonSetLed(int on);         // 0x1F3C0
void ChameleonSetColour(int colour);  // 0x1F400 0..3, where the mode allows it
