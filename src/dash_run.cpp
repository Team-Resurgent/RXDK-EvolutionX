// CEvoxDash::Run (0x1DDB0 in the original): the frame loop. Per frame: keyboard and gamepads
// (merged into m_DefaultGamepad as the sample framework does), timing and frame rate, Render,
// the Chameleon LED, then three steps:
//   m_action  a command for this frame (menu entries, servers): 0 none, see the switch below;
//             200..299 opens item (action - 200)
//   m_state   the screen's fade / transition state, 0 idle; each step moves a fade level by 8
//             (0..255) and ends in the next state
//   auto-launch of an inserted DVD, audio CD or game
// Run returns when the dashboard should leave (launching something, the servers' quit flags),
// with m_InstallIgr set for main; actions 5 and 32 halt from here.
#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "evox/dash.h"
#include "evox/globals.h"
#include "evox/pending.h"

#define F32(bits) __builtin_bit_cast(float, (unsigned)(bits))

DWORD g_ChameleonBlink = 1;
int g_DiscType = 3;
CMenu g_MenuMain, g_Menu47BC60, g_Menu47BCA8, g_Menu47BCCC, g_Menu47BCF0;
CMenu *g_MenuStack[0x102];
int g_MenuDepth;
int g_SelectedItem;

static const short XINPUT_DEADZONE = 0x1EB8; // 0.24 * 32768

// Lump all connected gamepads into m_DefaultGamepad (the sample framework's Run)
static void MergeGamepads(XBGAMEPAD *pads, XBGAMEPAD &out)
{
    memset(&out, 0, sizeof out);
    int lx = 0, ly = 0, rx = 0, ry = 0;
    for (int i = 0; i < 4; i++) {
        const XBGAMEPAD &g = pads[i];
        if (!g.hDevice)
            continue;
        if (g.sThumbLX > XINPUT_DEADZONE || g.sThumbLX < -XINPUT_DEADZONE)
            lx += g.sThumbLX;
        if (g.sThumbLY > XINPUT_DEADZONE || g.sThumbLY < -XINPUT_DEADZONE)
            ly += g.sThumbLY;
        if (g.sThumbRX > XINPUT_DEADZONE || g.sThumbRX < -XINPUT_DEADZONE)
            rx += g.sThumbRX;
        if (g.sThumbRY > XINPUT_DEADZONE || g.sThumbRY < -XINPUT_DEADZONE)
            ry += g.sThumbRY;
        out.fX1 += g.fX1;
        out.fY1 += g.fY1;
        out.fX2 += g.fX2;
        out.fY2 += g.fY2;
        out.wButtons |= g.wButtons;
        out.wPressedButtons |= g.wPressedButtons;
        out.wLastButtons |= g.wLastButtons;
        for (int b = 0; b < 8; b++) {
            out.bAnalogButtons[b] |= g.bAnalogButtons[b];
            out.bPressedAnalogButtons[b] |= g.bPressedAnalogButtons[b];
            out.bLastAnalogButtons[b] |= g.bLastAnalogButtons[b];
        }
    }
    out.sThumbLX = (SHORT)lx; // (no clamping, as in the original)
    out.sThumbLY = (SHORT)ly;
    out.sThumbRX = (SHORT)rx;
    out.sThumbRY = (SHORT)ry;
}

// fade levels: 8 per frame
static void FadeUp(float &f)
{
    f += F32(0x41000000);
    if (f > F32(0x437F0000)) // 255
        f = F32(0x437F0000);
}
static void FadeDown(float &f)
{
    f -= F32(0x41000000);
    if (f < 0.0f)
        f = 0.0f;
}

void CEvoxDash::Run()
{
    LARGE_INTEGER qwTicksPerSec;
    QueryPerformanceFrequency(&qwTicksPerSec);
    float fSecsPerTick = F32(0x3F800000) / (float)qwTicksPerSec.QuadPart;
    LARGE_INTEGER qwTime, qwLastTime, qwElapsedTime, qwAppTime, qwElapsedAppTime;
    QueryPerformanceCounter(&qwTime);
    qwLastTime = qwTime;
    qwAppTime.QuadPart = 0;
    static float s_fLastTime; // 0x47BF38
    static DWORD s_dwFrames;  // 0x47BF3C

    m_menu = &g_MenuMain;
    m_persistAlpha = F32(0x3F800000);
    m_InstallIgr = false;
    m_drawMenu = true;
    m_270 = 0;
    m_278 = -100;
    m_27C = false;
    m_action = 0;
    m_drawE8D = false;
    m_drawSettings = false;
    m_EA8 = 0;
    m_EAC = 0;
    m_E9C = 0;
    m_drawE98 = false;
    m_frame = 0;
    m_fade = 0;
    m_state = 2;
    m_drawMessage = false;
    m_FC8 = 0;
    m_FCC = 0;
    m_10D0 = false;
    m_268 = 0xFF;
    m_10D4 = 0;
    m_fadeAlpha = 0;
    m_192C = 0;
    m_ledFlags = 0;
    m_nextMenu = &g_MenuMain;

    for (;;) {
        m_frame++;
        m_keyboard = KeyboardPoll();
        XBInput_GetInput(m_Gamepad);
        MergeGamepads(m_Gamepad, m_DefaultGamepad);
        if ((m_keystroke = KeyboardGetKeystroke())) {
            m_key = m_keystroke[0];
            m_keyFlags = m_keystroke[1];
        } else {
            m_key = 0;
            m_keyFlags = 0;
        }

        // timing
        QueryPerformanceCounter(&qwTime);
        qwElapsedTime.QuadPart = qwTime.QuadPart - qwLastTime.QuadPart;
        qwLastTime = qwTime;
        qwElapsedAppTime.QuadPart = m_bPaused ? 0 : qwElapsedTime.QuadPart;
        qwAppTime.QuadPart += qwElapsedAppTime.QuadPart;
        m_fTime = fSecsPerTick * (float)qwTime.QuadPart;
        m_fElapsedTime = fSecsPerTick * (float)qwElapsedTime.QuadPart;
        m_fAppTime = fSecsPerTick * (float)qwAppTime.QuadPart;
        m_fElapsedAppTime = fSecsPerTick * (float)qwElapsedAppTime.QuadPart;
        s_dwFrames++;
        if (m_fTime - s_fLastTime > F32(0x3F800000)) {
            m_fFPS = (float)s_dwFrames / (m_fTime - s_fLastTime);
            s_dwFrames = 0;
            s_fLastTime = m_fTime;
            swprintf(m_strFrameRate, sizeof m_strFrameRate / sizeof m_strFrameRate[0], L"%0.02f fps", m_fFPS);
        }

        Render();

        // Chameleon LED: [Misc] ChameleonLed >= 2 blinks with that many frames on and off
        if (g_HasChameleon && (DWORD)g_ChameleonLed >= 2) {
            switch (g_ChameleonBlink / (DWORD)g_ChameleonLed) {
            case 0:
                ChameleonSetLed(0);
                g_ChameleonBlink++;
                break;
            case 1:
                ChameleonSetLed(1);
                g_ChameleonBlink++;
                break;
            default:
                g_ChameleonBlink = 1;
                break;
            }
        }
        if (m_action != 0 && g_HasChameleon == true) {
            if (m_ledFlags & 7)
                ChameleonSetColour(m_ledFlags & 7);
            if (m_ledFlags & 0x40)
                ChameleonSetFlash(0);
        }

        // ---- this frame's command ----
        switch (m_action) {
        case 0:
            goto action_done;
        case 1:
            m_state = 8;
            m_nextMenu = &g_Menu47BCA8;
            break;
        case 2: {
            m_state = 22;
            m_action = 0;
            m_27C = false;
            m_drawMessage = true;
            g_Unknown95BC0 = 0xFF;
            g_Unknown95BC1 = 0xFF;
            g_Unknown57E5C8 = 0;
            g_Unknown57EDC8 = 0;
            m_FCC = 0;
            strcpy(m_message, m_menu->GetItem(m_menu->selected)->path);
            goto action_done;
        }
        case 3: // the MS dashboard
            LaunchXbe(g_MsDashPath);
            m_action = 0;
            g_Unknown591A98 = g_IgrEnabled;
            goto leave;
        case 4:
            m_state = 8;
            m_nextMenu = &g_Menu47BC60;
            break;
        case 5:
            PrepareForExit();
            HalReturnToFirmware(HalHaltRoutine);
            return;
        case 8: { // launch the selected item
            m_action = 0;
            LaunchXbe(m_menu->GetItem(m_menu->selected)->path);
            if (m_menu->GetItem(m_menu->selected)->flags & 0x200)
                SetTrainer(m_menu->GetItem(m_menu->selected)->trainer);
            if (m_menu->GetItem(m_menu->selected)->flags & 0x100)
                g_Unknown95D64 = 0;
            g_IgrFlags = 0;
            if (m_menu->GetItem(m_menu->selected)->flags & 0x400)
                g_IgrFlags |= 1;
            if (m_menu->GetItem(m_menu->selected)->flags & 0x800)
                g_IgrFlags |= 2;
            switch (m_menu->GetItem(m_menu->selected)->flags & 0x3000) {
            case 0x1000:
                g_GameRegionOverride = 2;
                break;
            case 0x2000:
                g_GameRegionOverride = 3;
                break;
            case 0x3000:
                g_GameRegionOverride = 4;
                break;
            }
            g_Unknown591A98 = g_IgrEnabled;
            goto leave;
        }
        case 9:
            m_state = 25;
            m_action = 0;
            m_27C = false;
            m_drawSettings = true;
            m_EA4 = 0;
            m_EA8 = 0;
            m_EAC = 0;
            m_1911 = true;
            OpenSettings();
            goto action_done;
        case 10: { // (a debug leftover) IRQL test, written to e:\dump.txt
            m_action = 0;
            KIRQL old = KeRaiseIrqlToDpcLevel();
            KIRQL cur = KeGetCurrentIrql();
            HalInitiateShutdown();
            KfLowerIrql(old);
            FILE *f = fopen("e:\\dump.txt", "wt");
            fprintf(f, "%d\n", cur);
            fclose(f);
            goto action_done;
        }
        case 11:
            m_27C = false;
            m_state = 5;
            m_drawE98 = true;
            break;
        case 12:
            LoadSkin(m_menu->GetItem(m_menu->selected)->path);
            break;
        case 13:
            m_state = 8;
            m_nextMenu = &g_Menu47BCF0;
            break;
        case 14: // launch what is in the drive
            m_action = 0;
            if (g_DiscType == 0) {
                m_autoLaunchDvd = false;
                m_InstallIgr = true;
                m_state = 1;
                m_27C = false;
                LaunchXbe(m_dvdPlayer);
                g_Unknown591A98 = g_IgrEnabled;
            }
            if (g_DiscType == 1) {
                m_autoLaunchAudio = false;
                m_InstallIgr = true;
                m_state = 1;
                m_27C = false;
                LaunchXbe(m_audioPlayer);
                g_Unknown591A98 = g_IgrEnabled;
            }
            if (g_DiscType == 2 || g_DiscType == 4) {
                m_autoLaunchAudio = false; // (the original clears this flag here)
                m_InstallIgr = true;
                m_state = 1;
                m_27C = false;
                LaunchXbe("d:\\default.xbe");
                g_Unknown591A98 = g_IgrEnabled;
            }
            goto action_done;
        case 32:
            PrepareForExit();
            HalWriteSMBusValue(0x20, 0x1B, FALSE, 0);
            HalReturnToFirmware(HalHaltRoutine);
            return;
        case 33:
            m_state = 8;
            m_nextMenu = &g_Menu47BCCC;
            break;
        case 34:
            m_state = 25;
            m_action = 0;
            m_27C = false;
            m_drawSettings = true;
            m_EA4 = 0;
            m_EA8 = 0;
            m_EAC = 0;
            m_1911 = false;
            OpenFileView(m_menu->GetItem(m_menu->selected)->path);
            goto action_done;
        case 100:
            m_state = 8;
            m_nextMenu = m_menu->GetItem(m_menu->selected)->submenu;
            break;
        default:
            if (m_action >= 200 && m_action < 300) {
                g_SelectedItem = m_action - 200;
                m_action = 0;
                m_27C = false;
                m_drawE8D = true;
                m_E90 = 0;
                m_E94 = 0;
                m_state = 11;
                m_E8C = false;
                OpenItem();
            }
            break;
        }
        m_action = 0;
        goto action_done;
    leave: // fade out (state 1), then Run returns
        m_InstallIgr = true;
        m_state = 1;
        m_27C = false;
    action_done:

        // the servers can end the dashboard or put a message up
        if (g_DebugServer && g_DebugServer->m_quit == true) {
            g_Unknown591A98 = g_IgrEnabled;
            m_InstallIgr = true;
            return;
        }
        if (m_ftp) {
            if (m_ftp->m_quit == true) {
                g_Unknown591A98 = g_IgrEnabled;
                m_InstallIgr = true;
                return;
            }
            if (m_ftp->m_message == true) {
                m_ftp->m_message = false;
                m_action = 0;
                m_state = 22;
                m_27C = false;
                m_drawMessage = true;
                m_10D0 = true;
                m_FCC = 0;
                strcpy(m_message, m_ftp->m_messageText);
            }
        }

        // ---- fade / transition state ----
        switch (m_state) {
        case 1: // fade out, then leave Run
            m_fadeAlpha += F32(0x3C23D70A); // 0.01
            if (m_fadeAlpha > F32(0x3F800000))
                return;
            break;
        case 2:
            m_fade += F32(0x41000000);
            if (m_fade > F32(0x437F0000)) {
                m_fade = F32(0x437F0000);
                goto back_to_idle;
            }
            break;
        case 4:
            m_fade -= F32(0x41000000);
            if (m_fade < 0.0f) {
                m_27C = false;
                m_fade = 0;
                goto idle;
            }
            break;
        case 5:
            FadeDown(m_fade);
            if ((m_E9C += 8) > 0xFF) {
                Func20460();
                m_state = 6;
                m_E9C = 0xFF;
            }
            break;
        case 7:
            FadeUp(m_fade);
            if ((m_E9C -= 8) < 0) {
                m_drawE98 = false;
                m_E9C = 0;
                goto back_to_idle;
            }
            break;
        case 8: // to m_nextMenu, remembering this one
            m_fade -= F32(0x41000000);
            if (m_fade < 0.0f) {
                m_state = 2;
                m_27C = false;
                m_fade = 0;
                g_MenuStack[g_MenuDepth++] = m_menu;
                m_menu = m_nextMenu;
            }
            break;
        case 9: // back to m_nextMenu
            m_fade -= F32(0x41000000);
            if (m_fade < 0.0f) {
                m_state = 2;
                m_27C = false;
                m_fade = 0;
                m_menu = m_nextMenu;
            }
            break;
        case 11:
            FadeDown(m_fade);
            if ((m_E90 += 8) > 0xFF) {
                m_state = 12;
                m_E90 = 0xFF;
            }
            break;
        case 13:
            FadeUp(m_fade);
            if ((m_E90 -= 8) < 0) {
                m_drawE8D = false;
                m_E90 = 0;
                goto back_to_idle;
            }
            break;
        case 14:
            if (g_Unknown57E5BC == 0)
                m_state = 16;
            break;
        case 15:
            if ((m_E94 += 8) > 0xFF)
                m_E94 = 0xFF;
            if ((m_E90 -= 8) < 0) {
                m_state = 14;
                m_E90 = 0;
                Func20F20();
            }
            break;
        case 16:
            FadeUp(m_fade);
            if ((m_E94 -= 8) < 0) {
                m_state = 0;
                m_27C = true;
                m_drawE8D = false;
                m_E90 = 0;
                if (g_Unknown57E5C0 == true) {
                    m_action = 0;
                    m_state = 22;
                    m_27C = false;
                    m_drawMessage = true;
                    m_10D0 = true;
                    m_FCC = 0;
                    strcpy(m_message, g_Unknown47CFE0);
                    g_Unknown57E5C0 = false;
                }
            }
            break;
        case 22: // message in
            if ((m_FC8 += 8) > 0xFF) {
                m_state = 23;
                m_FC8 = 0xFF;
            }
            break;
        case 24: // message out
            if ((m_FC8 -= 8) < 0) {
                m_drawMessage = false;
                m_FC8 = 0;
                goto back_to_idle;
            }
            break;
        case 25: // settings in
            FadeDown(m_fade);
            if ((m_EA4 += 8) > 0xFF) {
                m_state = 26;
                m_EA4 = 0xFF;
                m_drawMenu = false;
            }
            break;
        case 27: // settings out
            m_drawMenu = true;
            FadeUp(m_fade);
            if ((m_EA4 -= 8) < 0) {
                m_drawSettings = false;
                m_EA4 = 0;
                goto back_to_idle;
            }
            break;
        }
        goto state_done;
    back_to_idle:
        m_27C = true;
    idle:
        m_state = 0;
    state_done:

        // auto-launch what was inserted
        if (m_autoLaunchDvd && g_DiscType == 0) {
            m_autoLaunchDvd = false;
            m_InstallIgr = true;
            m_state = 1;
            m_27C = false;
            LaunchXbe(m_dvdPlayer);
            g_Unknown591A98 = g_IgrEnabled;
        }
        if (m_autoLaunchAudio && g_DiscType == 1) {
            m_autoLaunchAudio = false;
            m_InstallIgr = true;
            m_state = 1;
            m_27C = false;
            LaunchXbe(m_audioPlayer);
            g_Unknown591A98 = g_IgrEnabled;
        }
        if (m_autoLaunchGames && g_DiscType == 2) {
            m_autoLaunchAudio = false; // (the original clears this flag, not AutoLaunchGames)
            m_InstallIgr = true;
            m_state = 1;
            m_27C = false;
            LaunchXbe("d:\\default.xbe");
            g_Unknown591A98 = g_IgrEnabled;
        }

        // Back, B, backspace (or keyboard code 0x27): input for the screen saver, and back to the
        // previous menu when idle
        if ((m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_BACK) || m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] ||
            m_key == 8 || m_keyboard == 0x27) {
            g_ScreenSaverDeadline = GetTickCount() + g_ScreenSaverMinutes * 60000;
            if (m_state == 0 && g_MenuDepth) {
                m_state = 9;
                m_nextMenu = g_MenuStack[--g_MenuDepth];
            }
        }
    }
}
