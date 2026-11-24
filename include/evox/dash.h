// The dashboard application object ("OpenX_Dash", 0x1940 bytes on main's stack).
// It began as the XDK sample framework's CXBApplication: the first 0x184 bytes keep that layout
// (present parameters, D3D objects, timing, gamepads). EvoX folded Initialize / FrameMove /
// Render into it and made Run a menu and fade state machine. Fields keep their offsets in the
// original; names say what the code does with them, "m_<offset>" where that is not known yet.
#pragma once
#include <xtl.h>
#include <stdio.h>
#include "evox/debugserver.h"
#include "evox/ftp.h"
#include "evox/menu.h"
#include "evox/xbfont.h"
#include "evox/xbinput.h"
#include "evox/xbmesh.h"
#include "evox/xbresource.h"

// ---- not rebuilt yet (src/pending.cpp) -----------------------------------------------------
struct SkinLabel {          // 0x418 bytes, a text label of the skin
    float alpha;            // +0x000 0..1
    float x, y;             // +0x004
    DWORD rgb;              // +0x00C
    DWORD flags;            // +0x010 XBFONT_*
    WCHAR text[0x200];      // +0x014 with <variables>
    SkinLabel *next;        // +0x414
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
    void DrawBackground(); // 0x18010
    void DrawLogo();      // 0x17CD0
    void DrawLabels();    // 0x1CA50
    void DrawQuad(IDirect3DTexture8 *tex, float alpha, void *verts);                     // 0x17450
    void DrawRect(float left, float top, float right, float bottom, DWORD fill, DWORD border); // 0x18140
    void LoadSkin(const char *name);                  // 0x1CC80
    void LoadBackground();                            // 0x17580
    void LoadLaunchPicture();                         // 0x17910
    void LoadLabels();                                // 0x1C890
    void InitItems();                                 // 0x1D680
    void DrawMenu();                                  // 0x18320 (m_drawMenu)
    void OpenSettings();                              // 0x1A150 action 9
    void SaveSettings();                              // 0x1B440 "Save And Exit"
    void DrawSettings();                              // 0x1B820 (m_drawSettings)
    void KeyboardInput();                             // (part of DrawSettings)
    // trainers (src/trainer.cpp)
    void OpenTrainerOptions(struct TrainerFile *trainer); // 0x1B130 action 34
    void SaveTrainerOptions();                        // 0x1B380 "Save And Exit" there
    void SaveTrainerFile();                           // 0x195E0 E:\TDATA\EVOX\Evox.etf
    // the menus from folders and [Menu] (src/dash_menus.cpp)
    void ScanBios(const char *dir);                   // 0x19480 the BIOS menu
    void ScanSkins(const char *dir);                  // 0x1D440 the skin menu
    void ScanTrainers(const char *dir);               // 0x19750 the trainer menu
    void PrepareMenuLine(char *line);                 // 0x199A0
    bool Tokenize(char *line);                        // 0x199E0 into m_argc / m_argv
    void ParseMenuSection(CMenu *menu);               // 0x19B20 a [Menu] section block
    void AutoAddItem(CMenu *menu, char *dir, int depth); // 0x191B0 the games in a folder
    const WCHAR *GetXbeTitle(const char *path);       // 0x18DC0
    void DrawBackupBox();                             // 0x18D10 (m_drawE98) src/backup.cpp
    void OpenItem();                                  // 0x1CAC0 actions 200..299 (src/actions.cpp)
    void DrawActionBox();                             // 0x187B0 (m_drawAction) its dialog
    void DrawMessage();                               // 0x18E30 (m_drawMessage) the "Flash BIOS" box (src/flash.cpp)
#ifdef EVOX_TEST_SCRIPT
    void RunTestScript();                             // test builds: src/testscript.cpp
#endif

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
    CXBFont m_font;                  // +0x184
    CXBPackedResource m_xpr;         // +0x1D4
    IDirect3DTexture8 *m_bgTexture;  // +0x1E4 the skin's background
    IDirect3DVertexBuffer8 *m_bgVB;  // +0x1E8 its quad
    IDirect3DTexture8 *m_fadeTexture; // +0x1EC drawn in state 1
    DWORD m_1F0;
    BOOL m_hasPersisted;             // +0x1F4 the last title's screen is shown and faded out
    IDirect3DSurface8 *m_persistSurface; // +0x1F8
    IDirect3DTexture8 *m_persistTexture; // +0x1FC
    CXBMesh *m_mesh;                 // +0x200
    BYTE m_fadeVerts[0x60];          // +0x204 vertices for the state-1 quad
    bool m_drawLogo;                 // +0x264
    BYTE pad265[3];
    int m_labelAlpha;                // +0x268 0..255
    float m_persistAlpha;            // +0x26C
    int m_270;                       // +0x270 the selection while DrawMenu handles input
    bool m_drawMenu;                 // +0x274
    BYTE pad275[3];
    int m_278;
    bool m_27C;                      // +0x27C
    BYTE pad27D[3];
    int m_action;                    // +0x280 command for the next frame (see Run)
    int m_state;                     // +0x284 fade / screen state (see Run)
    WCHAR m_actionTitle[0x100];      // +0x288 the action dialog: [Action_NN] info
    WCHAR m_actionText[0x400];       // +0x488 its Warning lines
    WCHAR m_actionProgress[0x100];   // +0xC88 progress, shown while the steps run
    int m_actionLines;               // +0xE88 Warning lines (the dialog's height)
    bool m_actionYes;                // +0xE8C Yes is selected
    bool m_drawAction;               // +0xE8D
    BYTE padE8E[2];
    int m_actionFade, m_progressFade; // +0xE90 the dialog's and the progress box's fade, 0..255
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
    int m_messageFade;               // +0xFC8 the "Flash BIOS" box's fade, 0..255
    int m_flashStep;                 // +0xFCC see DrawMessage
    char m_message[0x100];           // +0xFD0 the BIOS file to flash
    bool m_flashNow;                 // +0x10D0 flash without asking for Y (FTP FLASHBIOS, flashbios steps)
    BYTE pad10D1[3];
    int m_10D4;
    BYTE unk10D8[4];
    FILE *m_itemsFile;               // +0x10DC q:\items.txt being written
    char *m_menuLine;                // +0x10E0 [Menu] line being parsed
    float m_fadeAlpha;               // +0x10E4 state 1: fade out before leaving Run
    int m_logoType;                  // +0x10E8 [skin] LogoType, 0..4
    bool m_autoLaunchDvd;            // +0x10EC [Misc] AutoLaunchDVD
    char m_dvdPlayer[0x200];         // +0x10ED [Misc] DVDPlayer
    bool m_autoLaunchAudio;          // +0x12ED [Misc] AutoLaunchAudio
    char m_audioPlayer[0x200];       // +0x12EE [Misc] AudioPlayer
    bool m_autoLaunchGames;          // +0x14EE [Misc] AutoLaunchGames
    BYTE pad14EF;
    struct SkinLabel *m_labels;      // +0x14F0 the skin's text labels
    DWORD m_barColor1;               // +0x14F4 [skin] BarColor1: the selection bar
    DWORD m_barColor2;               // +0x14F8 [skin] BarColor2: its outline
    float m_barAlpha;                // +0x14FC [skin] BarAlpha
    DWORD m_menuColour[3];           // +0x1500 [skin] MenuColor1..3
    int m_argc;                      // +0x150C Tokenize: the words of the last line
    char *m_argv[0x100];             // +0x1510 (unused ones point at g_EmptyArg)
    bool m_noTrainers;               // +0x1910 ScanTrainers found none
    bool m_1911;
    BYTE pad1912[2];
    const WCHAR *m_screenTitle;      // +0x1914 the settings screen's title when m_1911 is false
    void *m_trainer;                 // +0x1918 trainer shown in the settings screen
    int m_trainerOptions;            // +0x191C
    const BYTE *m_keystroke;         // +0x1920 last keyboard keystroke, or NULL
    BYTE m_key;                      // +0x1924 its virtual key (8 backspace, 0x2A print screen)
    BYTE m_keyFlags;                 // +0x1925 its ASCII character (XINPUT_DEBUG_KEYSTROKE.Ascii)
    BYTE pad1926[2];
    int m_keyboard;                  // +0x1928 the DVD remote's button this frame (PollRemote)
    int m_192C;
    DWORD m_ledFlags;                // +0x1930 Chameleon LED commands
    int *m_gameRegionChoice;         // +0x1934 the Game Region row's choice (OpenSettings)
    int *m_tsrChoice;                // +0x1938 the TSR row's choice
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
extern CMenu g_Menu47BC60;
extern CMenu g_MenuBios;                 // 0x47BCA8 q:\bios (action 2 flashes the file)
extern CMenu g_MenuTrainers;             // 0x47BCCC q:\trainers\*.etm (action 34)
extern CMenu g_MenuSkins;                // 0x47BCF0 q:\skin\<name>\skin.ini (action 12)
extern char g_EmptyArg[0x100];           // 0x3D84B8 m_argv past m_argc (cleared by Create)

// a trainer file read by ScanTrainers: the trainer menu item's path points at this
struct TrainerFile {                     // 0x204 bytes
    char path[0x200];
    BYTE *data;                          // the whole .etm file
};
extern CMenu *g_MenuStack[0x102];        // 0x3F9538
extern int g_MenuDepth;                  // 0x3F9940
extern char g_MenuLine[0x600];           // 0x3D7A70 the [Menu] line being parsed
extern char g_ItemPaths[0x800][0x100];   // 0x3F9C48 XBEs [Menu] items name (AutoAddItem skips them)
extern int g_ItemPathCount;              // 0x47BC48
extern int g_SelectedItem;               // 0x57D3E0
