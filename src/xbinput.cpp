// Gamepad input, 0x26490..0x26A31 in the original. See include/evox/xbinput.h.
#include <math.h>
#include <string.h>
#include "evox/xbinput.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof" // (layout checks on purpose)
static_assert(offsetof(XBGAMEPAD, fX1) == 0x14, "fX1");
static_assert(offsetof(XBGAMEPAD, wLastButtons) == 0x24, "wLastButtons");
static_assert(offsetof(XBGAMEPAD, bLastAnalogButtons) == 0x28, "bLastAnalogButtons");
static_assert(offsetof(XBGAMEPAD, wPressedButtons) == 0x48, "wPressedButtons");
static_assert(offsetof(XBGAMEPAD, bPressedAnalogButtons) == 0x4C, "bPressedAnalogButtons");
static_assert(offsetof(XBGAMEPAD, Feedback) == 0x70, "Feedback");
static_assert(offsetof(XBGAMEPAD, caps) == 0xB6, "caps");
static_assert(offsetof(XBGAMEPAD, hDevice) == 0xD0, "hDevice");
static_assert(sizeof(XBGAMEPAD) == 0xDC, "XBGAMEPAD");

#define F32(bits) __builtin_bit_cast(float, (unsigned)(bits))

static XINPUT_STATE g_InputStates[4];                            // 0x591AB0
static XBGAMEPAD g_Gamepads[4];                                  // 0x591B08
static XINPUT_POLLING_PARAMETERS g_PollingParameters = {1, 1, 0, 8, 8, 0}; // 0x3D4140

// the analog buttons as booleans, as last seen in the given state
static void SetLastAnalog(XBGAMEPAD &g, const XINPUT_GAMEPAD &s)
{
    for (int b = 0; b < 8; b++)
        g.bLastAnalogButtons[b] = s.bAnalogButtons[b] > XINPUT_GAMEPAD_MAX_CROSSTALK;
}

HRESULT XBInput_CreateGamepads(XBGAMEPAD **ppGamepads)
{
    DWORD devices = XGetDevices(XDEVICE_TYPE_GAMEPAD);
    for (DWORD i = 0; i < 4; i++) {
        memset(&g_InputStates[i], 0, sizeof g_InputStates[i]);
        memset(&g_Gamepads[i], 0, sizeof g_Gamepads[i]);
        if (devices & (1u << i)) {
            // (as in the original: no check for a NULL handle here)
            g_Gamepads[i].hDevice = XInputOpen(XDEVICE_TYPE_GAMEPAD, i, XDEVICE_NO_SLOT, &g_PollingParameters);
            XInputGetCapabilities(g_Gamepads[i].hDevice, &g_Gamepads[i].caps);
            XInputGetState(g_Gamepads[i].hDevice, &g_InputStates[i]);
            g_Gamepads[i].wLastButtons = g_InputStates[i].Gamepad.wButtons;
            SetLastAnalog(g_Gamepads[i], g_InputStates[i].Gamepad);
        }
    }
    if (ppGamepads)
        *ppGamepads = g_Gamepads;
    return S_OK;
}

// Handle inserted and removed gamepads. The new state of an inserted pad is read through
// g_Gamepads (the original only ever passes g_Gamepads).
void XBInput_RefreshDeviceList(XBGAMEPAD *pGamepads)
{
    if (!pGamepads)
        pGamepads = g_Gamepads;
    DWORD insertions, removals;
    XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, &insertions, &removals);
    for (DWORD i = 0; i < 4; i++) {
        XBGAMEPAD &g = pGamepads[i];
        g.bRemoved = (removals & (1u << i)) != 0;
        if (g.bRemoved) {
            if (g.hDevice)
                XInputClose(g.hDevice);
            g.hDevice = nullptr;
            g.Feedback.Rumble.wLeftMotorSpeed = 0;
            g.Feedback.Rumble.wRightMotorSpeed = 0;
        }
        g.bInserted = (insertions & (1u << i)) != 0;
        if (g.bInserted) {
            g.hDevice = XInputOpen(XDEVICE_TYPE_GAMEPAD, i, XDEVICE_NO_SLOT, &g_PollingParameters);
            if (g.hDevice) {
                XInputGetCapabilities(g.hDevice, &g.caps);
                XInputGetState(g_Gamepads[i].hDevice, &g_InputStates[i]);
                g_Gamepads[i].wLastButtons = g_InputStates[i].Gamepad.wButtons;
                SetLastAnalog(g_Gamepads[i], g_InputStates[i].Gamepad);
            }
        }
    }
}

// [-1, 1] with a 0.24 dead zone
static float Thumb(SHORT v)
{
    float f = ((float)v + F32(0x3F000000)) * F32(0x38000080);          // (v + 0.5) / 32767.5
    float sign = f >= 0.0f ? F32(0x3F800000) : F32(0xBF800000);
    float m = (fabsf(f) - F32(0x3E75C28F)) * F32(0x3FA86BCA);           // (|f| - 0.24) / 0.76
    if (!(0.0f < m))
        m = 0.0f;
    return sign * m;
}

void XBInput_GetInput(XBGAMEPAD *pGamepads)
{
    if (!pGamepads)
        pGamepads = g_Gamepads;
    XBInput_RefreshDeviceList(pGamepads);
    for (DWORD i = 0; i < 4; i++) {
        XBGAMEPAD &g = pGamepads[i];
        if (!g.hDevice)
            continue;
        XInputGetState(g.hDevice, &g_InputStates[i]);
        memcpy((XINPUT_GAMEPAD *)&g, &g_InputStates[i].Gamepad, sizeof(XINPUT_GAMEPAD));
        g.fX1 = Thumb(g.sThumbLX);
        g.fY1 = Thumb(g.sThumbLY);
        g.fX2 = Thumb(g.sThumbRX);
        g.fY2 = Thumb(g.sThumbRY);
        g.wPressedButtons = (g.wLastButtons ^ g.wButtons) & g.wButtons;
        g.wLastButtons = g.wButtons;
        for (int b = 0; b < 8; b++) {
            BOOL pressed = g.bAnalogButtons[b] > XINPUT_GAMEPAD_MAX_CROSSTALK;
            g.bPressedAnalogButtons[b] = pressed ? !g.bLastAnalogButtons[b] : FALSE;
            g.bLastAnalogButtons[b] = pressed;
        }
    }
}
