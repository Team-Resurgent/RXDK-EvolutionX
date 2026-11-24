// Trainers: the options screen and the files (0x195E0, 0x1B130, 0x1B380 in the original); see
// include/evox/trainer.h for the .etm and Evox.etf layouts.
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "evox/dash.h"
#include "evox/log.h"
#include "evox/settings.h"
#include "evox/system.h"
#include "evox/trainer.h"

WCHAR g_TrainerTitle[0x100];
WCHAR g_TrainerLines[0x100][0x100];
char g_TrainerEnable[0x10];
char g_TrainerValues[200][0x10];

// 0x1B130 (action 34): the settings table becomes "Enable" (the trainer), one Yes/No row per
// option, "Save And Exit", then the information lines as headers
void CEvoxDash::OpenTrainerOptions(TrainerFile *trainer)
{
    memset(g_Settings, 0, sizeof g_Settings);
    m_trainer = trainer;
    BYTE *data = trainer->data;
    const BYTE *enables = data + Read32(data + 0x0A);
    const BYTE *strings = data + Read32(data + 0x0E);
    swprintf(g_TrainerTitle, 0x100, L"%hs", (const char *)(data + Read32(strings)));
    int options = 0;
    for (const BYTE *p = strings + 8; Read32(p); p += 4, options++)
        swprintf(g_TrainerLines[options], 0x100, L"%hs", (const char *)(data + Read32(p)));
    m_screenTitle = g_TrainerTitle;

    strcpy(g_TrainerEnable, data[0x22] ? "Yes" : "No");
    g_Settings[0].label = L"Enable";
    g_Settings[0].value = g_TrainerEnable;
    g_Settings[0].type = SETTING_YESNO;
    m_trainerOptions = options;
    int row = 1;
    for (; row < options + 1; row++) {
        strcpy(g_TrainerValues[row - 1], enables[row - 1] ? "Yes" : "No");
        g_Settings[row].label = g_TrainerLines[row - 1];
        g_Settings[row].value = g_TrainerValues[row - 1];
        g_Settings[row].type = SETTING_YESNO;
    }
    g_Settings[row].label = L"Save And Exit";
    g_Settings[row].value = (char *)"";
    g_Settings[row].type = SETTING_SAVE_TRAINER;
    row++;

    // the information lines: headers (their value is left NULL, as in the original)
    int line = options;
    for (const char *s = (const char *)(data + Read32(strings + 4)); *s; s += strlen(s) + 1, line++, row++) {
        swprintf(g_TrainerLines[line], 0x100, L"%hs", s);
        g_Settings[row].label = g_TrainerLines[line];
        g_Settings[row].type = SETTING_HEADER;
    }
    g_SettingsCount = row;
}

// 0x1B380 ("Save And Exit" on the trainer screen): the choices into the data, the data back into
// its .etm
void CEvoxDash::SaveTrainerOptions()
{
    TrainerFile *trainer = (TrainerFile *)m_trainer;
    BYTE *data = trainer->data;
    BYTE *enables = data + Read32(data + 0x0A);
    data[0x22] = _stricmp(g_TrainerEnable, "No") != 0;
    for (int i = 1; i < m_trainerOptions + 1; i++)
        enables[i - 1] = _stricmp(g_TrainerValues[i - 1], "No") != 0;
    if (FILE *f = fopen(trainer->path, "wb")) {
        fwrite(data, 1, Read32(data), f);
        fclose(f);
    }
}

// 0x195E0 (Destroy): E:\TDATA\EVOX\Evox.etf from the enabled trainers, unless there are none.
// (The pair table is never freed, as in the original.)
void CEvoxDash::SaveTrainerFile()
{
    if (m_noTrainers)
        return;
    int items = g_MenuTrainers.count - 1; // (the last item is the end marker)
    int ids = 0;
    for (int i = 0; i < items; i++) {
        const BYTE *data = ((TrainerFile *)g_MenuTrainers.GetItem(i)->path)->data;
        if (data[0x22] != 1)
            continue;
        for (const BYTE *p = data + Read32(data + 0x12); Read32(p); p += 4)
            ids++;
    }
    int dwords = ids * 2 + 2;
    DWORD *table = (DWORD *)operator new(dwords * 4);
    DWORD offset = dwords * 4; // the trainers' data follows the table
    int n = 0;
    for (int i = 0; i < items; i++) {
        const BYTE *data = ((TrainerFile *)g_MenuTrainers.GetItem(i)->path)->data;
        if (data[0x22] != 1)
            continue;
        for (const BYTE *p = data + Read32(data + 0x12); Read32(p); p += 4) {
            table[n++] = Read32(p);
            table[n++] = offset;
        }
        offset += Read32(data);
    }
    table[n] = 0;
    table[n + 1] = 0;
    FILE *f = fopen("E:\\TDATA\\EVOX\\Evox.etf", "wb");
    if (!f)
        return;
    fwrite(table, 4, dwords, f);
    Log("3\n");
    for (int i = 0; i < items; i++) {
        const BYTE *data = ((TrainerFile *)g_MenuTrainers.GetItem(i)->path)->data;
        if (data[0x22] == 1)
            fwrite(data, 1, Read32(data), f);
    }
    fclose(f);
}
