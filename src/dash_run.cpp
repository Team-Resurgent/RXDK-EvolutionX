// CEvoxDash::Run (0x1DDB0 in the original): the frame loop. Per frame: keyboard and gamepads
// (merged into m_DefaultGamepad as the sample framework does), timing and frame rate, Render,
// the Chameleon LED, then three steps:
//   m_action  a command for this frame (menu entries, servers): a DashAction (dash.h);
//             200..299 opens item (action - 200)
//   m_state   the screen's fade / transition, a DashState (dash.h); each step moves a fade by 8
//             (0..255) and ends in the next state
//   auto-launch of an inserted DVD, audio CD or game
// Run returns when the dashboard should leave (launching something, the servers' quit flags),
// with m_installIgr set for main; the reboot and power-off actions do not return.
#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "evox/actions.h"
#include "evox/dash.h"
#include "evox/devices.h"
#include "evox/flash.h"
#include "evox/globals.h"
#include "evox/log.h"
#include "evox/system.h"

#define F32(bits) __builtin_bit_cast(float, (unsigned)(bits))

DWORD g_ChameleonBlink = 1;
int g_DiscType = 3;
CMenu g_MenuMain, g_MenuAction4, g_MenuBios, g_MenuTrainers, g_MenuSkins;
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
    m_installIgr = false;
    m_drawMenu = true;
    m_cursor = 0;
    m_unused278 = -100;
    m_menuInput = false;
    m_action = ACTION_NONE;
    m_drawAction = false;
    m_drawSettings = false;
    m_settingsRow = 0;
    m_settingsTop = 0;
    m_backupFade = 0;
    m_drawBackup = false;
    m_frame = 0;
    m_fade = 0;
    m_state = STATE_MENU_IN;
    m_drawMessage = false;
    m_messageFade = 0;
    m_flashStep = 0;
    m_flashNow = false;
    m_labelAlpha = 0xFF;
    m_unused10D4 = 0;
    m_fadeAlpha = 0;
    m_actionExtraFlags = 0;
    m_ledFlags = 0;
    m_nextMenu = &g_MenuMain;

    for (;;) {
        m_frame++;
        m_remoteButton = PollRemote();
        XBInput_GetInput(m_Gamepad);
        MergeGamepads(m_Gamepad, m_DefaultGamepad);
        if ((m_keystroke = (const BYTE *)GetKeystroke())) {
            m_key = m_keystroke[0];
            m_keyAscii = m_keystroke[1];
        } else {
            m_key = 0;
            m_keyAscii = 0;
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
        if (m_action != ACTION_NONE && g_HasChameleon == true) {
            if (m_ledFlags & 7)
                ChameleonSetColour(m_ledFlags & 7);
            if (m_ledFlags & 0x40)
                ChameleonSetFlash(0);
        }

        // ---- this frame's command ----
        switch (m_action) {
        case ACTION_NONE:
            goto action_done;
        case ACTION_BIOS_MENU:
            m_state = STATE_SUBMENU;
            m_nextMenu = &g_MenuBios;
            break;
        case ACTION_FLASH_BIOS_FILE: {
            m_state = STATE_MESSAGE_IN;
            m_action = ACTION_NONE;
            m_menuInput = false;
            m_drawMessage = true;
            g_FlashManufacturer = 0xFF;
            g_FlashDevice = 0xFF;
            g_FlashLine1[0] = 0;
            g_FlashLine2[0] = 0;
            m_flashStep = 0;
            strcpy(m_message, m_menu->GetItem(m_menu->selected)->path);
            goto action_done;
        }
        case ACTION_MS_DASH: // the MS dashboard
            LaunchXbe(g_MsDashPath);
            m_action = ACTION_NONE;
            g_IgrOnLaunch = g_IgrEnabled;
            goto leave;
        case ACTION_MENU4:
            m_state = STATE_SUBMENU;
            m_nextMenu = &g_MenuAction4;
            break;
        case ACTION_QUICK_REBOOT:
            PrepareForExit();
            HalReturnToFirmware(HalHaltRoutine);
            return;
        case ACTION_LAUNCH: { // launch the selected item
            m_action = ACTION_NONE;
            LaunchXbe(m_menu->GetItem(m_menu->selected)->path);
            if (m_menu->GetItem(m_menu->selected)->flags & 0x200)
                SetTrainer(m_menu->GetItem(m_menu->selected)->trainer);
            if (m_menu->GetItem(m_menu->selected)->flags & 0x100)
                g_LaunchMountsD = 0;
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
            g_IgrOnLaunch = g_IgrEnabled;
            goto leave;
        }
        case ACTION_SETTINGS:
            m_state = STATE_SETTINGS_IN;
            m_action = ACTION_NONE;
            m_menuInput = false;
            m_drawSettings = true;
            m_settingsFade = 0;
            m_settingsRow = 0;
            m_settingsTop = 0;
            m_isSettings = true;
            OpenSettings();
            goto action_done;
        case ACTION_POWER_OFF: { // (a debug leftover) IRQL test, written to e:\dump.txt
            m_action = ACTION_NONE;
            KIRQL old = KeRaiseIrqlToDpcLevel();
            KIRQL cur = KeGetCurrentIrql();
            HalInitiateShutdown();
            KfLowerIrql(old);
            FILE *f = fopen("e:\\dump.txt", "wt");
            fprintf(f, "%d\n", cur);
            fclose(f);
            goto action_done;
        }
        case ACTION_BACKUP:
            m_menuInput = false;
            m_state = STATE_BACKUP_IN;
            m_drawBackup = true;
            break;
        case ACTION_LOAD_SKIN:
            LoadSkin(m_menu->GetItem(m_menu->selected)->path);
            break;
        case ACTION_SKIN_MENU:
            m_state = STATE_SUBMENU;
            m_nextMenu = &g_MenuSkins;
            break;
        case ACTION_LAUNCH_DISC: // launch what is in the drive
            m_action = ACTION_NONE;
            if (g_DiscType == 0) {
                m_autoLaunchDvd = false;
                m_installIgr = true;
                m_state = STATE_LEAVE;
                m_menuInput = false;
                LaunchXbe(m_dvdPlayer);
                g_IgrOnLaunch = g_IgrEnabled;
            }
            if (g_DiscType == 1) {
                m_autoLaunchAudio = false;
                m_installIgr = true;
                m_state = STATE_LEAVE;
                m_menuInput = false;
                LaunchXbe(m_audioPlayer);
                g_IgrOnLaunch = g_IgrEnabled;
            }
            if (g_DiscType == 2 || g_DiscType == 4) {
                m_autoLaunchAudio = false; // (the original clears this flag here)
                m_installIgr = true;
                m_state = STATE_LEAVE;
                m_menuInput = false;
                LaunchXbe("d:\\default.xbe");
                g_IgrOnLaunch = g_IgrEnabled;
            }
            goto action_done;
        case ACTION_FULL_REBOOT:
            PrepareForExit();
            HalWriteSMBusValue(0x20, 0x1B, FALSE, 0);
            HalReturnToFirmware(HalHaltRoutine);
            return;
        case ACTION_TRAINER_MENU:
            m_state = STATE_SUBMENU;
            m_nextMenu = &g_MenuTrainers;
            break;
        case ACTION_TRAINER_OPTIONS:
            m_state = STATE_SETTINGS_IN;
            m_action = ACTION_NONE;
            m_menuInput = false;
            m_drawSettings = true;
            m_settingsFade = 0;
            m_settingsRow = 0;
            m_settingsTop = 0;
            m_isSettings = false;
            OpenTrainerOptions((TrainerFile *)m_menu->GetItem(m_menu->selected)->path);
            goto action_done;
        case ACTION_SUBMENU:
            m_state = STATE_SUBMENU;
            m_nextMenu = m_menu->GetItem(m_menu->selected)->submenu;
            break;
        default:
            if (m_action >= ACTION_ITEM_FIRST && m_action < ACTION_ITEM_END) {
                g_SelectedItem = m_action - ACTION_ITEM_FIRST;
                m_action = ACTION_NONE;
                m_menuInput = false;
                m_drawAction = true;
                m_actionFade = 0;
                m_progressFade = 0;
                m_state = STATE_ACTION_IN;
                m_actionYes = false;
                OpenItem();
            }
            break;
        }
        m_action = ACTION_NONE;
        goto action_done;
    leave: // fade out (state 1), then Run returns
        m_installIgr = true;
        m_state = STATE_LEAVE;
        m_menuInput = false;
    action_done:

        // the servers can end the dashboard or put a message up
        if (g_DebugServer && g_DebugServer->m_quit == true) {
            g_IgrOnLaunch = g_IgrEnabled;
            m_installIgr = true;
            return;
        }
        if (m_ftp) {
            if (m_ftp->m_quit == true) {
                g_IgrOnLaunch = g_IgrEnabled;
                m_installIgr = true;
                return;
            }
            if (m_ftp->m_message == true) {
                m_ftp->m_message = false;
                m_action = ACTION_NONE;
                m_state = STATE_MESSAGE_IN;
                m_menuInput = false;
                m_drawMessage = true;
                m_flashNow = true;
                m_flashStep = 0;
                strcpy(m_message, m_ftp->m_messageText);
            }
        }

        // ---- fade / transition state ----
        switch (m_state) {
        case STATE_LEAVE: // fade out, then leave Run
            m_fadeAlpha += F32(0x3C23D70A); // 0.01
            if (m_fadeAlpha > F32(0x3F800000))
                return;
            break;
        case STATE_MENU_IN:
            m_fade += F32(0x41000000);
            if (m_fade > F32(0x437F0000)) {
                m_fade = F32(0x437F0000);
                goto back_to_idle;
            }
            break;
        case STATE_MENU_OUT:
            m_fade -= F32(0x41000000);
            if (m_fade < 0.0f) {
                m_menuInput = false;
                m_fade = 0;
                goto idle;
            }
            break;
        case STATE_BACKUP_IN:
            FadeDown(m_fade);
            if ((m_backupFade += 8) > 0xFF) {
                StartBackup();
                m_state = STATE_BACKUP;
                m_backupFade = 0xFF;
            }
            break;
        case STATE_BACKUP_OUT:
            FadeUp(m_fade);
            if ((m_backupFade -= 8) < 0) {
                m_drawBackup = false;
                m_backupFade = 0;
                goto back_to_idle;
            }
            break;
        case STATE_SUBMENU: // to m_nextMenu, remembering this one
            m_fade -= F32(0x41000000);
            if (m_fade < 0.0f) {
                m_state = STATE_MENU_IN;
                m_menuInput = false;
                m_fade = 0;
                g_MenuStack[g_MenuDepth++] = m_menu;
                m_menu = m_nextMenu;
            }
            break;
        case STATE_MENU_BACK: // back to m_nextMenu
            m_fade -= F32(0x41000000);
            if (m_fade < 0.0f) {
                m_state = STATE_MENU_IN;
                m_menuInput = false;
                m_fade = 0;
                m_menu = m_nextMenu;
            }
            break;
        case STATE_ACTION_IN:
            FadeDown(m_fade);
            if ((m_actionFade += 8) > 0xFF) {
                m_state = STATE_ACTION;
                m_actionFade = 0xFF;
            }
            break;
        case STATE_ACTION_OUT:
            FadeUp(m_fade);
            if ((m_actionFade -= 8) < 0) {
                m_drawAction = false;
                m_actionFade = 0;
                goto back_to_idle;
            }
            break;
        case STATE_ACTION_RUNNING:
            if (!g_ActionThread)
                m_state = STATE_PROGRESS_OUT;
            break;
        case STATE_PROGRESS_IN:
            if ((m_progressFade += 8) > 0xFF)
                m_progressFade = 0xFF;
            if ((m_actionFade -= 8) < 0) {
                m_state = STATE_ACTION_RUNNING;
                m_actionFade = 0;
                StartActionScript();
            }
            break;
        case STATE_PROGRESS_OUT:
            FadeUp(m_fade);
            if ((m_progressFade -= 8) < 0) {
                m_state = STATE_IDLE;
                m_menuInput = true;
                m_drawAction = false;
                m_actionFade = 0;
                if (g_ActionFlash == true) {
                    m_action = ACTION_NONE;
                    m_state = STATE_MESSAGE_IN;
                    m_menuInput = false;
                    m_drawMessage = true;
                    m_flashNow = true;
                    m_flashStep = 0;
                    strcpy(m_message, g_ActionFlashFile);
                    g_ActionFlash = false;
                }
            }
            break;
        case STATE_MESSAGE_IN: // message in
            if ((m_messageFade += 8) > 0xFF) {
                m_state = STATE_MESSAGE;
                m_messageFade = 0xFF;
            }
            break;
        case STATE_MESSAGE_OUT: // message out
            if ((m_messageFade -= 8) < 0) {
                m_drawMessage = false;
                m_messageFade = 0;
                goto back_to_idle;
            }
            break;
        case STATE_SETTINGS_IN: // settings in
            FadeDown(m_fade);
            if ((m_settingsFade += 8) > 0xFF) {
                m_state = STATE_SETTINGS;
                m_settingsFade = 0xFF;
                m_drawMenu = false;
            }
            break;
        case STATE_SETTINGS_OUT: // settings out
            m_drawMenu = true;
            FadeUp(m_fade);
            if ((m_settingsFade -= 8) < 0) {
                m_drawSettings = false;
                m_settingsFade = 0;
                goto back_to_idle;
            }
            break;
        }
        goto state_done;
    back_to_idle:
        m_menuInput = true;
    idle:
        m_state = STATE_IDLE;
    state_done:

        // auto-launch what was inserted
        if (m_autoLaunchDvd && g_DiscType == 0) {
            m_autoLaunchDvd = false;
            m_installIgr = true;
            m_state = STATE_LEAVE;
            m_menuInput = false;
            LaunchXbe(m_dvdPlayer);
            g_IgrOnLaunch = g_IgrEnabled;
        }
        if (m_autoLaunchAudio && g_DiscType == 1) {
            m_autoLaunchAudio = false;
            m_installIgr = true;
            m_state = STATE_LEAVE;
            m_menuInput = false;
            LaunchXbe(m_audioPlayer);
            g_IgrOnLaunch = g_IgrEnabled;
        }
        if (m_autoLaunchGames && g_DiscType == 2) {
            m_autoLaunchAudio = false; // (the original clears this flag, not AutoLaunchGames)
            m_installIgr = true;
            m_state = STATE_LEAVE;
            m_menuInput = false;
            LaunchXbe("d:\\default.xbe");
            g_IgrOnLaunch = g_IgrEnabled;
        }

        // Back, B, backspace (or keyboard code 0x27): input for the screen saver, and back to the
        // previous menu when idle
        if ((m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_BACK) || m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] ||
            m_key == 8 || m_remoteButton == 0x27) {
            ResetScreenSaver();
            if (m_state == STATE_IDLE && g_MenuDepth) {
                m_state = STATE_MENU_BACK;
                m_nextMenu = g_MenuStack[--g_MenuDepth];
            }
        }
    }
}
