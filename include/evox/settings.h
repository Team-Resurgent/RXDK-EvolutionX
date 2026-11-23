// The Settings / Info screen (0x1A150 OpenSettings, 0x1B440 SaveSettings, 0x1B820 DrawSettings in
// the original): a table of rows (headers, read-only information, evox.ini values) with an
// on-screen keyboard for editing.
#pragma once
#include <xtl.h>

enum SettingType : DWORD {
    SETTING_YESNO = 0x00,    // "Yes" / "No": the Yes/No keyboard
    SETTING_ADDRESS = 0x01,  // IP addresses: the text keyboard
    SETTING_TEXT = 0x02,     // the text keyboard
    SETTING_CHOICE = 0x03,   // value is an index into the live list, A steps through it
    SETTING_SAVE = 0x10,     // Save And Exit
    SETTING_EXIT = 0x11,     // Exit
    SETTING_SAVE_TRAINER = 0x12, // the trainer options screen: save and exit
    SETTING_INFO = 0x80,     // read-only: live shows the text
    SETTING_HEADER = 0xC0,
};

struct SettingRow {        // 16 bytes
    const WCHAR *label;    // +0x0
    char *value;           // +0x4 the evox.ini value (a 256-byte buffer); for SETTING_CHOICE the index
    const void *live;      // +0x8 SETTING_INFO: the text (WCHAR *); SETTING_CHOICE: WCHAR *[] list
    DWORD type;            // +0xC SettingType
};

extern SettingRow g_Settings[200]; // 0x3F88B8
extern int g_SettingsCount;        // 0x3F9738

// a key of the on-screen keyboard
struct KeyboardKey {       // 0x1C bytes
    const WCHAR *label;    // +0x00
    DWORD enabled;         // +0x04
    DWORD code;            // +0x08 0: type the label; 0x20 delete, 0x21 done, 0x22 yes, 0x23 no
    float x, y, w, h;      // +0x0C relative to the keyboard
};

struct KeyboardLayout {    // 0x4B0 bytes
    float x, y, w, h;      // +0x00
    const WCHAR *unk10;    // +0x10
    WCHAR *edit;           // +0x14 text being edited, NULL for Yes / No
    KeyboardKey keys[3][14]; // +0x18
};
