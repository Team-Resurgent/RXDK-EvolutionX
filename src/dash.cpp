// The dashboard object: construction, Create, Destroy and Render (0x1CFE0, 0x1D920, 0x1D0E0,
// 0x1D130 in the original). Run is in dash_run.cpp. See include/evox/dash.h.
#include <xtl.h>
#include <d3dx8.h>
#include <xgraphics.h>
#include <string.h>
#include "evox/assets.h"
#include "evox/dash.h"
#include "evox/globals.h"
#include "evox/inifile.h"
#include "evox/log.h"
#include "evox/pending.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof" // (layout checks on purpose)
static_assert(offsetof(CEvoxDash, m_d3dpp) == 0x004, "m_d3dpp");
static_assert(offsetof(CEvoxDash, m_InputDeviceTypes) == 0x09C, "m_InputDeviceTypes");
static_assert(offsetof(CEvoxDash, m_DefaultGamepad) == 0x0A8, "m_DefaultGamepad");
static_assert(offsetof(CEvoxDash, m_embedded) == 0x184, "m_embedded");
static_assert(offsetof(CEvoxDash, m_xpr) == 0x1D4, "m_xpr");
static_assert(offsetof(CEvoxDash, m_mesh) == 0x200, "m_mesh");
static_assert(offsetof(CEvoxDash, m_action) == 0x280, "m_action");
static_assert(offsetof(CEvoxDash, m_E8C) == 0xE8C, "m_E8C");
static_assert(offsetof(CEvoxDash, m_ftp) == 0xFB0, "m_ftp");
static_assert(offsetof(CEvoxDash, m_message) == 0xFD0, "m_message");
static_assert(offsetof(CEvoxDash, m_fadeAlpha) == 0x10E4, "m_fadeAlpha");
static_assert(offsetof(CEvoxDash, m_autoLaunchDvd) == 0x10EC, "m_autoLaunchDvd");
static_assert(offsetof(CEvoxDash, m_autoLaunchGames) == 0x14EE, "m_autoLaunchGames");
static_assert(offsetof(CEvoxDash, m_keystroke) == 0x1920, "m_keystroke");
static_assert(offsetof(CEvoxDash, m_InstallIgr) == 0x193C, "m_InstallIgr");
static_assert(sizeof(CEvoxDash) == 0x1940, "CEvoxDash");
static_assert(sizeof(CDebugServer) == 0x41E, "CDebugServer");
static_assert(sizeof(CFtpServer) == 0x5E0, "CFtpServer");
static_assert(D3DFMT_LIN_A8R8G8B8 == 0x12, "D3DFMT_LIN_A8R8G8B8");

#define F32(bits) __builtin_bit_cast(float, (unsigned)(bits))

IDirect3DDevice8 *g_pd3dDevice;
CDebugServer *g_DebugServer;
int g_Shade;
static BYTE g_Unknown3D84B8[0x100]; // 0x3D84B8 cleared by Create

XBRESOURCE g_ResourceTags[] = {{"xray1.bmp", 0x28}, {"xray2.bmp", 0x3C}, {nullptr, 0}};

CEvoxDash::CEvoxDash()
{
    m_pD3D = nullptr;
    m_pd3dDevice = nullptr;
    m_pDepthBuffer = nullptr;
    m_pBackBuffer = nullptr;
    m_bPaused = FALSE;
    m_fTime = 0;
    m_fElapsedTime = 0;
    m_fAppTime = 0;
    m_fElapsedAppTime = 0;
    m_fFPS = 0;
    m_strFrameRate[0] = 0;
    memset(&m_d3dpp, 0, sizeof m_d3dpp);
    m_d3dpp.BackBufferWidth = 640;
    m_d3dpp.BackBufferHeight = 480;
    m_d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    m_d3dpp.BackBufferCount = 1;
    m_d3dpp.EnableAutoDepthStencil = TRUE;
    m_d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    m_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    if (XGetVideoFlags() & XC_VIDEO_FLAGS_WIDESCREEN)
        m_d3dpp.Flags = D3DPRESENTFLAG_WIDESCREEN;
    m_dwNumInputDeviceTypes = 0;
    m_InputDeviceTypes = nullptr;
    m_264 = 1;
    // Not set by the original constructor (Run sets it), but main reads it when Create fails:
    // there it was stack garbage. False here.
    m_InstallIgr = false;
}

// members m_xpr and m_embedded are destroyed after this, as in the original
CEvoxDash::~CEvoxDash() {}

HRESULT CEvoxDash::Create()
{
    memset(g_Unknown3D84B8, 0, sizeof g_Unknown3D84B8);
    if (!g_ResourceXprSize)
        Log("dash\t: built without EvoX's graphics (no evoxdash.xbe at build time)\n\r");

    // the sample framework's start-up
    Log("XBApp: Creating Direct3D...\n");
    if (!(m_pD3D = Direct3DCreate8(D3D_SDK_VERSION))) {
        Log("XBApp: Unable to create Direct3D!\n");
        return E_FAIL;
    }
    Log("XBApp: Creating the D3D device...\n");
    HRESULT hr = Direct3D_CreateDevice(0, D3DDEVTYPE_HAL, nullptr, D3DCREATE_HARDWARE_VERTEXPROCESSING, &m_d3dpp,
                                       &m_pd3dDevice);
    if (FAILED(hr)) {
        Log("XBApp: Could not create D3D device!\n");
        return hr;
    }
    g_pd3dDevice = m_pd3dDevice;
    m_pDepthBuffer = D3DDevice_GetDepthStencilSurface2();
    m_pBackBuffer = D3DDevice_GetBackBuffer2(0);
    XInitDevices(m_dwNumInputDeviceTypes, m_InputDeviceTypes);
    Log("XBApp: Creating gamepad devices...\n");
    if (FAILED(hr = XBInput_CreateGamepads(&m_Gamepad))) {
        Log("XBApp: Call to CreateGamepads() failed!\n");
        return hr;
    }

    // EvoX's own initialisation
    if (InitDevices23D70() < 0 || InitDevices23F70() < 0)
        return E_FAIL;
    if (m_embedded.Create(m_pd3dDevice, 0) < 0)
        return 1;
    // the last title's screen (D3DDevice_PersistDisplay), copied to fade it out in Render
    m_hasPersisted = FALSE;
    if (IDirect3DSurface8 *persisted = D3DDevice_GetPersistedSurface2()) {
        m_persistTexture = D3DDevice_CreateTexture2(640, 480, 1, 1, 0, D3DFMT_LIN_A8R8G8B8, D3DRTYPE_TEXTURE);
        m_persistSurface = D3DTexture_GetSurfaceLevel2(m_persistTexture, 0);
        D3DXLoadSurfaceFromSurface(m_persistSurface, nullptr, nullptr, persisted, nullptr, nullptr,
                                   D3DX_FILTER_TRIANGLE, 0);
        m_hasPersisted = TRUE;
    } else {
        g_Shade = 100;
    }
    if (m_xpr.Create(m_pd3dDevice, g_ResourceXpr, 4, g_ResourceTags) < 0)
        return 1;
    m_mesh = new CXBMesh;
    if (m_mesh->Create(m_pd3dDevice, g_LogoMesh, &m_xpr) < 0)
        return S_OK; // (as in the original: a failed mesh ends Create early, but succeeds)

    D3DXMATRIX proj, view;
    D3DXMatrixPerspectiveFovLH(&proj, F32(0x3F490FDB), F32(0x3FAAAAAB), F32(0x3DCCCCCD), F32(0x44FA0000)); // pi/4, 4/3, 0.1, 2000
    D3DDevice_SetTransform(D3DTS_PROJECTION, &proj);
    D3DXMatrixTranslation(&view, 0, 0, F32(0x42700000)); // z 60
    D3DDevice_SetTransform(D3DTS_VIEW, &view);

    m_ftp = nullptr;
    g_DebugServer = nullptr;
    if (g_NetworkStarted == true) {
        Log("dash\t: Start Debug\n\r");
        g_DebugServer = new CDebugServer;
        Log("dash\t: OK\n\r");
        Log("dash\t: Start FTP\n\r");
        m_ftp = new CFtpServer;
        Log("dash\t: OK\n\r");
    }
    m_1E4 = 0;
    m_fadeTexture = nullptr;
    m_14F0 = 0;
    InitItems();

    g_Ini->Lock();
    m_autoLaunchDvd = g_Ini->GetBool("Misc", "AutoLaunchDVD", true);
    m_autoLaunchAudio = g_Ini->GetBool("Misc", "AutoLaunchAudio", true);
    m_autoLaunchGames = g_Ini->GetBool("Misc", "AutoLaunchGames", true);
    if (char *p = g_Ini->GetString("Misc", "DVDPlayer", g_MsDashPath))
        strcpy(m_dvdPlayer, strtok(p, "\""));
    if (char *p = g_Ini->GetString("Misc", "AudioPlayer", g_MsDashPath))
        strcpy(m_audioPlayer, strtok(p, "\""));
    char *skin = g_Ini->GetString("Misc", "SkinName", "Original");
    g_Ini->Unlock();
    LoadSkin(skin); // (the name lives in the INI's work buffer, read after Unlock as in the original)
    return S_OK;
}

void CEvoxDash::Destroy()
{
    ReleaseSkin();
    if (m_1E8) {
        D3DResource_Release((D3DResource *)m_1E8);
        m_1E8 = nullptr;
    }
    if (m_pd3dDevice) {
        D3DDevice_Release();
        m_pd3dDevice = nullptr;
    }
    if (m_pD3D)
        m_pD3D = nullptr;
}

struct FadeVertex { // pre-transformed, texture in texels (linear texture)
    float x, y, z, w, u, v;
};

void CEvoxDash::Render()
{
    D3DDevice_Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
    DrawBackground();
    DrawPass17CD0();
    DrawPass1CA50();
    if (m_drawMenu)
        DrawMenu();
    if (m_drawE8D)
        DrawPass187B0();
    if (m_drawSettings)
        DrawSettings();
    if (m_drawE98)
        DrawPass18D10();
    if (m_drawMessage)
        DrawMessage();
    if (m_state == 1)
        DrawQuad(m_fadeTexture, m_fadeAlpha, m_fadeVerts);

    // the last title's screen over everything, fading out
    if (m_hasPersisted == TRUE) {
        FadeVertex v[4] = {
            {F32(0xBF000000), F32(0xBF000000), 0, 0, 0, 0},                       // (-0.5, -0.5)
            {F32(0x441FE000), F32(0xBF000000), 0, 0, F32(0x44200000), 0},          // (639.5, -0.5)
            {F32(0x441FE000), F32(0x43EFC000), 0, 0, F32(0x44200000), F32(0x43F00000)}, // (639.5, 479.5)
            {F32(0xBF000000), F32(0x43EFC000), 0, 0, 0, F32(0x43F00000)},          // (-0.5, 479.5)
        };
        DrawQuad(m_persistTexture, m_persistAlpha, v);
        m_persistAlpha -= F32(0x3C23D70A); // 0.01
        if (m_persistAlpha < 0.0f) {
            D3DResource_Release((D3DResource *)m_persistTexture);
            m_persistTexture = nullptr;
            m_hasPersisted = FALSE;
        }
    }

    // Black (or the keyboard's print-screen key) saves the back buffer
    if (m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_BLACK] || m_key == 0x2A)
        XGWriteSurfaceToFile(m_pBackBuffer, "e:\\screendump.bmp");

    // screen saver: shade in after [Misc] ScreenSaver minutes without input, up to ShadeLevel
    if (g_ScreenSaverMinutes) {
        DWORD shade = (DWORD)g_Shade;
        bool draw = true;
        if (GetTickCount() > g_ScreenSaverDeadline) {
            if (shade < (DWORD)g_ShadeLevel)
                g_Shade = shade += 2;
        } else if (shade) {
            g_Shade = shade -= 2;
        } else {
            draw = false;
        }
        if (draw && shade) {
            DWORD c = (DWORD)(long long)((float)shade * F32(0x40233333)) << 24; // 2.55
            DrawRect(0, 0, F32(0x44200000), F32(0x43F00000), c, c);             // 640 x 480
        }
    }
    D3DDevice_Swap(0);

    ClockTick();
    Tick1F610();
    Tick1FE70();
    Tick21820();
}
