// The dashboard application object ("OpenX_Dash", 0x1940 bytes on main's stack).
// It began as the XDK sample framework's CXBApplication: the first 0x184 bytes keep that layout
// (present parameters, D3D objects, timing, gamepads). EvoX folded Initialize / FrameMove /
// Render into it and made Run a menu and fade state machine. Fields keep their offsets in the
// original; names say what the code does with them, "m_<offset>" where that is not known yet.
#pragma once
#include <xtl.h>
#include "evox/xbinput.h"

// ---- not rebuilt yet (src/pending.cpp) -----------------------------------------------------
class CEmbeddedResource { // 0x50 bytes; resources from an XPR inside the XBE (0x83750)
public:
    CEmbeddedResource();                                 // 0x25200
    ~CEmbeddedResource();                                // 0x256B0
    HRESULT Create(IDirect3DDevice8 *dev, DWORD unused); // 0x25250
    BYTE raw[0x50];
};

class CXBPackedResource { // 0x10 bytes; the sample framework's packed-resource loader
public:
    CXBPackedResource();  // 0x26110
    ~CXBPackedResource(); // 0x26280
    HRESULT Create(IDirect3DDevice8 *dev, const void *xpr, DWORD count, void *tags); // 0x26180
    BYTE raw[0x10];
};

class CXBMesh { // 0x18 bytes; the sample framework's XBG mesh (EvoX's logo model)
public:
    CXBMesh();                                                                     // 0x25BB0
    HRESULT Create(IDirect3DDevice8 *dev, const void *xbg, CXBPackedResource *res); // 0x25C80
    BYTE raw[0x18];
};

struct CMenuItem {          // (partial)
    BYTE unk00[0x10];
    char *path;             // +0x10 XBE or skin to launch / load
    BYTE unk14[0x08];
    struct CMenu *submenu;  // +0x1C
    BYTE unk20[0x04];
    DWORD flags;            // +0x24 0x100, 0x200 trainer, 0x400/0x800 IGR, 0x3000 region
    int trainer;            // +0x28
};

struct CMenu {              // 0x24 bytes (partial)
    BYTE unk00[0x1C];
    int selected;           // +0x1C
    BYTE unk20[0x04];
    CMenuItem *GetItem(int index); // 0x24E90
};
static_assert(sizeof(CMenu) == 0x24, "CMenu");

class CDebugServer { // 0x41E bytes
public:
    CDebugServer(); // 0x22FA0
    BYTE raw[0x41D];
    bool m_quit;    // +0x41D the dashboard should exit
};

class CFtpServer { // 0x5E0 bytes
public:
    CFtpServer(); // 0x14D90
    BYTE raw[0x35C];
    bool m_message;          // +0x35C a message for the screen is waiting
    char m_messageText[0x200]; // +0x35D
    bool m_quit;             // +0x55D
    BYTE unk55E[0x5E0 - 0x55E];
};

// ---- the dashboard -------------------------------------------------------------------------
class CEvoxDash {
public:
    CEvoxDash();          // 0x1CFE0
    virtual ~CEvoxDash(); // 0x173F0 (vtable 0x7F56C holds the deleting destructor 0x1D0C0)
    HRESULT Create();     // 0x1D920
    void Run();           // 0x1DDB0
    void Destroy();       // 0x1D0E0

    // rebuilt
    void Render();        // 0x1D130

    // not rebuilt yet (src/pending.cpp)
    void InitItems();                                 // 0x1D680 paths, items.txt
    void LoadSkin(const char *name);                  // 0x1CC80
    void ReleaseSkin();                               // 0x195E0
    void OpenSettings();                              // 0x1A150 action 9
    void OpenFileView(const char *path);              // 0x1B130 action 34
    void OpenItem();                                  // 0x1CAC0 actions 200..299
    void DrawBackground();                            // 0x18010
    void DrawPass17CD0();                             // 0x17CD0
    void DrawPass1CA50();                             // 0x1CA50
    void DrawMenu();                                  // 0x18320 (m_drawMenu)
    void DrawPass187B0();                             // 0x187B0 (m_drawE8D)
    void DrawSettings();                              // 0x1B820 (m_drawSettings)
    void DrawPass18D10();                             // 0x18D10 (m_drawE98)
    void DrawMessage();                               // 0x18E30 (m_drawMessage)
    void DrawQuad(IDirect3DTexture8 *tex, float alpha, void *verts);       // 0x17450
    void DrawRect(float x, float y, float w, float h, DWORD c1, DWORD c2); // 0x18140

    D3DPRESENT_PARAMETERS m_d3dpp;   // +0x004
    IDirect3D8 *m_pD3D;              // +0x048
    IDirect3DDevice8 *m_pd3dDevice;  // +0x04C
    IDirect3DSurface8 *m_pBackBuffer;  // +0x050
    IDirect3DSurface8 *m_pDepthBuffer; // +0x054
    FLOAT m_fTime;                   // +0x058
    FLOAT m_fElapsedTime;            // +0x05C
    FLOAT m_fAppTime;                // +0x060
    FLOAT m_fElapsedAppTime;         // +0x064
    BOOL m_bPaused;                  // +0x068 (never set by EvoX)
    FLOAT m_fFPS;                    // +0x06C
    WCHAR m_strFrameRate[0x16];      // +0x070
    XDEVICE_PREALLOC_TYPE *m_InputDeviceTypes; // +0x09C
    DWORD m_dwNumInputDeviceTypes;   // +0x0A0
    XBGAMEPAD *m_Gamepad;            // +0x0A4
    XBGAMEPAD m_DefaultGamepad;      // +0x0A8 all gamepads merged
    CEmbeddedResource m_embedded;    // +0x184
    CXBPackedResource m_xpr;         // +0x1D4
    DWORD m_1E4;
    IDirect3DResource8 *m_1E8;       // +0x1E8 released by Destroy
    IDirect3DTexture8 *m_fadeTexture; // +0x1EC drawn in state 1
    DWORD m_1F0;
    BOOL m_hasPersisted;             // +0x1F4 the last title's screen is shown and faded out
    IDirect3DSurface8 *m_persistSurface; // +0x1F8
    IDirect3DTexture8 *m_persistTexture; // +0x1FC
    CXBMesh *m_mesh;                 // +0x200
    BYTE m_fadeVerts[0x60];          // +0x204 vertices for the state-1 quad
    BYTE m_264;
    BYTE pad265[3];
    int m_268;
    float m_persistAlpha;            // +0x26C
    int m_270;
    bool m_drawMenu;                 // +0x274
    BYTE pad275[3];
    int m_278;
    bool m_27C;                      // +0x27C
    BYTE pad27D[3];
    int m_action;                    // +0x280 command for the next frame (see Run)
    int m_state;                     // +0x284 fade / screen state (see Run)
    BYTE unk288[0xE8C - 0x288];
    bool m_E8C;
    bool m_drawE8D;                  // +0xE8D
    BYTE padE8E[2];
    int m_E90, m_E94;                // +0xE90 fade levels, 0..255
    bool m_drawE98;                  // +0xE98
    BYTE padE99[3];
    int m_E9C;
    bool m_drawSettings;             // +0xEA0
    BYTE padEA1[3];
    int m_EA4, m_EA8, m_EAC;
    BYTE unkEB0[0xFB0 - 0xEB0];
    CFtpServer *m_ftp;               // +0xFB0
    DWORD m_frame;                   // +0xFB4
    float m_fade;                    // +0xFB8 screen fade, 0..255
    CMenu *m_menu;                   // +0xFBC current menu
    CMenu *m_nextMenu;               // +0xFC0
    bool m_drawMessage;              // +0xFC4
    BYTE padFC5[3];
    int m_FC8, m_FCC;
    char m_message[0x100];           // +0xFD0
    bool m_10D0;
    BYTE pad10D1[3];
    int m_10D4;
    BYTE unk10D8[0x10E4 - 0x10D8];
    float m_fadeAlpha;               // +0x10E4 state 1: fade out before leaving Run
    BYTE unk10E8[4];
    bool m_autoLaunchDvd;            // +0x10EC [Misc] AutoLaunchDVD
    char m_dvdPlayer[0x200];         // +0x10ED [Misc] DVDPlayer
    bool m_autoLaunchAudio;          // +0x12ED [Misc] AutoLaunchAudio
    char m_audioPlayer[0x200];       // +0x12EE [Misc] AudioPlayer
    bool m_autoLaunchGames;          // +0x14EE [Misc] AutoLaunchGames
    BYTE pad14EF;
    DWORD m_14F0;
    BYTE unk14F4[0x1911 - 0x14F4];
    bool m_1911;
    BYTE unk1912[0x1920 - 0x1912];
    const BYTE *m_keystroke;         // +0x1920 last keyboard keystroke, or NULL
    BYTE m_key;                      // +0x1924 its virtual key (8 backspace, 0x2A print screen)
    BYTE m_keyFlags;                 // +0x1925
    BYTE pad1926[2];
    int m_keyboard;                  // +0x1928
    int m_192C;
    DWORD m_ledFlags;                // +0x1930 Chameleon LED commands
    BYTE unk1934[0x193C - 0x1934];
    bool m_InstallIgr;               // +0x193C leave Run and run the IGR steps
    BYTE pad193D[3];
};

// globals the dashboard uses (original addresses)
extern IDirect3DDevice8 *g_pd3dDevice;   // 0x47BC5C
extern CDebugServer *g_DebugServer;      // 0x47BC4C
extern int g_Shade;                      // 0x47BC54 screen-saver shade, 0..g_ShadeLevel
extern DWORD g_ChameleonBlink;           // 0x94B58
extern int g_DiscType;                   // 0x95BBC 0 video DVD, 1 audio CD, 2/4 game
extern CMenu g_MenuMain;                 // 0x47BC84
extern CMenu g_Menu47BC60, g_Menu47BCA8, g_Menu47BCCC, g_Menu47BCF0;
extern CMenu *g_MenuStack[0x102];        // 0x3F9538
extern int g_MenuDepth;                  // 0x3F9940
extern int g_SelectedItem;               // 0x57D3E0
