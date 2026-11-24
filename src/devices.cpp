// The DVD remote and the USB keyboard (0x23D70..0x240E4 in the original); see include/evox/devices.h.
#include "evox/devices.h"
#include <string.h>

static_assert(sizeof(XINPUT_STATE) == 0x16, "XINPUT_STATE");
static_assert(sizeof(RemotePort) == 0x28, "RemotePort");
static_assert(__builtin_offsetof(RemotePort, handle) == 0x1C, "RemotePort.handle");

static XINPUT_STATE g_RemoteState[4];         // 0x590E78
static RemotePort g_Remotes[4];               // 0x590ED0
static int g_RemoteRepeat;                    // 0x590F70 frames the last button still counts
static DWORD g_RemoteLastPacket = 0xFFFFFFFF; // 0xCE9F0 (one for all four ports)
static XINPUT_DEBUG_KEYSTROKE g_Keystroke;    // 0x590F74
static HANDLE g_Keyboards[4];                 // 0x590F78
static BOOL g_KeyboardQueueReady;             // 0x590F88

// auto-poll, interrupt-out, 32 ms both ways (the remote's and the keyboard's polling)
static XINPUT_POLLING_PARAMETERS Polling()
{
    XINPUT_POLLING_PARAMETERS p;
    p.fAutoPoll = 1;
    p.fInterruptOut = 1;
    p.ReservedMBZ1 = 0;
    p.bInputInterval = 0x20;
    p.bOutputInterval = 0x20;
    p.ReservedMBZ2 = 0;
    return p;
}

// 0x23D70: each remote present: opened (without polling parameters) and its capabilities read
HRESULT OpenRemotes()
{
    DWORD present = XGetDevices(XDEVICE_TYPE_IR_REMOTE);
    for (DWORD port = 0; port < 4; port++) {
        memset(&g_RemoteState[port], 0, sizeof g_RemoteState[port]);
        memset(&g_Remotes[port], 0, sizeof g_Remotes[port]);
        if (present & (1 << port)) {
            g_Remotes[port].handle = XInputOpen(XDEVICE_TYPE_IR_REMOTE, port, XDEVICE_NO_SLOT, nullptr);
            XInputGetCapabilities(g_Remotes[port].handle, &g_Remotes[port].caps);
        }
    }
    return S_OK;
}

// 0x23E10: the remote's button this frame: the inverted low byte of its state, for a new packet,
// or again for 5 frames while the packet stays the same. Only the codes 0x1C, 0x1D, 0x20 and
// 0x22 keep repeating; anything else (also "nothing", 0) stops the repeat.
int PollRemote()
{
    DWORD insertions, removals;
    int code = 0;
    XGetDeviceChanges(XDEVICE_TYPE_IR_REMOTE, &insertions, &removals);
    for (DWORD port = 0; port < 4; port++) {
        RemotePort &r = g_Remotes[port];
        DWORD bit = 1 << port;
        r.removed = (removals & bit) != 0;
        if (r.removed) {
            if (r.handle)
                XInputClose(r.handle);
            r.handle = nullptr;
        }
        r.inserted = (insertions & bit) != 0;
        if (r.inserted) {
            XINPUT_POLLING_PARAMETERS polling = Polling();
            r.handle = XInputOpen(XDEVICE_TYPE_IR_REMOTE, port, XDEVICE_NO_SLOT, &polling);
        }
        if (!r.handle)
            continue;
        XInputGetState(r.handle, &g_RemoteState[port]);
        DWORD packet = g_RemoteState[port].dwPacketNumber;
        if (g_RemoteLastPacket != packet) {
            g_RemoteLastPacket = packet;
            g_RemoteRepeat = 5;
        } else if (g_RemoteRepeat == 0) {
            continue;
        }
        code = (BYTE)~((const BYTE *)&g_RemoteState[port])[4];
    }
    if (g_RemoteRepeat)
        g_RemoteRepeat--;
    switch (code) {
    case 0x1C:
    case 0x1D:
    case 0x20:
    case 0x22:
        break;
    default:
        g_RemoteRepeat = 0;
        break;
    }
    return code;
}

// 0x23F70 (once): the keystroke queue (key downs and repeats, 25 deep, repeat after 500 ms
// every 50 ms) and each keyboard present
HRESULT OpenKeyboards()
{
    if (g_KeyboardQueueReady)
        return S_OK;
    XINPUT_DEBUG_KEYQUEUE_PARAMETERS queue = {
        XINPUT_DEBUG_KEYQUEUE_FLAG_KEYDOWN | XINPUT_DEBUG_KEYQUEUE_FLAG_KEYREPEAT, 25, 500, 50};
    if (XInputDebugInitKeyboardQueue(&queue) != ERROR_SUCCESS)
        return E_FAIL;
    // 0x23FB7
    g_KeyboardQueueReady = TRUE;
    DWORD present = XGetDevices(XDEVICE_TYPE_DEBUG_KEYBOARD);
    for (DWORD port = 0; port < 4; port++) {
        if (present & (1 << port)) {
            XINPUT_POLLING_PARAMETERS polling = Polling();
            g_Keyboards[port] = XInputOpen(XDEVICE_TYPE_DEBUG_KEYBOARD, port, XDEVICE_NO_SLOT, &polling);
        }
    }
    return S_OK;
}

// 0x24030: keyboards in and out, then the first keystroke queued, or NULL. (A removed keyboard
// is closed without checking its handle, as in the original.)
const XINPUT_DEBUG_KEYSTROKE *GetKeystroke()
{
    DWORD insertions, removals;
    XGetDeviceChanges(XDEVICE_TYPE_DEBUG_KEYBOARD, &insertions, &removals);
    for (DWORD port = 0; port < 4; port++) {
        DWORD bit = 1 << port;
        if (removals & bit) {
            XInputClose(g_Keyboards[port]);
            g_Keyboards[port] = nullptr;
        }
        if (insertions & bit) {
            XINPUT_POLLING_PARAMETERS polling = Polling();
            g_Keyboards[port] = XInputOpen(XDEVICE_TYPE_DEBUG_KEYBOARD, port, XDEVICE_NO_SLOT, &polling);
        }
        if (g_Keyboards[port] && XInputDebugGetKeystroke(&g_Keystroke) == ERROR_SUCCESS)
            return &g_Keystroke;
    }
    return nullptr;
}
