// The menus that come from folders and from evox.ini [Menu] (original addresses per function):
// the BIOS, skin and trainer folder scans, the [Menu] line tokenizer and section parser, and the
// game folder scan of "AutoAddItem".
//
// [Menu] holds blocks like
//     Section "Root"
//     {
//         Item "Halo","e:\games\halo\default.xbe"
//         Item "Settings",ID_Settings
//         Item "Reboot",@5
//         AutoAddItem "e:\games\"
//         Section "Apps" ID_Chameleon_Mode_1
//         {
//             ...
//         }
//         Line "-----",2
//         SortAll
//     }
// Words are split at spaces, tabs and commas; "quotes" keep them together. ID_<name> words name
// an action or item flags (s_Keywords); Item's last other word is the XBE (action 8) or file.
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "evox/dash.h"
#include "evox/globals.h"
#include "evox/inifile.h"
#include "evox/log.h"
#include "evox/system.h"

char g_MenuLine[0x600];          // 0x3D7A70
char g_ItemPaths[0x800][0x100];  // 0x3F9C48
int g_ItemPathCount;             // 0x47BC48
static BYTE g_XbeHeader[0x200];  // 0x47BD20 GetXbeTitle's read buffer

// 0x94BB0: the ID_<name> words. A non-zero action replaces the item's; the flags are or-ed into
// the item's +0x20 and +0x24 (0x100 no D: mount, 0x200 the next word is a trainer, 0x400/0x800
// SwapStick, 0x1000..0x3000 game region, 0x04..0x07 Chameleon bank, 0x20..0x38 Chameleon mode
// (the item is only shown in that mode), 0x40 Chameleon release).
struct MenuKeyword {
    const char *name;
    int action;
    DWORD flags20, flags24;
};
static const MenuKeyword s_Keywords[] = {
    {"Full_Reboot", 32, 0, 0},
    {"Quick_Reboot", 5, 0, 0},
    {"Power_Off", 10, 0, 0},
    {"Flash_Bios", 1, 0, 0},
    {"Launch_DVD", 14, 0, 0},
    {"MS_Dash", 3, 0, 0},
    {"Settings", 9, 0, 0},
    {"Skins", 13, 0, 0},
    {"Backup", 11, 0, 0},
    {"SwapStick", 0, 0, 0xC00},
    {"SwapStick_Move", 0, 0, 0x400},
    {"SwapStick_Press", 0, 0, 0x800},
    {"No_D_Mount", 0, 0, 0x100},
    {"CMD", 0, 0, 0x200},
    {"Reg_NA", 0, 0, 0x1000},
    {"Reg_J", 0, 0, 0x2000},
    {"Reg_ROW", 0, 0, 0x3000},
    {"Trainer", 33, 0, 0},
    {"Flash_Bios_File", 2, 0, 0},
    {"Chameleon_Release_D0", 0, 0, 0x40},
    {"Chameleon_Bank_0", 0, 0, 0x4},
    {"Chameleon_Bank_1", 0, 0, 0x5},
    {"Chameleon_Bank_2", 0, 0, 0x6},
    {"Chameleon_Bank_3", 0, 0, 0x7},
    {"Chameleon_Mode_1", 0, 0, 0x20},
    {"Chameleon_Mode_2", 0, 0, 0x30},
    {"Chameleon_Mode_3", 0, 0, 0x28},
    {"Chameleon_Mode_4", 0, 0, 0x38},
};
static_assert(sizeof s_Keywords / sizeof s_Keywords[0] == 0x1C, "s_Keywords");

// 0x19AB0
static bool LookupKeyword(const char *name, int *action, DWORD *flags20, DWORD *flags24)
{
    for (const MenuKeyword &k : s_Keywords) {
        if (_stricmp(name, k.name) == 0) {
            if (k.action)
                *action = k.action;
            *flags20 |= k.flags20;
            *flags24 |= k.flags24;
            return true;
        }
    }
    return false;
}

// Chameleon mode words (flags 0x20..0x38): is the fitted Chameleon in that mode?
static bool ChameleonModeMatches(DWORD flags)
{
    bool ok = g_HasChameleon;
    BYTE mode = ChameleonRead(1);
    return ((((int)flags >> 3) ^ (mode >> 2)) & 3) == 0 && ok;
}

static DWORD Read32(const BYTE *p)
{
    DWORD v;
    memcpy(&v, p, 4);
    return v;
}

// 0x19480: every file in the folder flashes as a BIOS (action 2), named without its extension
void CEvoxDash::ScanBios(const char *dir)
{
    char search[0x200], path[0x200];
    WCHAR text[0x100];
    WIN32_FIND_DATA fd;
    strcpy(search, dir);
    strcat(search, "\\*.*");
    HANDLE h = FindFirstFile(search, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            strcpy(path, dir);
            strcat(path, "\\");
            strcat(path, fd.cFileName);
            if (char *dot = strrchr(fd.cFileName, '.'))
                *dot = 0;
            swprintf(text, 0x100, L"%hs", fd.cFileName);
            g_MenuBios.AddItem(1, 1, 2, text, path);
        } while (FindNextFile(h, &fd));
        FindClose(h);
    }
    g_MenuBios.SortAll();
}

// 0x1D440: "Original" (the built-in skin), then each folder whose skin.ini has a SkinName line:
// SkinName "<menu text>","<skin name>". The rest of that skin.ini is read into evox.ini's
// sections, not saved ([Skin_<name>] for LoadSkin). skin.ini is opened in binary mode, so an
// unquoted last word keeps the line's \r\n (as in the original).
void CEvoxDash::ScanSkins(const char *dir)
{
    char search[0x200], path[0x200], line[0x200];
    WCHAR text[0x100];
    WIN32_FIND_DATA fd;
    strcpy(search, dir);
    strcat(search, "\\*.*");
    g_MenuSkins.AddItem(1, 1, 12, L"Original", "Original");
    g_MenuSkins.MarkSortStart();
    HANDLE h = FindFirstFile(search, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                continue;
            strcpy(path, dir);
            strcat(path, "\\");
            strcat(path, fd.cFileName);
            strcat(path, "\\skin.ini");
            FILE *f = fopen(path, "rb");
            if (!f)
                continue;
            while (!feof(f)) {
                fgets(line, 0x200, f);
                Tokenize(line);
                if (_stricmp("SkinName", m_argv[0]) == 0) {
                    swprintf(text, 0x100, L"%hs", m_argv[1]);
                    g_MenuSkins.AddItem(1, 1, 12, text, m_argv[2]);
                    g_Ini->Lock();
                    g_Ini->LoadFromFile(f);
                    g_Ini->Unlock();
                    break;
                }
            }
            fclose(f);
        } while (FindNextFile(h, &fd));
        FindClose(h);
    }
    g_MenuSkins.SortFromStart();
}

// 0x19750: each .etm file is read whole; its item (action 34) shows the name stored in it and
// points at a TrainerFile instead of a path. "Sorry Trainers" when there are none.
void CEvoxDash::ScanTrainers(const char *dir)
{
    char search[0x200], path[0x200];
    WCHAR text[0x100];
    WIN32_FIND_DATA fd;
    int found = 0;
    m_noTrainers = true;
    strcpy(search, dir);
    strcat(search, "\\*.etm");
    g_MenuTrainers.MarkSortStart();
    HANDLE h = FindFirstFile(search, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            strcpy(path, dir);
            strcat(path, "\\");
            strcat(path, fd.cFileName);
            Log("Dash : Trainer Name %s\n", path);
            FILE *f = fopen(path, "rb");
            if (!f)
                continue;
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            BYTE *data = (BYTE *)malloc(size);
            fread(data, 1, size, f);
            fclose(f);
            TrainerFile *t = (TrainerFile *)malloc(sizeof(TrainerFile));
            t->data = data;
            strcpy(t->path, path);
            // the name: an offset at +0x0E points at the name's offset
            swprintf(text, 0x100, L"%hs", (const char *)(data + Read32(data + Read32(data + 0x0E))));
            CMenuItem *it = g_MenuTrainers.AddItem(1, 1, 34, text, nullptr);
            it->path = (char *)t;
            found++;
        } while (FindNextFile(h, &fd));
        FindClose(h);
    }
    if (found == 0)
        g_MenuTrainers.AddItem(1, 1, 0, L"Sorry Trainers", nullptr);
    else
        m_noTrainers = false;
    g_MenuTrainers.SortFromStart();
    g_MenuTrainers.AddItem(0, 1, -1, L"", nullptr); // the end marker
    Log("Dash : Trainer Done.");
}

// 0x199A0: tabs to spaces; the line ends at the first \r or \n
void CEvoxDash::PrepareMenuLine(char *line)
{
    for (char c = *line; c; c = *++line) {
        if (c == '\t')
            *line = ' ';
        else if (c == '\r' || c == '\n')
            *line = 0;
    }
}

// 0x199E0: split the line in place into m_argv (words end at a space, tab or comma; a "quoted"
// word at its closing quote). Unused m_argv entries point at an empty string. No bound on the
// word count (as in the original).
bool CEvoxDash::Tokenize(char *line)
{
    m_argc = 0;
    if (!line)
        return false;
    for (char *&a : m_argv)
        a = g_EmptyArg;
    int state = 0; // 0 between words, 1 in a quoted word, 2 in a plain word
    for (int i = 0; line[i]; i++) {
        char c = line[i];
        if (state == 0) {
            if (c == ' ' || c == '\t' || c == ',')
                continue;
            if (c == '"' && line[i + 1]) {
                state = 1;
                m_argv[m_argc++] = &line[i + 1];
            } else {
                state = 2;
                m_argv[m_argc++] = &line[i];
            }
        } else if (state == 1) {
            if (c == '"') {
                line[i] = 0;
                state = 0;
            }
        } else if (c == ',' || c == ' ' || c == '\t') {
            line[i] = 0;
            state = 0;
        }
    }
    return m_argc != 0;
}

// 0x19B20: the lines of one section block, up to its "}", into the menu. Lines starting with
// '#', ';' or '{' are skipped. A section whose Chameleon mode does not match is skipped up to
// the first "}" (nested blocks inside it are not counted, as in the original).
void CEvoxDash::ParseMenuSection(CMenu *menu)
{
    WCHAR text[0x84];
    g_MenuLine[0] = 0;
    for (m_menuLine = g_Ini->NextLine(); m_menuLine; m_menuLine = g_Ini->NextLine()) {
        strcpy(g_MenuLine, m_menuLine);
        PrepareMenuLine(g_MenuLine);
        if (!Tokenize(g_MenuLine))
            continue;
        char first = m_argv[0][0];
        if (first == '#' || first == ';' || first == '{')
            continue;
        if (first == '}')
            break;

        // Section "<text>" [ID_...]: an item (action 100) opening a submenu
        if (_stricmp(m_argv[0], "section") == 0) {
            int action = 0;
            DWORD flags20 = 0, flags24 = 0;
            for (int i = 2; i < m_argc; i++)
                if (_strnicmp(m_argv[i], "ID_", 3) == 0)
                    LookupKeyword(m_argv[i] + 3, &action, &flags20, &flags24);
            if (!(flags24 & 0x38) || ChameleonModeMatches(flags24)) {
                swprintf(text, 0x84, L"%hs", m_argv[1]);
                CMenuItem *it = menu->AddItem(1, 1, 100, text, nullptr);
                CMenu *sub = new CMenu;
                it->submenu = sub;
                ParseMenuSection(sub);
            } else {
                for (; m_menuLine; m_menuLine = g_Ini->NextLine()) {
                    strcpy(g_MenuLine, m_menuLine);
                    PrepareMenuLine(g_MenuLine);
                    if (Tokenize(g_MenuLine) && m_argv[0][0] == '}')
                        break;
                }
            }
        }

        // Item "<text>",@<action>[,<path>]   or   Item "<text>",<path>|ID_... ...
        if (_stricmp(m_argv[0], "item") == 0) {
            swprintf(text, 0x84, L"%hs", m_argv[1]);
            if (m_argv[2][0] == '@') {
                int action = strtol(m_argv[2] + 1, nullptr, 10);
                menu->AddItem(1, 1, action, text, m_argv[3][0] ? m_argv[3] : nullptr);
            } else {
                DWORD flags20 = 0, flags24 = 0;
                char *trainer = nullptr;
                int pathArg = -1;
                int action = 8;
                bool ok = true, firstTrainer = true;
                for (int i = 2; i < m_argc; i++) {
                    if (_strnicmp(m_argv[i], "ID_", 3) != 0) {
                        pathArg = i;
                        continue;
                    }
                    LookupKeyword(m_argv[i] + 3, &action, &flags20, &flags24);
                    if ((flags24 & 0x200) && firstTrainer) { // ID_CMD: the next word
                        firstTrainer = false;
                        i++;
                        trainer = (char *)malloc(strlen(m_argv[i]) + 1);
                        strcpy(trainer, m_argv[i]);
                    }
                }
                if (flags24 & 0x38)
                    ok = ChameleonModeMatches(flags24);
                CMenuItem *it = nullptr;
                if (ok) {
                    if (action != 8) {
                        it = menu->AddItem(1, 1, action, text, pathArg != -1 ? m_argv[pathArg] : nullptr);
                    } else if (pathArg != -1) {
                        // an XBE: remembered for AutoAddItem; greyed out if it cannot be opened
                        strcpy(g_ItemPaths[g_ItemPathCount++], m_argv[pathArg]);
                        if (FILE *f = fopen(m_argv[pathArg], "rb")) {
                            it = menu->AddItem(1, 1, 8, text, m_argv[pathArg]);
                            fclose(f);
                        } else {
                            it = menu->AddItem(0, 2, 8, text, m_argv[pathArg]);
                        }
                    }
                }
                if (it) {
                    it->m_20 = flags20;
                    it->flags = flags24;
                    it->trainer = trainer;
                }
            }
        }

        // AutoAddItem "<folder>": the games in it and its subfolders
        if (_stricmp(m_argv[0], "autoadditem") == 0)
            AutoAddItem(menu, m_argv[1], 0);
        // Line "<text>",<colour 0..2>: a text line that cannot be selected
        if (_stricmp(m_argv[0], "line") == 0) {
            swprintf(text, 0x84, L"%hs", m_argv[1]);
            menu->AddItem(0, strtol(m_argv[2], nullptr, 10) + 1, 8, text, nullptr);
        }
        if (_stricmp(m_argv[0], "sortall") == 0)
            menu->SortAll();
        if (_stricmp(m_argv[0], "sortstart") == 0)
            menu->MarkSortStart();
        if (_stricmp(m_argv[0], "sortend") == 0)
            menu->SortFromStart();
    }
    menu->ResetSelection();
    menu->AddItem(0, 0xFFFFFF, -1, L"", nullptr); // the end marker
}

// 0x191B0: each default.xbe in the folder (depth 0) or one level down (depth 1) that no Item
// names already: an item (action 8) with the XBE's title, or its folder's name when the title
// is empty; also written to q:\items.txt. A trailing backslash is cut off the folder in place.
void CEvoxDash::AutoAddItem(CMenu *menu, char *dir, int depth)
{
    char search[0x200], path[0x200], folder[0x200];
    WCHAR text[0x100];
    WIN32_FIND_DATA fd;
    size_t n = strlen(dir);
    if (dir[n - 1] == '\\')
        dir[n - 1] = 0;
    strcpy(search, dir);
    strcat(search, "\\*.*");
    HANDLE h = FindFirstFile(search, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth == 1)
                continue;
            strcpy(path, dir);
            strcat(path, "\\");
            strcat(path, fd.cFileName);
            AutoAddItem(menu, path, depth + 1);
            continue;
        }
        if (_stricmp(fd.cFileName, "default.xbe") != 0)
            continue;
        strcpy(path, dir);
        strcat(path, "\\");
        strcat(path, fd.cFileName);
        bool listed = false;
        for (int i = 0; i < g_ItemPathCount && !listed; i++)
            listed = _stricmp(path, g_ItemPaths[i]) == 0;
        if (listed)
            continue;
        // (the original reads the title without checking for NULL: an unreadable default.xbe
        // crashed it; here it is named after its folder)
        const WCHAR *title = GetXbeTitle(path);
        if (!title || !title[0]) {
            strcpy(folder, dir);
            const char *name = strrchr(folder, '\\') + 1;
            swprintf(text, 0x100, L"%hs", name);
            menu->AddItem(1, 1, 8, text, path);
            if (m_itemsFile)
                fprintf(m_itemsFile, "Item \"%s\",\"%s\"\r", name, path);
        } else {
            menu->AddItem(1, 1, 8, title, path);
            if (m_itemsFile)
                fprintf(m_itemsFile, "Item \"%ls\",\"%s\"\r", title, path);
        }
    } while (FindNextFile(h, &fd));
    FindClose(h);
}

// 0x18DC0: the title name from the XBE's certificate, or NULL if the file cannot be opened
const WCHAR *CEvoxDash::GetXbeTitle(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return nullptr;
    fread(g_XbeHeader, 1, 0x200, f);
    // the certificate's address (+0x118) less the base address (+0x104) is its file offset
    fseek(f, Read32(g_XbeHeader + 0x118) - Read32(g_XbeHeader + 0x104), SEEK_SET);
    fread(g_XbeHeader, 1, 0x200, f);
    fclose(f);
    return (const WCHAR *)(g_XbeHeader + 0x0C); // the certificate's title name
}
