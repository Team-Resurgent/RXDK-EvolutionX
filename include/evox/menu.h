// EvoX's menus (0x24E30..0x251FF in the original): a doubly linked list of items with a
// cursor, a selection and a smooth scroll position; items can be sorted by text.
#pragma once
#include <xtl.h>

struct CMenu;

struct CMenuItem {           // 0x2C bytes
    DWORD colour;            // +0x00 1..3: [skin] MenuColor1..3
    BYTE selectable;         // +0x04 1: can be selected
    WCHAR *text;             // +0x08
    int action;              // +0x0C the dashboard action (Run), -1 ends the menu
    char *path;              // +0x10 XBE, skin or file for the action
    CMenuItem *prev;         // +0x14
    CMenuItem *next;         // +0x18
    CMenu *submenu;          // +0x1C action 100
    DWORD extraFlags;        // +0x20 the keywords' first flag word: none sets it, so always 0
    DWORD flags;             // +0x24 0x100, 0x200 trainer, 0x400/0x800 IGR, 0x3000 region; LED bits
    char *trainer;           // +0x28 trainer path (action 8, flag 0x200)
};

struct CMenu {               // 0x24 bytes
    CMenuItem *cursor;       // +0x00 GetItem's position
    CMenuItem *first;        // +0x04
    CMenuItem *last;         // +0x08
    int cursorIndex;         // +0x0C
    CMenuItem **sortArray;   // +0x10
    int sortStart;           // +0x14 SortFromStart sorts the items from here on
    int count;               // +0x18
    int selected;            // +0x1C
    int scroll;              // +0x20 pixels (20 per item), -100 = above the first

    CMenu();                                                                 // 0x24E30
    void ResetSelection();                                                   // 0x24E50
    CMenuItem *GetItem(int index);                                           // 0x24E90
    CMenuItem *AddItem(BYTE selectable, DWORD colour, int action, const WCHAR *text, const char *path); // 0x24EF0
    void MarkSortStart();                                                    // 0x250B0
    void SortAll();                                                          // 0x25190
    void SortFromStart();                                                    // 0x251C0

private:
    void BuildSortArray();                                                   // 0x250C0
    void QuickSort(CMenuItem **items, int lo, int hi);                       // 0x24FE0
    void RelinkFromSortArray();                                              // 0x25110
};

extern CMenuItem g_EmptyMenuItem; // 0xCF208 returned for an index past the end
