// EvoX's menus, 0x24E30..0x251FF in the original. See include/evox/menu.h.
#include <string.h>
#include <wchar.h>
#include "evox/menu.h"

static_assert(sizeof(CMenuItem) == 0x2C, "CMenuItem");
static_assert(sizeof(CMenu) == 0x24, "CMenu");

CMenuItem g_EmptyMenuItem;

CMenu::CMenu()
{
    cursor = first = last = nullptr;
    cursorIndex = 0;
    selected = 0;
    scroll = -100;
    count = 0;
}

// select the first selectable item
void CMenu::ResetSelection()
{
    selected = 0;
    scroll = -100;
    cursor = first;
    cursorIndex = 0;
    while (cursor && cursor->selectable != 1) {
        cursorIndex++;
        scroll += 20;
        selected++;
        cursor = cursor->next;
    }
}

// walk from the cursor to the item; past either end: g_EmptyMenuItem
CMenuItem *CMenu::GetItem(int index)
{
    if (!cursor) {
        cursor = first;
        cursorIndex = 0;
        if (!cursor)
            return &g_EmptyMenuItem;
    }
    while (index != cursorIndex) {
        if ((unsigned)index > (unsigned)cursorIndex) {
            cursor = cursor->next;
            cursorIndex++;
        } else {
            cursor = cursor->prev;
            cursorIndex--;
        }
        if (!cursor)
            return &g_EmptyMenuItem;
    }
    return cursor;
}

CMenuItem *CMenu::AddItem(BYTE selectable, DWORD colour, int action, const WCHAR *text, const char *path)
{
    CMenuItem *it = (CMenuItem *)operator new(sizeof(CMenuItem));
    memset(it, 0, sizeof *it);
    WCHAR *t = (WCHAR *)operator new(wcslen(text) * 2 + 2);
    wcscpy(t, text);
    char *p = nullptr;
    if (path) {
        p = (char *)operator new(strlen(path) + 1);
        strcpy(p, path);
    }
    it->selectable = selectable;
    it->colour = colour;
    it->action = action;
    it->text = t;
    it->path = p;
    it->trainer = 0;
    it->extraFlags = 0;
    it->flags = 0;
    if (!first) {
        count++;
        first = last = it;
        return it;
    }
    last->next = it;
    it->prev = last;
    count++;
    last = it;
    return it;
}

void CMenu::MarkSortStart() { sortStart = count; }

void CMenu::BuildSortArray()
{
    cursor = first;
    sortArray = (CMenuItem **)operator new(count * sizeof(CMenuItem *));
    for (int i = 0; i < count; i++) {
        sortArray[i] = cursor;
        cursor = cursor->next;
    }
    cursor = nullptr;
}

// by text, ignoring case
void CMenu::QuickSort(CMenuItem **items, int lo, int hi)
{
    for (;;) {
        int i = lo, j = hi;
        CMenuItem *pivot = items[(lo + hi) / 2];
        do {
            while (_wcsicmp(items[i]->text, pivot->text) < 0)
                i++;
            while (_wcsicmp(items[j]->text, pivot->text) > 0)
                j--;
            if (i <= j) {
                CMenuItem *t = items[i];
                items[i++] = items[j];
                items[j--] = t;
            }
        } while (i <= j);
        if (j > lo)
            QuickSort(items, lo, j);
        if (i >= hi)
            return;
        lo = i;
    }
}

void CMenu::RelinkFromSortArray()
{
    first = sortArray[0];
    first->prev = nullptr;
    for (int i = 0; i < count; i++) {
        if (i != count - 1)
            sortArray[i]->next = sortArray[i + 1];
        if (i)
            sortArray[i]->prev = sortArray[i - 1];
    }
    last = sortArray[count - 1];
    last->next = nullptr;
    cursor = nullptr;
    operator delete(sortArray);
}

void CMenu::SortAll()
{
    if (count < 2)
        return;
    BuildSortArray();
    QuickSort(sortArray, 0, count - 1);
    RelinkFromSortArray();
}

void CMenu::SortFromStart()
{
    if (count - sortStart < 2)
        return;
    BuildSortArray();
    QuickSort(sortArray, sortStart, count - 1);
    RelinkFromSortArray();
}
