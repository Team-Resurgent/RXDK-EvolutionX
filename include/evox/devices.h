// The DVD remote and the USB keyboard (0x23D70..0x240E4 in the original). Create opens them;
// Run asks each frame for the remote's button (m_keyboard, the menus' page keys) and for a
// keystroke (m_keystroke / m_key / m_keyFlags).
#pragma once
#include <xtl.h>
#include <xkbd.h>

// one DVD remote port (0x590ED0, 0x28 bytes each)
struct RemotePort {
    XINPUT_CAPABILITIES caps;  // +0x00 (0x19 bytes, packed)
    BYTE pad19[3];
    HANDLE handle;             // +0x1C
    BOOL inserted;             // +0x20 this frame
    BOOL removed;              // +0x24 this frame
};

HRESULT OpenRemotes();                          // 0x23D70 (Create)
int PollRemote();                               // 0x23E10 each frame: a button code, or 0
HRESULT OpenKeyboards();                        // 0x23F70 (Create)
const XINPUT_DEBUG_KEYSTROKE *GetKeystroke();   // 0x24030 each frame: a keystroke, or NULL
