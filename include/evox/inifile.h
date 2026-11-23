// The INI reader/writer behind evox.ini (0x11000..0x117BF in the original).
// Sections are a singly linked list headed by "Root" (lines before the first [section]); each
// section holds a singly linked list of raw lines. Lookups return pointers into the object's own
// work buffers, so a result is only valid until the next call. One critical section serialises
// users (Lock / Unlock); the methods themselves do not take it.
#pragma once
#include <xtl.h>
#include <stdio.h>

struct IniLine {     // 8 bytes
    char *text;      // +0x00 the whole line, as read
    IniLine *next;   // +0x04
};

struct IniSection {  // 0x2C bytes
    char name[0x20]; // +0x00 (copied unbounded, as in the original)
    IniSection *next;   // +0x20
    IniLine *lines;     // +0x24
    bool save;          // +0x28 written back by Save
};

class CIniFile {
public:
    CIniFile();          // 0x11000
    virtual ~CIniFile(); // 0x11060 (vtable 0x7DAE8 holds the deleting destructor 0x11710)

    void Lock();   // 0x11070
    void Unlock(); // 0x11080

    bool Load(const char *path);     // 0x111C0 also remembers path for Save
    bool LoadFromFile(FILE *f);      // 0x11660 sections read here are not saved
    bool SaveAs(const char *path);   // 0x112A0
    bool Save();                     // 0x11370

    // the value of key in section, trimmed; nullptr if absent
    char *GetValue(const char *section, const char *key); // 0x11420
    // the next line of the same section with the key of the last GetValue; nullptr at the end
    char *GetNextValue();                                  // 0x114E0
    bool GetBool(const char *section, const char *key, bool def);                  // 0x11730
    char *GetString(const char *section, const char *key, const char *def);       // 0x11780
    void SetValue(const char *section, const char *key, const char *value);       // 0x11590

    IniSection *SelectSection(const char *name, bool save); // 0x11090 find or append
    void AddLine(const char *text);                         // 0x11120 to the selected section
    char *FindSection(const char *name);                    // 0x11380 its first line, or nullptr
    char *NextLine();                                       // 0x113F0

    CRITICAL_SECTION m_lock;   // +0x004
    IniSection *m_first;       // +0x020 "Root"
    IniSection *m_section;     // +0x024 current section (see SelectSection)
    IniLine *m_line;           // +0x028 first line of the selected section
    IniLine *m_cursor;         // +0x02C line cursor
    char m_path[0x100];        // +0x030 file of the last Load
    char m_work[0x200];        // +0x130 parsed copy of a line; GetValue/GetString results
    char m_key[0x100];         // +0x330 key of the last GetValue
    char m_newLine[0x200];     // +0x430 line built by SetValue
    char m_lineCopy[0x200];    // +0x630 copy returned by FindSection / NextLine
    bool m_writable;           // +0x830 false: Save does nothing (main clears it on CD)
};

extern CIniFile *g_Ini; // 0x591A9C: evox.ini
