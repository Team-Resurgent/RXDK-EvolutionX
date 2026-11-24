// The menu's scripted actions (items with actions 200..299; 0x1CAC0, 0x187B0, 0x20490..0x20F42
// in the original). An item's action n - 200 names an evox.ini section:
//     [Action_07]
//     info     = "Title of the dialog"
//     Warning  = "a line of the warning"      (any number of these)
//     progress = "text while it runs"
//     LogFile  = "e:\action07.log"            (optional)
//     copy "e:\from\" "f:\to\"                 (then the steps, in order)
//     ...
// OpenItem fills the "Continue ?" dialog (CEvoxDash::DrawActionBox); Yes runs the steps on a
// thread (StartActionScript). Steps: configsector <file> (writes the file's first 512 KB to the
// start of the disk), format <drive letter>, rename <from> <to>, copy <from> <to> (a folder when
// <from> ends in '\'), flashbios <file> (the "Flash BIOS" box after the script), hddlockdisable,
// hddlockenable, hddmasterdisable. Lines starting with '#' are skipped; a path starting with '\'
// is on Q:.
#pragma once
#include <xtl.h>

extern HANDLE g_ActionThread;          // 0x57E5BC while the steps run (Run's state 14 waits)
extern bool g_ActionFlash;             // 0x57E5C0 a flashbios step ran: flash g_ActionFlashFile
extern char g_ActionFlashFile[0x100];  // 0x47CFE0

void StartActionScript();              // 0x20F20 Run's state 15
