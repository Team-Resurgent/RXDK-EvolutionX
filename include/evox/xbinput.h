// Gamepad input (0x26490..0x26A31 in the original): the XDK sample framework's xbinput.cpp as
// EvoX linked it. XBGAMEPAD extends XINPUT_GAMEPAD with converted thumbsticks and the buttons
// pressed since the last poll.
#pragma once
#include <xtl.h>

struct XBGAMEPAD : public XINPUT_GAMEPAD { // 0xDC bytes
    FLOAT fX1, fY1, fX2, fY2;      // +0x14 thumbsticks in [-1, 1] after the dead zone
    WORD wLastButtons;             // +0x24
    BOOL bLastAnalogButtons[8];    // +0x28
    WORD wPressedButtons;          // +0x48 went down since the last poll
    BOOL bPressedAnalogButtons[8]; // +0x4C
    XINPUT_RUMBLE Rumble;          // +0x6C
    XINPUT_FEEDBACK Feedback;      // +0x70
    XINPUT_CAPABILITIES caps;      // +0xB6
    HANDLE hDevice;                // +0xD0
    BOOL bInserted;                // +0xD4
    BOOL bRemoved;                 // +0xD8
};

HRESULT XBInput_CreateGamepads(XBGAMEPAD **ppGamepads);   // 0x26490
void XBInput_RefreshDeviceList(XBGAMEPAD *pGamepads);     // 0x265F0
void XBInput_GetInput(XBGAMEPAD *pGamepads);              // 0x267A0
