// The evox.ini reader/writer, 0x11000..0x117BF in the original. See include/evox/inifile.h.
#include <string.h>
#include "evox/inifile.h"
#include "evox/log.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof" // (the layout checks below are on purpose)

static_assert(sizeof(IniLine) == 0x08, "IniLine");
static_assert(sizeof(IniSection) == 0x2C, "IniSection");
static_assert(offsetof(CIniFile, m_lock) == 0x004, "m_lock");
static_assert(offsetof(CIniFile, m_first) == 0x020, "m_first");
static_assert(offsetof(CIniFile, m_path) == 0x030, "m_path");
static_assert(offsetof(CIniFile, m_writable) == 0x830, "m_writable");
static_assert(sizeof(CIniFile) == 0x834, "CIniFile");

CIniFile *g_Ini;

CIniFile::CIniFile()
{
    InitializeCriticalSection(&m_lock);
    m_first = new IniSection;
    strcpy(m_first->name, "Root");
    m_first->next = nullptr;
    m_first->lines = nullptr;
    // The original never sets Root's save flag, so it was whatever the heap held (fresh heap
    // pages are zero: Root's lines were not written back). Zero here.
    m_first->save = false;
    m_section = m_first;
    m_cursor = nullptr;
    m_line = nullptr;
    m_writable = true;
}

// The original destructor only resets the vtable: no sections or lines are freed.
CIniFile::~CIniFile() {}

void CIniFile::Lock() { EnterCriticalSection(&m_lock); }
void CIniFile::Unlock() { LeaveCriticalSection(&m_lock); }

// Find the section, else append a new one. NOTE: m_section is only updated for a new section,
// so AddLine on an existing, empty section links the line into the previously current one (as
// the original does).
IniSection *CIniFile::SelectSection(const char *name, bool save)
{
    IniSection *s = m_first;
    if (_stricmp(s->name, name) != 0) {
        for (;;) {
            if (!s->next) {
                IniSection *n = new IniSection;
                s->next = n;
                strcpy(n->name, name);
                n->next = nullptr;
                n->lines = nullptr;
                n->save = save;
                m_cursor = nullptr;
                m_line = nullptr;
                m_section = n;
                return n;
            }
            s = s->next;
            if (_stricmp(s->name, name) == 0)
                break;
        }
    }
    m_cursor = s->lines;
    m_line = s->lines;
    return s;
}

void CIniFile::AddLine(const char *text)
{
    char *copy = new char[strlen(text) + 1];
    strcpy(copy, text);
    IniLine *l = new IniLine;
    l->text = copy;
    l->next = nullptr;
    if (!m_cursor) {
        m_cursor = m_line;
        if (!m_line) {
            m_line = l;
            m_cursor = l;
            m_section->lines = l;
            return;
        }
    }
    while (m_cursor->next)
        m_cursor = m_cursor->next;
    m_cursor->next = l;
    m_cursor = l;
}

// Read "[section]" headers and lines. As in the original, the loop stops as soon as fgets sets
// end-of-file, so a last line without a newline is not read.
bool CIniFile::Load(const char *path)
{
    strcpy(m_path, path);
    FILE *f = fopen(path, "rt");
    if (!f)
        return false;
    SelectSection("Root", true);
    char buf[0x200];
    fgets(buf, 0x1FF, f);
    while (!feof(f)) {
        char *tok = strtok(buf, "\r\n");
        if (tok) {
            if (*tok == '[')
                SelectSection(strtok(buf, "[]"), true);
            else
                AddLine(tok);
        }
        fgets(buf, 0x1FF, f);
    }
    fclose(f);
    return true;
}

bool CIniFile::LoadFromFile(FILE *f)
{
    SelectSection("Root", true);
    char buf[0x200];
    fgets(buf, 0x1FF, f);
    while (!feof(f)) {
        char *tok = strtok(buf, "\r\n");
        if (tok) {
            if (*tok == '[')
                SelectSection(strtok(buf, "[]"), false);
            else
                AddLine(tok);
        }
        fgets(buf, 0x1FF, f);
    }
    return true;
}

bool CIniFile::SaveAs(const char *path)
{
    if (!m_writable)
        return false;
    FILE *f = fopen(path, "wt");
#ifdef EVOX_TEST_SCRIPT
    Log("test: SaveAs %s -> %p\n", path, f);
#endif
    if (!f)
        return false;
    for (m_section = m_first; m_section; m_section = m_section->next) {
        if (m_section->save != true)
            continue;
        if (m_section != m_first)
            fprintf(f, "[%s]\n\n", m_section->name);
        for (m_cursor = m_section->lines; m_cursor; m_cursor = m_cursor->next)
            fprintf(f, "%s\n", m_cursor->text);
        fprintf(f, "\n");
    }
    fclose(f);
    return true;
}

bool CIniFile::Save() { return SaveAs(m_path); }

char *CIniFile::FindSection(const char *name)
{
    for (m_section = m_first; m_section; m_section = m_section->next) {
        if (_stricmp(m_section->name, name) == 0) {
            m_cursor = m_section->lines;
            m_line = m_section->lines;
            if (!m_cursor)
                return nullptr;
            strcpy(m_lineCopy, m_cursor->text);
            return m_lineCopy;
        }
    }
    return nullptr;
}

char *CIniFile::NextLine()
{
    if (m_cursor)
        m_cursor = m_cursor->next;
    if (!m_cursor)
        return nullptr;
    strcpy(m_lineCopy, m_cursor->text);
    return m_lineCopy;
}

// Skip ' ', '=' and tab, then cut trailing spaces and tabs. The backward scan has no lower
// bound, so an empty value looks at the bytes before it (as in the original).
static char *TrimValue(char *v)
{
    while (*v == ' ' || *v == '=' || *v == '\t')
        v++;
    char *e = v + strlen(v);
    char c;
    do
        c = *--e;
    while (c == ' ' || c == '\t');
    e[1] = 0;
    return v;
}

char *CIniFile::GetValue(const char *section, const char *key)
{
    strcpy(m_key, key);
    for (char *line = FindSection(section); line; line = NextLine()) {
        strcpy(m_work, line);
        if (_stricmp(m_key, strtok(m_work, " =\t")) == 0) {
            char *v = strtok(nullptr, "");
            if (!v)
                return nullptr;
            return TrimValue(v);
        }
    }
    return nullptr;
}

char *CIniFile::GetNextValue()
{
    for (char *line = NextLine(); line; line = NextLine()) {
        strcpy(m_work, line);
        if (_stricmp(m_key, strtok(m_work, " =\t")) == 0) {
            char *v = strtok(nullptr, "");
            if (!v)
                return (char *)""; // the original returned its empty string constant here
            return TrimValue(v);
        }
    }
    return nullptr;
}

// "yes" or "enable" (any case) is true, anything else false; def if the key is absent
bool CIniFile::GetBool(const char *section, const char *key, bool def)
{
    char *v = GetValue(section, key);
    if (!v)
        return def;
    return _stricmp(v, "yes") == 0 || _stricmp(v, "enable") == 0;
}

char *CIniFile::GetString(const char *section, const char *key, const char *def)
{
    char *v = GetValue(section, key);
    if (!v) {
        strcpy(m_work, def);
        return m_work;
    }
    return v;
}

// Replace the key's line, or append "key<pad>value" to the section (creating it, saved).
// Keys shorter than 8 characters get "\t\t= ", longer ones "\t= ".
void CIniFile::SetValue(const char *section, const char *key, const char *value)
{
    char pad[5];
    if (strlen(key) < 8)
        strcpy(pad, "\t\t= ");
    else
        strcpy(pad, "\t= ");
    sprintf(m_newLine, "%s%s%s", key, pad, value);
    if (GetValue(section, key)) {
        delete[] m_cursor->text;
        m_cursor->text = new char[strlen(m_newLine) + 1];
        strcpy(m_cursor->text, m_newLine);
        return;
    }
    SelectSection(section, true);
    AddLine(m_newLine);
}
