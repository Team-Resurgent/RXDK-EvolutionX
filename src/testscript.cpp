// Test builds only: scripted input for the dashboard (tools/gen_test_script.py writes the script;
// build with EXTRA_CFLAGS='-DEVOX_TEST_SCRIPT=\"../work/test/<name>.h\"'). Each step runs on its
// frame of Run: a dashboard action, one frame of a button, a typed character, a log line or a
// file dumped to the log.
#ifdef EVOX_TEST_SCRIPT
#include <xtl.h>
#include <stdio.h>
#include "evox/dash.h"
#include "evox/log.h"

struct TestStep {
    DWORD frame;
    char op;          // 'A' action, 'B' analog button, 'D' digital button, 'K' typed key, 'L' log,
                      // 'F' dump file, 'M' dump the menus
    int arg;
    const char *text;
};
#include EVOX_TEST_SCRIPT // static const TestStep g_TestScript[]

// one line per item: selectable, colour, action, text, path (a trainer's file), flags, submenus
static void DumpMenu(const char *name, CMenu *menu, int depth)
{
    if (depth == 0)
        Log("menu %s: %d items\n", name, menu->count);
    for (CMenuItem *it = menu->first; it; it = it->next) {
        const char *path = it->path;
        if (it->action == 34 && path)
            path = ((TrainerFile *)path)->path;
        Log("%*s%d %06x %3d \"%ls\" %s %x/%x%s%s\n", depth * 2 + 2, "", it->selectable, it->colour, it->action,
            it->text, path ? path : "-", it->m_20, it->flags, it->trainer ? " trainer " : "",
            it->trainer ? it->trainer : "");
        if (it->submenu)
            DumpMenu(name, it->submenu, depth + 1);
    }
}

void CEvoxDash::RunTestScript()
{
    for (const TestStep &s : g_TestScript) {
        if (s.frame != m_frame)
            continue;
        switch (s.op) {
        case 'A':
            Log("test %u: action %d\n", m_frame, s.arg);
            m_action = s.arg;
            break;
        case 'B':
            m_DefaultGamepad.bAnalogButtons[s.arg] = 0xFF;
            m_DefaultGamepad.bPressedAnalogButtons[s.arg] = TRUE;
            break;
        case 'D':
            m_DefaultGamepad.wButtons |= (WORD)s.arg;
            m_DefaultGamepad.wPressedButtons |= (WORD)s.arg;
            break;
        case 'K':
            m_keyFlags = (BYTE)s.arg;
            break;
        case 'L':
            Log("test %u: %s\n", m_frame, s.text);
            break;
        case 'F': {
            Log("test %u: --- %s ---\n", m_frame, s.text);
            if (FILE *f = fopen(s.text, "rt")) {
                char line[0x200];
                while (fgets(line, sizeof line, f))
                    Log("%s", line);
                fclose(f);
            } else {
                Log("(cannot open)\n");
            }
            Log("test %u: --- end ---\n", m_frame);
            break;
        }
        case 'M':
            DumpMenu("main", &g_MenuMain, 0);
            DumpMenu("bios", &g_MenuBios, 0);
            DumpMenu("skins", &g_MenuSkins, 0);
            DumpMenu("trainers", &g_MenuTrainers, 0);
            Log("test %u: --- menus end ---\n", m_frame);
            break;
        }
    }
}
#endif
