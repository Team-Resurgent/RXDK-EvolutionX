// The dashboard's skin and main menu: LoadSkin and its three loaders, InitItems (the menus) and
// DrawMenu (original addresses per function). A skin is an evox.ini section [Skin_<name>];
// without one the built-in "Original" skin is used: the pictures inside the XBE (g_ResourceXpr).
#include <xtl.h>
#include <d3dx8.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "evox/dash.h"
#include "evox/globals.h"
#include "evox/guard.h"
#include "evox/inifile.h"
#include "evox/info.h"
#include "evox/system.h"

#define F32(bits) __builtin_bit_cast(float, (unsigned)(bits))

static char g_SkinSection[0x100];     // 0x3D8070 "Skin_<name>"
static int g_MenuRepeat;              // 0x47BD14 frames until the selection moves again

struct SkinVertex { // pre-transformed, texture coordinates in texels (linear textures)
    float x, y, z, rhw, u, v;
};

// the full-screen quad for a 640x480 picture; uMax/vMax: its texel size
static void FullScreenQuad(SkinVertex *v, float uMax, float vMax)
{
    v[0] = {F32(0xBF000000), F32(0xBF000000), 0, 0, 0, 0};       // (-0.5, -0.5)
    v[1] = {F32(0x441FE000), F32(0xBF000000), 0, 0, uMax, 0};    // (639.5, -0.5)
    v[2] = {F32(0x441FE000), F32(0x43EFC000), 0, 0, uMax, vMax}; // (639.5, 479.5)
    v[3] = {F32(0xBF000000), F32(0x43EFC000), 0, 0, 0, vMax};    // (-0.5, 479.5)
}

// "q:" + the first quoted part of [skin] key; false without the key (or on a fault reading it)
static bool SkinPicturePath(const char *key, char *path)
{
    bool found = false;
    // catch (...) 0x1764B (Main), 0x179DB (Launch): release the lock
    if (!CrashGuard([&] {
        g_Ini->Lock();
        if (char *v = g_Ini->GetValue(g_SkinSection, key))
            if (char *p = strtok(v, "\"")) {
                strcpy(path, "q:");
                strcat(path, p);
                found = true;
            }
        g_Ini->Unlock();
    }))
        g_Ini->Unlock();
    return found;
}

// A 640x480 linear texture from the file. Other sizes are loaded again resampled (D3DX's
// default filters) and, as in the original, mapped with texture size 1024x512.
static bool LoadPicture(IDirect3DDevice8 *dev, const char *path, IDirect3DTexture8 **tex, float *uMax, float *vMax)
{
    D3DXIMAGE_INFO info;
    if (D3DXCreateTextureFromFileExA(dev, path, 640, 480, 1, 1, D3DFMT_LIN_A8R8G8B8, (D3DPOOL)0, D3DX_FILTER_NONE,
                                     D3DX_FILTER_NONE, 0, &info, nullptr, tex) != S_OK)
        return false;
    if (info.Width == 640 && info.Height == 480) {
        *uMax = F32(0x44200000); // 640
        *vMax = F32(0x43F00000); // 480
        return true;
    }
    D3DResource_Release((D3DResource *)*tex);
    D3DXCreateTextureFromFileExA(dev, path, 640, 480, 1, 1, D3DFMT_LIN_A8R8G8B8, (D3DPOOL)0, D3DX_DEFAULT,
                                 D3DX_DEFAULT, 0, &info, nullptr, tex);
    *uMax = F32(0x44800000); // 1024
    *vMax = F32(0x44000000); // 512
    return true;
}

// 0x17580: [skin] Main, else the built-in background (resource 0)
void CEvoxDash::LoadBackground()
{
    m_bgTexture = nullptr;
    char path[0x100];
    float u = F32(0x44200000), v = F32(0x43F00000);
    if (!SkinPicturePath("Main", path) || !LoadPicture(m_pd3dDevice, path, &m_bgTexture, &u, &v)) {
        m_bgTexture = (IDirect3DTexture8 *)m_xpr.m_pSysMemData;
        u = F32(0x44200000);
        v = F32(0x43F00000);
    }
    m_bgVB = D3DDevice_CreateVertexBuffer2(4 * sizeof(SkinVertex));
    FullScreenQuad((SkinVertex *)D3DVertexBuffer_Lock2((D3DVertexBuffer *)m_bgVB, 0), u, v);
}

// 0x17910 (0x17B40 is part of it, split off by Ghidra): [skin] Launch, the picture faded in when
// leaving; else the built-in one (resource 1)
void CEvoxDash::LoadLaunchPicture()
{
    m_fadeTexture = nullptr;
    char path[0x100];
    float u = F32(0x44200000), v = F32(0x43F00000);
    if (!SkinPicturePath("Launch", path) || !LoadPicture(m_pd3dDevice, path, &m_fadeTexture, &u, &v)) {
        m_fadeTexture = (IDirect3DTexture8 *)(m_xpr.m_pSysMemData + 0x14);
        u = F32(0x44200000);
        v = F32(0x43F00000);
    }
    FullScreenQuad((SkinVertex *)m_fadeVerts, u, v);
}

// 0x1C890: [skin] text = x, y, alpha, colour, flags, "text" (any number of them)
void CEvoxDash::LoadLabels()
{
    m_labels = nullptr;
    // catch (...) 0x1CA37: release the lock, keeping the labels read so far
    if (!CrashGuard([&] {
        g_Ini->Lock();
        for (char *v = g_Ini->GetValue(g_SkinSection, "text"); v; v = g_Ini->GetNextValue()) {
            char *t;
            if (!(t = strtok(v, ",")))
                continue;
            float x = (float)atof(t);
            if (!(t = strtok(nullptr, ",")))
                continue;
            float y = (float)atof(t);
            if (!(t = strtok(nullptr, ",")))
                continue;
            float alpha = (float)atof(t);
            if (!(t = strtok(nullptr, ",")))
                continue;
            DWORD rgb = t[1] == 'x' ? strtoul(t + 2, nullptr, 16) : strtoul(t, nullptr, 10);
            if (!(t = strtok(nullptr, ",")))
                continue;
            DWORD flags = strtoul(t, nullptr, 10);
            if (!(t = strtok(nullptr, ",")) || !(t = strtok(t, "\"")))
                continue;
            SkinLabel *l = (SkinLabel *)operator new(sizeof(SkinLabel));
            l->next = m_labels;
            m_labels = l;
            l->alpha = alpha;
            l->x = x;
            l->y = y;
            l->rgb = rgb;
            l->flags = flags;
            swprintf(l->text, 0x200, L"%hs", t);
        }
        g_Ini->Unlock();
    }))
        g_Ini->Unlock();
}

// "0x......" colours; the second character is forced to 'x' first, as in the original
static DWORD SkinColour(const char *key, const char *def)
{
    char *s = g_Ini->GetString(g_SkinSection, key, def);
    s[1] = 'x';
    return strtol(s, nullptr, 16);
}

// 0x1CC80
void CEvoxDash::LoadSkin(const char *name)
{
    char skin[0x100];
    strcpy(skin, name);
    // the built-in pictures belong to the resource; loaded ones are released
    if (m_bgTexture != (IDirect3DTexture8 *)m_xpr.m_pSysMemData && m_bgTexture)
        D3DResource_Release((D3DResource *)m_bgTexture);
    if (m_fadeTexture != (IDirect3DTexture8 *)(m_xpr.m_pSysMemData + 0x14) && m_fadeTexture)
        D3DResource_Release((D3DResource *)m_fadeTexture);
    m_fadeTexture = nullptr;
    m_bgTexture = nullptr;

    // catch (...) 0x1CEE3: release the lock and load the pictures and labels all the same
    if (!CrashGuard([&] {
        g_Ini->Lock();
        sprintf(g_SkinSection, "Skin_%s", skin);
        m_logoType = 0;
        char *logo = g_Ini->GetValue(g_SkinSection, "LogoType");
        m_drawLogo = true;
        if (logo) {
            m_logoType = strtol(logo, nullptr, 10);
            if (m_logoType == 0x45766F58) { // LogoType=1165389656 ('XovE'): no logo
                m_drawLogo = false;
                m_logoType = 0;
            } else if (m_logoType >= 5) {
                m_logoType = 0;
            }
        } else { // no such skin (no LogoType): the built-in one
            strcpy(g_SkinSection, "Skin_Original");
            strcpy(skin, "Original");
        }
        m_barColor1 = SkinColour("BarColor1", "0x000080");
        m_barColor2 = SkinColour("BarColor2", "0x808080");
        m_barAlpha = (float)atof(g_Ini->GetString(g_SkinSection, "BarAlpha", "0.5"));
        m_menuColour[0] = SkinColour("MenuColor1", "0xc0c0c0");
        m_menuColour[1] = SkinColour("MenuColor2", "0xc00000");
        m_menuColour[2] = SkinColour("MenuColor3", "0x00c000");
        g_Ini->Unlock();
    }))
        g_Ini->Unlock();

    for (SkinLabel *l = m_labels, *next; l; l = next) {
        next = l->next;
        operator delete(l);
    }
    m_labels = nullptr;
    LoadBackground();
    LoadLaunchPicture();
    LoadLabels();

    // catch (...) 0x1CFC9: release the lock
    if (!CrashGuard([&] {
        g_Ini->Lock();
        if (_stricmp(g_Ini->GetString("Misc", "SkinName", "Original"), skin) != 0) {
            g_Ini->SetValue("Misc", "SkinName", skin);
            g_Ini->Save();
        }
        g_Ini->Unlock();
    }))
        g_Ini->Unlock();
}

// 0x1D680: the menus. The folders q:\bios, q:\skin and q:\trainers fill their menus; the main
// menu comes from evox.ini [Menu], or is the built-in one.
void CEvoxDash::InitItems()
{
    memset(g_ItemPaths, 0, sizeof g_ItemPaths);
    g_ItemPathCount = 0;
    g_MenuDepth = 0;
    ScanBios("q:\\bios");
    ScanSkins("q:\\skin");
    ScanTrainers("q:\\trainers");
    m_itemsFile = nullptr;
    if (g_UseItems)
        m_itemsFile = fopen("q:\\items.txt", "wt");

    // catch (...) 0x1D8F3: release the lock and close items.txt
    if (!CrashGuard([&] {
        g_Ini->Lock();
        if ((m_menuLine = g_Ini->FindSection("Menu"))) {
            for (; m_menuLine; m_menuLine = g_Ini->NextLine()) {
                strcpy(g_MenuLine, m_menuLine);
                PrepareMenuLine(g_MenuLine);
                if (g_MenuLine[0] == '#' || g_MenuLine[0] == ';')
                    continue;
                if (char *word = strtok(g_MenuLine, " ")) {
                    strlwr(word);
                    if (_stricmp(word, "section") == 0)
                        ParseMenuSection(&g_MenuMain);
                }
            }
        } else {
            if (g_MenuBios.count) // BIOS images found
                g_MenuBios.AddItem(0, 1, ACTION_END_OF_MENU, L"", nullptr);
            g_MenuMain.AddItem(1, 1, ACTION_MS_DASH, L"MS Dashboard", nullptr);
            g_MenuMain.AddItem(1, 1, ACTION_QUICK_REBOOT, L"Reboot", nullptr);
            g_MenuMain.AddItem(1, 1, ACTION_POWER_OFF, L"Power Off", nullptr);
            g_MenuMain.AddItem(1, 1, ACTION_SETTINGS, L"Settings", nullptr);
            g_MenuMain.AddItem(1, 1, ACTION_BACKUP, L"Backup", nullptr);
            if (g_MenuBios.count)
                g_MenuMain.AddItem(1, 1, ACTION_BIOS_MENU, L"Flash BIOS", nullptr);
        }
        g_MenuMain.AddItem(0, 1, ACTION_END_OF_MENU, L"", nullptr); // the end marker
        if (m_itemsFile)
            fclose(m_itemsFile);
        g_Ini->Unlock();
    })) {
        g_Ini->Unlock();
        if (m_itemsFile)
            fclose(m_itemsFile);
    }

    g_MenuMain.ResetSelection();
    g_MenuAction4.ResetSelection();
    g_MenuBios.ResetSelection();
    g_MenuSkins.ResetSelection();
    g_MenuTrainers.ResetSelection();
    InfoTick();
}

// 0x18320: the current menu with its selection bar, and its input: up / down (D-pad, arrow keys;
// the left trigger repeats the step up to 5 times a frame), A or Enter runs the item's action
void CEvoxDash::DrawMenu()
{
    DWORD barAlpha = (DWORD)(long long)(m_barAlpha * m_fade) << 24;
    DrawRect(F32(0x42C80000), F32(0x43AB0000), F32(0x44070000), F32(0x43B50000), m_barColor1 | barAlpha,
             m_barColor2 | barAlpha); // (100, 342) - (540, 362)

    int steps = (int)(long long)((float)m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] * F32(0x3CA3D70A));
    switch (m_remoteButton) { // the remote's repeating buttons move 8 or 2 items
    case 0x1C:
        steps = 8;
        m_remoteButton = 0x58;
        break;
    case 0x1D:
        steps = 8;
        m_remoteButton = 0x59;
        break;
    case 0x20:
        steps = 2;
        m_remoteButton = 0x58;
        break;
    case 0x22:
        steps = 2;
        m_remoteButton = 0x59;
        break;
    }

    int top;
    do {
        m_cursor = m_menu->selected;
        if (m_menuInput && g_MenuRepeat == 0) {
            if ((m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) || m_key == 0x28 || m_remoteButton == 0x58) {
                ResetScreenSaver();
                if (m_menu->GetItem(m_cursor + 1)->action != ACTION_END_OF_MENU) {
                    int i = m_cursor;
                    do {
                        i++;
                        g_MenuRepeat += 9;
                    } while (!m_menu->GetItem(i)->selectable && m_menu->GetItem(i)->action != ACTION_END_OF_MENU);
                    if (m_menu->GetItem(i)->selectable == 1)
                        m_cursor = i;
                    else
                        g_MenuRepeat = 0;
                }
            }
            if ((m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) || m_key == 0x26 || m_remoteButton == 0x59) {
                ResetScreenSaver();
                if (int i = m_cursor) {
                    do {
                        g_MenuRepeat += 9;
                        i--;
                    } while (i && !m_menu->GetItem(i)->selectable);
                    if (m_menu->GetItem(i)->selectable == 1)
                        m_cursor = i;
                    else
                        g_MenuRepeat = 0;
                }
            }
            if (m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] || m_key == 0x0D || m_remoteButton == 0xF4) {
                ResetScreenSaver();
                m_action = m_menu->GetItem(m_cursor)->action;
                m_actionExtraFlags = m_menu->GetItem(m_cursor)->extraFlags;
                m_ledFlags = m_menu->GetItem(m_cursor)->flags;
            }
        }
        m_menu->selected = m_cursor;
        if (g_MenuRepeat)
            g_MenuRepeat--;
        // ease the scroll towards the selection, 2 pixels a step (20 per item, selection 5th)
        int target = (m_cursor - 5) * 20;
        if (m_menu->scroll != target)
            m_menu->scroll += m_menu->scroll > target ? -2 : 2;
        top = m_menu->scroll / 20;
    } while (steps-- != 0);

    // up to 11 items, brightest in the middle (sine over the list), with a faint shadow
    for (int y = 0, i = top; y < 220; y += 20, i++) {
        if (i < 0)
            continue;
        if (m_menu->GetItem(i)->action == ACTION_END_OF_MENU)
            return;
        float t = (float)(y - m_menu->scroll % 20);
        float s = sinf(t * F32(0x40490FDB) * F32(0x3BA4A9CF)); // t * pi / 199
        if (s < 0.0f)
            continue;
        DWORD colour;
        switch (m_menu->GetItem(i)->colour) {
        case 2:
            colour = m_menuColour[1];
            break;
        case 3:
            colour = m_menuColour[2];
            break;
        default:
            colour = m_menuColour[0];
            break;
        }
        DWORD shadow = (DWORD)(long long)(m_fade * F32(0x3E4CCCCD) * s) << 24 | (colour & 0xFFFFFF); // 0.2
        m_font.DrawText(F32(0x439F0000), t + F32(0x43720000), shadow, m_menu->GetItem(i)->text, XBFONT_CENTER_X); // 319, +242
        DWORD main = (DWORD)(long long)(s * m_fade) << 24 | (colour & 0xFFFFFF);
        m_font.DrawText(F32(0x43A00000), t + F32(0x43700000), main, m_menu->GetItem(i)->text, XBFONT_CENTER_X); // 320, +240
    }
}
