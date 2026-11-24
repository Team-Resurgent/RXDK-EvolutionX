// The Settings / Info screen: OpenSettings (0x1A150), SaveSettings (0x1B440) and DrawSettings
// (0x1B820) in the original. See include/evox/settings.h.
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "evox/dash.h"
#include "evox/globals.h"
#include "evox/guard.h"
#include "evox/info.h"
#include "evox/inifile.h"
#include "evox/system.h"
#include "evox/settings.h"
#include "evox/skinvars.h"

#define F32(bits) __builtin_bit_cast(float, (unsigned)(bits))

static_assert(sizeof(SettingRow) == 0x10, "SettingRow");
static_assert(sizeof(KeyboardKey) == 0x1C, "KeyboardKey");
static_assert(sizeof(KeyboardLayout) == 0x4B0, "KeyboardLayout");

SettingRow g_Settings[200];
int g_SettingsCount;

// the evox.ini values being edited (original addresses)
static char s_SetupNetwork[0x100];   // 0x47B048
static char s_StaticIP[0x100];       // 0x3D81B8
static char s_Ip[0x100];             // 0x3D80B8
static char s_Subnetmask[0x100];     // 0x3D82B8
static char s_Gateway[0x100];        // 0x3F87B8
static char s_Dns1[0x100];           // 0x3F9948
static char s_Dns2[0x100];           // 0x3F9840
static char s_SetupDelay[0x100];     // 0x3D7870
static char s_SkipIfNoLink[0x100];   // 0x3D7E70
static char s_FtpEnable[0x100];      // 0x47BB48
static char s_FtpPassword[0x100];    // 0x47B648
static char s_TelnetEnable[0x100];   // 0x479D48
static char s_RdtoolsEnable[0x100];  // 0x3F9740
static char s_RdtoolsName[0x100];    // 0x47B548
static char s_JumpToMsDash[0x100];   // 0x3F9B48
static char s_JumpIfNoLink[0x100];   // 0x3D7770
static char s_Use24[0x100];          // 0x47B148
static char s_SwapDate[0x100];       // 0x47B448
static char s_SntpServer[0x100];     // 0x47B948
static char s_Fahrenheit[0x100];     // 0x3F85B8
static char s_AutoLaunchGames[0x100]; // 0x3D83B8
static char s_AutoLaunchDvd[0x100];  // 0x3D7970
static char s_AutoLaunchAudio[0x100]; // 0x47B248
static char s_UseFDrive[0x100];      // 0x3D7F70
static char s_UseGDrive[0x100];      // 0x3F9A48
static char s_UseItems[0x100];       // 0x47B748
static char s_ScreenSaver[0x100];    // 0x47B848
static char s_ShadeLevel[0x100];     // 0x3D7670
static char s_Igr[0x100];            // 0x47BA48

static const WCHAR *const s_GameRegions[] = { // 0x94D88
    L"Default", L"Auto", L"North America (NTSC)", L"Japan (NTSC)", L"Rest of world (PAL)", nullptr};
static const WCHAR *const s_TsrTypes[] = {L"Disabled", L"Normal", L"Debug", nullptr}; // 0x94DA0

// the on-screen keyboards (0x951B0 text, 0x95660 Yes / No)
static WCHAR g_EditText[0x200] = L"Edit"; // 0x94DB0
#define KEY(l, code, x, y, w) {l, 1, code, F32(x), F32(y), F32(w), F32(0x41C00000)}
static KeyboardLayout s_TextKeyboard = { // at (102, 160), 436 x 160
    F32(0x42CC0000), F32(0x43200000), F32(0x43DA0000), F32(0x43200000), L"", g_EditText,
    {{KEY(L"A", 0, 0x41200000, 0x41200000, 0x41C00000), KEY(L"B", 0, 0x42180000, 0x41200000, 0x41C00000),
      KEY(L"C", 0, 0x42840000, 0x41200000, 0x41C00000), KEY(L"D", 0, 0x42BC0000, 0x41200000, 0x41C00000),
      KEY(L"E", 0, 0x42F40000, 0x41200000, 0x41C00000), KEY(L"F", 0, 0x43160000, 0x41200000, 0x41C00000),
      KEY(L"G", 0, 0x43320000, 0x41200000, 0x41C00000), KEY(L"H", 0, 0x434E0000, 0x41200000, 0x41C00000),
      KEY(L"I", 0, 0x436A0000, 0x41200000, 0x41C00000), KEY(L"1", 0, 0x43910000, 0x41200000, 0x41C00000),
      KEY(L"2", 0, 0x439F0000, 0x41200000, 0x41C00000), KEY(L"3", 0, 0x43AD0000, 0x41200000, 0x41C00000),
      KEY(L"0", 0, 0x43BB0000, 0x41200000, 0x41C00000), KEY(L".", 0, 0x43C90000, 0x41200000, 0x41C00000)},
     {KEY(L"J", 0, 0x41200000, 0x42180000, 0x41C00000), KEY(L"K", 0, 0x42180000, 0x42180000, 0x41C00000),
      KEY(L"L", 0, 0x42840000, 0x42180000, 0x41C00000), KEY(L"M", 0, 0x42BC0000, 0x42180000, 0x41C00000),
      KEY(L"N", 0, 0x42F40000, 0x42180000, 0x41C00000), KEY(L"O", 0, 0x43160000, 0x42180000, 0x41C00000),
      KEY(L"P", 0, 0x43320000, 0x42180000, 0x41C00000), KEY(L"Q", 0, 0x434E0000, 0x42180000, 0x41C00000),
      KEY(L"R", 0, 0x436A0000, 0x42180000, 0x41C00000), KEY(L"4", 0, 0x43910000, 0x42180000, 0x41C00000),
      KEY(L"5", 0, 0x439F0000, 0x42180000, 0x41C00000), KEY(L"6", 0, 0x43AD0000, 0x42180000, 0x41C00000),
      KEY(L"Del", 0x20, 0x43BB0000, 0x42180000, 0x42500000), {}},
     {KEY(L"S", 0, 0x41200000, 0x42840000, 0x41C00000), KEY(L"T", 0, 0x42180000, 0x42840000, 0x41C00000),
      KEY(L"U", 0, 0x42840000, 0x42840000, 0x41C00000), KEY(L"V", 0, 0x42BC0000, 0x42840000, 0x41C00000),
      KEY(L"W", 0, 0x42F40000, 0x42840000, 0x41C00000), KEY(L"X", 0, 0x43160000, 0x42840000, 0x41C00000),
      KEY(L"Y", 0, 0x43320000, 0x42840000, 0x41C00000), KEY(L"Z", 0, 0x434E0000, 0x42840000, 0x41C00000),
      KEY(L"_", 0, 0x436A0000, 0x42840000, 0x41C00000), KEY(L"7", 0, 0x43910000, 0x42840000, 0x41C00000),
      KEY(L"8", 0, 0x439F0000, 0x42840000, 0x41C00000), KEY(L"9", 0, 0x43AD0000, 0x42840000, 0x41C00000),
      KEY(L"Done", 0x21, 0x43BB0000, 0x42840000, 0x42500000), {}}},
};
static KeyboardLayout s_YesNoKeyboard = { // at (102, 160), 436 x 75
    F32(0x42CC0000), F32(0x43200000), F32(0x43DA0000), F32(0x42960000), nullptr, nullptr,
    {{KEY(L"Yes", 0x22, 0x41200000, 0x41200000, 0x43440000), KEY(L"No", 0x23, 0x43640000, 0x41200000, 0x43440000)}},
};
#undef KEY

static KeyboardLayout *g_Keyboard = &s_TextKeyboard; // 0x95B10
static bool g_KeyboardOpen;  // 0x47BF34
static int g_KeyRow;         // 0x47BF28
static int g_KeyCol;         // 0x47BF2C
static int g_KeyBlink;       // 0x47BF30 cursor blink counter
static int g_SettingsRepeat; // 0x47BF24 frames until the next key / move

// ---- OpenSettings (0x1A150): build the table --------------------------------------------------

struct TableBuilder {
    int n = 0;
    SettingRow &Add(const WCHAR *label, DWORD type)
    {
        SettingRow &r = g_Settings[n++];
        r.label = label;
        r.type = type;
        return r;
    }
    void Header(const WCHAR *label) { Add(label, SETTING_HEADER).value = (char *)""; }
    void Info(const WCHAR *label, const WCHAR *text) { Add(label, SETTING_INFO).live = text; }
    // an evox.ini value, copied into buf
    void Ini(const WCHAR *label, DWORD type, char *buf, const char *section, const char *key, const char *def)
    {
        strcpy(buf, g_Ini->GetString(section, key, def));
        Add(label, type).value = buf;
    }
};

void CEvoxDash::OpenSettings()
{
    memset(g_Settings, 0, sizeof g_Settings);
    // catch (...) 0x1B11E: release the lock (the table and its count stay as they were)
    if (!CrashGuard([&] {
        g_Ini->Lock();
        TableBuilder t;

        t.Header(L"Network");
        t.Info(L"Type", g_InfoNetType);
        t.Info(L"IP", g_VarIP);
        t.Info(L"Line Status", g_InfoLineStatus);
        t.Info(L"Ethernet MAC address", g_InfoMac);
        t.Ini(L"Setup Network", SETTING_YESNO, s_SetupNetwork, "Network", "SetupNetwork", "No");
        t.Ini(L"Use Static IP", SETTING_YESNO, s_StaticIP, "Network", "StaticIP", "No");
        t.Ini(L"IP", SETTING_ADDRESS, s_Ip, "Network", "Ip", "192.168.0.2");
        t.Ini(L"Subnetmask", SETTING_ADDRESS, s_Subnetmask, "Network", "Subnetmask", "255.255.255.0");
        t.Ini(L"DefaultGateway", SETTING_ADDRESS, s_Gateway, "Network", "Defaultgateway", "192.168.0.1");
        t.Ini(L"DNS", SETTING_ADDRESS, s_Dns1, "Network", "DNS1", "0.0.0.0");
        t.Ini(L"DNS", SETTING_ADDRESS, s_Dns2, "Network", "DNS2", "0.0.0.0");
        t.Ini(L"Setup Delay", SETTING_ADDRESS, s_SetupDelay, "Network", "SetupDelay", "0");
        t.Ini(L"Skip Setup If No Link", SETTING_YESNO, s_SkipIfNoLink, "Network", "SkipIfNoLink", "No");

        t.Header(L"Harddisk");
        t.Info(L"Security", g_InfoSecurity);
        t.Info(L"Free Space on C:", g_VarSpaceC);
        t.Info(L"Free Space on E:", g_VarSpaceE);
        if (g_HasFDrive)
            t.Info(L"Free Space on F:", g_VarSpaceF);
        if (g_HasGDrive)
            t.Info(L"Free Space on G:", g_VarSpaceG);
        t.Info(L"Free Space on X:", g_VarSpaceX);
        t.Info(L"Free Space on Y:", g_VarSpaceY);
        t.Info(L"Free Space on Z:", g_VarSpaceZ);

        t.Header(L"BIOS");
        t.Info(L"BIOS Version", g_VarBiosVer);
        t.Info(L"Kernel Version", g_VarKernelVer);
        t.Info(L"Xbox Hardware Info", g_InfoHardware);

        t.Header(L"FTP");
        t.Ini(L"Enable", SETTING_YESNO, s_FtpEnable, "FTP", "Enable", "Yes");
        t.Ini(L"Password", SETTING_TEXT, s_FtpPassword, "FTP", "Password", "xbox");
        t.Header(L"Telnet");
        t.Ini(L"Enable", SETTING_YESNO, s_TelnetEnable, "TELNET", "Enable", "Yes");
        t.Header(L"RDTools");
        t.Ini(L"Enable", SETTING_YESNO, s_RdtoolsEnable, "RDTOOLS", "Enable", "Yes");
        t.Ini(L"Name", SETTING_TEXT, s_RdtoolsName, "RDTOOLS", "Name", "EvoX");

        t.Header(L"Clock");
        t.Ini(L"Jump To Ms Dash", SETTING_YESNO, s_JumpToMsDash, "Clock", "JumpToMsDash", "No");
        t.Ini(L"Jump If No Link", SETTING_YESNO, s_JumpIfNoLink, "Clock", "JumpIfNoLink", "Yes");
        t.Ini(L"24 hour", SETTING_YESNO, s_Use24, "Clock", "use24", "Yes");
        t.Ini(L"SwapDate", SETTING_YESNO, s_SwapDate, "Clock", "SwapDate", "No");
        t.Ini(L"SNTP Server", SETTING_TEXT, s_SntpServer, "Clock", "SNTP_Server", "0.0.0.0");

        t.Header(L"Video");
        SettingRow &video = t.Add(L"Video Standard", SETTING_INFO);
        switch (XGetVideoStandard()) {
        case XC_VIDEO_STANDARD_NTSC_M: video.live = L"NTSC - North America"; break;
        case XC_VIDEO_STANDARD_NTSC_J: video.live = L"NTSC - Japan"; break;
        case XC_VIDEO_STANDARD_PAL_I: video.live = L"PAL"; break;
        default: video.live = L"Unknown"; break;
        }
        video.value = (char *)"";
        SettingRow &region = t.Add(L"Game Region", SETTING_CHOICE);
        m_gameRegionChoice = (int *)&region.value;
        *m_gameRegionChoice = strtol(g_Ini->GetString("Misc", "GameRegion", "0"), nullptr, 10);
        if (*m_gameRegionChoice >= 5)
            *m_gameRegionChoice = 0;
        region.live = s_GameRegions;

        t.Header(L"Misc");
        t.Info(L"M/B Temp", g_VarTemp1);
        t.Info(L"CPU Temp", g_VarTemp2);
        t.Info(L"HDD Temp", g_VarTemp3);
        t.Info(L"Dash Version", g_VarVersion);
        t.Ini(L"Fahrenheit", SETTING_YESNO, s_Fahrenheit, "Misc", "Fahrenheit", "No");
        t.Ini(L"Auto Launch Games", SETTING_YESNO, s_AutoLaunchGames, "Misc", "AutoLaunchGames", "Yes");
        t.Ini(L"Auto Launch DVD", SETTING_YESNO, s_AutoLaunchDvd, "Misc", "AutoLaunchDVD", "Yes");
        t.Ini(L"Auto Launch Audio", SETTING_YESNO, s_AutoLaunchAudio, "Misc", "AutoLaunchAudio", "Yes");
        t.Ini(L"Use F Drive", SETTING_YESNO, s_UseFDrive, "Misc", "UseFDrive", "No");
        t.Ini(L"Use G Drive", SETTING_YESNO, s_UseGDrive, "Misc", "UseGDrive", "No");
        t.Ini(L"Use Items.txt", SETTING_YESNO, s_UseItems, "Misc", "UseItems", "Yes");
        t.Ini(L"Screen Saver", SETTING_TEXT, s_ScreenSaver, "Misc", "ScreenSaver", "5");
        t.Ini(L"Shade Level", SETTING_TEXT, s_ShadeLevel, "Misc", "ShadeLevel", "90");
        SettingRow &tsr = t.Add(L"TSR", SETTING_CHOICE);
        m_tsrChoice = (int *)&tsr.value;
        *m_tsrChoice = strtol(g_Ini->GetString("Misc", "TSR_Type", "1"), nullptr, 10);
        if (*m_tsrChoice >= 3)
            *m_tsrChoice = 0;
        tsr.live = s_TsrTypes;
        t.Ini(L"In Game Reset", SETTING_YESNO, s_Igr, "Misc", "IGR", "No");

        if (ChameleonRead(0) == 0xAA) {
            t.Header(L"Chameleon Adapter");
            t.Info(L"Raw", g_InfoChameleonRaw);
            t.Info(L"Switch 1", g_InfoChameleonSw1);
            t.Info(L"Switch 2", g_InfoChameleonSw2);
            t.Info(L"Mode", g_InfoChameleonMode);
            t.Info(L"Current Bank", g_InfoChameleonBank);
        }

        t.Header(L"Exit");
        if (g_Ini->m_writable == true)
            t.Add(L"Save And Exit", SETTING_SAVE).value = (char *)"";
        t.Add(L"Exit", SETTING_EXIT).value = (char *)"";
        g_SettingsCount = t.n;
        g_Ini->Unlock();
    }))
        g_Ini->Unlock();
}

// ---- SaveSettings (0x1B440) -------------------------------------------------------------------

void CEvoxDash::SaveSettings()
{
    char number[0x80];
    // catch (...) 0x1B803: release the lock
    if (!CrashGuard([&] {
        g_Ini->Lock();
        g_Ini->SetValue("Network", "SetupNetwork", s_SetupNetwork);
        g_Ini->SetValue("Network", "StaticIP", s_StaticIP);
        g_Ini->SetValue("Network", "Ip", s_Ip);
        g_Ini->SetValue("Network", "Subnetmask", s_Subnetmask);
        g_Ini->SetValue("Network", "Defaultgateway", s_Gateway);
        g_Ini->SetValue("Network", "DNS1", s_Dns1);
        g_Ini->SetValue("Network", "DNS2", s_Dns2);
        g_Ini->SetValue("Network", "SetupDelay", s_SetupDelay);
        g_Ini->SetValue("Network", "SkipIfNoLink", s_SkipIfNoLink);
        g_Ini->SetValue("FTP", "Enable", s_FtpEnable);
        g_Ini->SetValue("FTP", "Password", s_FtpPassword);
        g_Ini->SetValue("Telnet", "Enable", s_TelnetEnable);
        g_Ini->SetValue("RDTOOLS", "Enable", s_RdtoolsEnable);
        g_Ini->SetValue("RDTOOLS", "Name", s_RdtoolsName);
        g_Ini->SetValue("Clock", "JumpToMsDash", s_JumpToMsDash);
        g_Ini->SetValue("Clock", "JumpIfNoLink", s_JumpIfNoLink);
        g_Ini->SetValue("Clock", "Use24", s_Use24);
        g_Ini->SetValue("Clock", "SwapDate", s_SwapDate);
        g_Ini->SetValue("Clock", "SNTP_Server", s_SntpServer);
        sprintf(number, "%d", *m_gameRegionChoice);
        g_Ini->SetValue("Misc", "GameRegion", number);
        g_Ini->SetValue("Misc", "Fahrenheit", s_Fahrenheit);
        g_Ini->SetValue("Misc", "AutoLaunchGames", s_AutoLaunchGames);
        g_Ini->SetValue("Misc", "AutoLaunchDVD", s_AutoLaunchDvd);
        g_Ini->SetValue("Misc", "AutoLaunchAudio", s_AutoLaunchAudio);
        g_Ini->SetValue("Misc", "UseFDrive", s_UseFDrive);
        g_Ini->SetValue("Misc", "UseGDrive", s_UseGDrive);
        g_Ini->SetValue("Misc", "UseItems", s_UseItems);
        g_Ini->SetValue("Misc", "ScreenSaver", s_ScreenSaver);
        g_Ini->SetValue("Misc", "ShadeLevel", s_ShadeLevel);
        sprintf(number, "%d", *m_tsrChoice);
        g_Ini->SetValue("Misc", "TSR_Type", number);
        g_Ini->SetValue("Misc", "IGR", s_Igr);
        g_Ini->Save();
        g_Ini->Unlock();
    }))
        g_Ini->Unlock();
}

// ---- DrawSettings (0x1B820): draw, and handle input while the screen is up (state 26) ---------

static const KeyboardKey &Key(int row, int col) { return g_Keyboard->keys[row][col]; }

void CEvoxDash::DrawSettings()
{
    const DWORD a = (DWORD)m_settingsFade; // the screen's fade, 0..255
    const DWORD frame = (a / 2) << 24 | 0xFFFFFF;
    DrawRect(F32(0x42A00000), F32(0x42700000), F32(0x440C0000), F32(0x43CB0000), // (80, 60) - (560, 406)
             (DWORD)(long long)((float)a * F32(0x3F19999A)) << 24 | 0x80, frame);    // * 0.6
    DrawRect(F32(0x42A80000), F32(0x42800000), F32(0x440B0000), F32(0x42B00000), // (84, 64) - (556, 88)
             (DWORD)(long long)((float)a * F32(0x3E99999A)) << 24 | 0x80, frame);    // * 0.3
    m_font.Begin();
    m_font.DrawText(F32(0x43A00000), F32(0x42800000), a << 24 | 0xFFFFFF, // (320, 64)
                    m_isSettings ? L"Settings / Info" : m_screenTitle, XBFONT_CENTER_X);

    // the highlight, then 13 rows from m_settingsTop
    float y = F32(0x42B40000); // 90
    float bar = (float)(m_settingsRow * 24) + y;
    DrawRect(F32(0x42BC0000), bar + F32(0x40000000), F32(0x44088000), F32(0x41C00000) + bar, // (94, +2) - (546, +24)
             (DWORD)(long long)((float)a * F32(0x3F000000)) << 24 | 0x8000, frame);
    WCHAR text[0x200];
    for (int i = m_settingsTop; i < m_settingsTop + 13; i++, y += F32(0x41C00000)) {
        const SettingRow &r = g_Settings[i];
        DWORD colour = 0xC0D0E0;
        if (r.label) {
            switch (r.type & 0xC0) {
            case 0:
                colour = 0xA0C0E0;
                m_font.DrawText(F32(0x42EC0000), y, a << 24 | colour, r.label, 0); // 118
                break;
            case SETTING_INFO:
                colour = 0xD0E0;
                m_font.DrawText(F32(0x42EC0000), y, a << 24 | colour, r.label, 0);
                break;
            default: // headers
                colour = 0xC0D000;
                m_font.DrawText(F32(0x43A00000), y, a << 24 | colour, r.label, XBFONT_CENTER_X);
                break;
            }
        }
        const WCHAR *value = nullptr;
        if ((r.type & 0x3F) == SETTING_CHOICE) {
            value = ((const WCHAR *const *)r.live)[(int)r.value];
        } else if (r.live) {
            value = (const WCHAR *)r.live;
        } else if (r.value) {
            swprintf(text, 0x200, L"%hs", r.value);
            value = text;
        }
        if (value)
            m_font.DrawText(F32(0x43AA0000), y, a << 24 | colour, value, 0); // 340
    }
    m_font.End();

    // the keyboard over it
    if (g_KeyboardOpen) {
        m_font.Begin();
        const KeyboardLayout &k = *g_Keyboard;
        float kx = k.x, ky = k.y;
        DrawRect(kx, ky, kx + k.w, ky + k.h, 0xFF000080, frame);
        float top = ky + F32(0x40800000); // + 4
        DrawRect(kx + F32(0x40800000), top, kx + k.w - F32(0x40800000), ky + F32(0x41E00000), 0xFF000080, frame);
        m_font.DrawText(k.w * F32(0x3F000000) + kx, top, a << 24 | 0xFFFFFF, g_Settings[m_settingsTop + m_settingsRow].label,
                        XBFONT_CENTER_X);
        ky += F32(0x41F00000); // + 30
        if (k.edit) {
            top = ky + F32(0x40800000);
            DrawRect(kx + F32(0x41200000), top, kx + k.w - F32(0x41200000), ky + F32(0x41E00000), 0xFF000080, frame);
            m_font.DrawText(kx + F32(0x41400000), top, a << 24 | 0xFFFFFF, k.edit, 0); // + 12
            float w, h;
            m_font.GetTextExtent(k.edit, &w, &h, TRUE);
            if ((g_KeyBlink & 0x1F) < 0xF) { // the cursor
                float cx = w + kx;
                DrawRect(cx + F32(0x41400000), ky + F32(0x41000000), F32(0x41B00000) + cx, ky + F32(0x41C00000),
                         0xFFC0C0C0, 0xFFC0C0C0); // (+12, +8) - (+22, +24)
            }
            ky += F32(0x41F00000);
        }
        for (int row = 0; row < 3; row++)
            for (int col = 0; col < 14; col++) {
                const KeyboardKey &key = k.keys[row][col];
                if (!key.label)
                    continue;
                DWORD fill = g_KeyCol == col && g_KeyRow == row ? 0xFF006000 : 0xFF000080;
                DrawRect(kx + key.x, ky + key.y, key.w + key.x + kx, key.h + key.y + ky, fill, frame);
                m_font.DrawText(key.w * F32(0x3F000000) + key.x + kx, key.h * F32(0x3F000000) + key.y + ky + F32(0x3F800000),
                                0xFFC0C0C0, key.label, XBFONT_CENTER_X | XBFONT_CENTER_Y);
            }
        g_KeyBlink++;
        m_font.End();
    }

    if (m_state != STATE_SETTINGS)
        return;

    int steps = 0;
    if (!g_KeyboardOpen) {
        if (m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER]) {
            ResetScreenSaver();
            steps = (int)(long long)((float)m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] * F32(0x3CA3D70A));
        }
        switch (m_remoteButton) { // the keyboard's page keys
        case 0x1C: steps = 8; m_remoteButton = 0x58; break;
        case 0x1D: steps = 8; m_remoteButton = 0x59; break;
        case 0x20: steps = 2; m_remoteButton = 0x58; break;
        case 0x22: steps = 2; m_remoteButton = 0x59; break;
        }
    }

    do {
        if (g_KeyboardOpen) {
            if (g_SettingsRepeat) {
                g_SettingsRepeat--;
                continue;
            }
            KeyboardInput();
            if (g_KeyboardOpen)
                continue;
        }
        // the list
        if (g_SettingsRepeat) {
            g_SettingsRepeat--;
            continue;
        }
        if ((m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_BACK) || m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] ||
            m_key == 8 || m_remoteButton == 0x27) {
            ResetScreenSaver();
            m_state = STATE_SETTINGS_OUT;
        }
        if ((m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) || m_key == 0x28 || m_remoteButton == 0x58) {
            ResetScreenSaver();
            if (m_settingsTop + m_settingsRow + 1 != g_SettingsCount) {
                if (m_settingsRow >= 12)
                    m_settingsTop++;
                else
                    m_settingsRow++;
                g_SettingsRepeat += 8;
            }
        }
        if ((m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) || m_key == 0x26 || m_remoteButton == 0x59) {
            ResetScreenSaver();
            if (m_settingsRow) {
                m_settingsRow--;
                g_SettingsRepeat += 8;
            } else if (m_settingsTop) {
                m_settingsTop--;
                g_SettingsRepeat += 8;
            }
        }
        if (m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] || m_key == 0x0D || m_remoteButton == 0xF4) {
            ResetScreenSaver();
            SettingRow &r = g_Settings[m_settingsTop + m_settingsRow];
            switch (r.type) {
            case SETTING_YESNO:
                g_Keyboard = &s_YesNoKeyboard;
                g_KeyboardOpen = true;
                g_SettingsRepeat += 8;
                g_KeyRow = 0;
                g_KeyCol = _stricmp(r.value, "yes") != 0;
                break;
            case SETTING_ADDRESS:
            case SETTING_TEXT:
                g_SettingsRepeat += 8;
                g_Keyboard = &s_TextKeyboard;
                g_KeyboardOpen = true;
                g_KeyCol = 0;
                g_KeyRow = 0;
                swprintf(g_EditText, 0x200, L"%hs", r.value);
                break;
            case SETTING_CHOICE: {
                g_SettingsRepeat += 8;
                int &choice = *(int *)&r.value;
                if (!((const WCHAR *const *)r.live)[++choice])
                    choice = 0;
                break;
            }
            case SETTING_SAVE:
                SaveSettings();
                m_state = STATE_SETTINGS_OUT;
                break;
            case SETTING_EXIT:
                m_state = STATE_SETTINGS_OUT;
                break;
            case SETTING_SAVE_TRAINER:
                SaveTrainerOptions();
                m_state = STATE_SETTINGS_OUT;
                break;
            }
        }
    } while (steps-- != 0);
}

// the keyboard's input (part of DrawSettings in the original)
void CEvoxDash::KeyboardInput()
{
    SettingRow &row = g_Settings[m_settingsTop + m_settingsRow];
    const XBGAMEPAD &pad = m_DefaultGamepad;

    if ((pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) || m_remoteButton == 0x58) {
        ResetScreenSaver();
        if (g_KeyCol == 13)
            g_KeyCol = 12;
        int r = g_KeyRow;
        do
            r = (r + 1) % 3;
        while (!Key(r, g_KeyCol).enabled);
        g_KeyRow = r;
        g_SettingsRepeat += 8;
    }
    if ((pad.wButtons & XINPUT_GAMEPAD_DPAD_UP) || m_remoteButton == 0x59) {
        ResetScreenSaver();
        if (g_KeyCol == 13)
            g_KeyCol = 12;
        int r = g_KeyRow;
        do
            r = r == 0 ? 2 : r - 1;
        while (!Key(r, g_KeyCol).enabled);
        g_KeyRow = r;
        g_SettingsRepeat += 8;
    }
    if ((pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) || m_remoteButton == 0x57 || m_key == 0x27) {
        ResetScreenSaver();
        int c = g_KeyCol;
        do
            c = (c + 1) % 14;
        while (!Key(g_KeyRow, c).enabled);
        g_KeyCol = c;
        g_SettingsRepeat += 8;
    }
    if ((pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) || m_remoteButton == 0x56 || m_key == 0x25) {
        ResetScreenSaver();
        int c = g_KeyCol;
        do
            c = c == 0 ? 13 : c - 1;
        while (!Key(g_KeyRow, c).enabled);
        g_KeyCol = c;
        g_SettingsRepeat += 8;
    }
    if (m_key == 'Y' || m_key == 'y') {
        ResetScreenSaver();
        g_SettingsRepeat += 8;
        g_KeyCol = 0;
    }
    if (m_key == 'N' || m_key == 'n') {
        ResetScreenSaver();
        g_SettingsRepeat += 8;
        g_KeyCol = 1;
    }
    if (pad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] || m_key == 8) { // delete a character
        ResetScreenSaver();
        if (g_Keyboard->edit) {
            if (size_t n = wcslen(g_Keyboard->edit))
                g_Keyboard->edit[n - 1] = 0;
            g_KeyBlink = 0;
        }
    }
    if (pad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y]) { // clear
        ResetScreenSaver();
        if (g_Keyboard->edit) {
            g_Keyboard->edit[0] = 0;
            g_KeyBlink = 0;
        }
    }
    if (pad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] || m_key == 0x1B || m_remoteButton == 0x27) { // cancel
        ResetScreenSaver();
        g_SettingsRepeat += 8;
        g_KeyboardOpen = false;
    }
    // typed letters, digits and '.' (not for Yes / No), while the text fits
    if (g_Keyboard != &s_YesNoKeyboard) {
        char c = (char)m_keyAscii;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.') {
            ResetScreenSaver();
            float w, h;
            m_font.GetTextExtent(g_Keyboard->edit, &w, &h, TRUE);
            if (!(g_Keyboard->w - F32(0x428C0000) <= w)) { // 70
                WCHAR s[2] = {(WCHAR)c, 0};
                wcscat(g_Keyboard->edit, s);
            }
        }
    }
    if (pad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] || m_remoteButton == 0xF4 || m_key == 0x0D) {
        ResetScreenSaver();
        const KeyboardKey &key = Key(g_KeyRow, g_KeyCol);
        switch (key.code) {
        case 0x20: // Del
            if (g_Keyboard->edit) {
                if (size_t n = wcslen(g_Keyboard->edit))
                    g_Keyboard->edit[n - 1] = 0;
                g_KeyBlink = 0;
            }
            break;
        case 0x21: // Done
            if (g_Keyboard->edit) {
                sprintf(row.value, "%ls", g_Keyboard->edit);
                g_SettingsRepeat += 8;
                g_KeyboardOpen = false;
            }
            break;
        case 0x22: // Yes
            strcpy(row.value, "Yes");
            g_SettingsRepeat += 8;
            g_KeyboardOpen = false;
            break;
        case 0x23: // No
            strcpy(row.value, "No");
            g_SettingsRepeat += 8;
            g_KeyboardOpen = false;
            break;
        default:
            if (g_Keyboard->edit) {
                float w, h;
                m_font.GetTextExtent(g_Keyboard->edit, &w, &h, TRUE);
                if (!(g_Keyboard->w - F32(0x428C0000) <= w))
                    wcscat(g_Keyboard->edit, key.label);
            }
            break;
        }
    }
}
