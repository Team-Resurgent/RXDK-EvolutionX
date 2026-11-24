// Trainers (0x195E0, 0x1B130, 0x1B380 in the original): q:\trainers\*.etm files, read whole by
// ScanTrainers (src/dash_menus.cpp) into a TrainerFile (include/evox/dash.h). Picking one in the
// trainer menu (action 34) opens the settings screen with its options (OpenTrainerOptions);
// "Save And Exit" writes the choices back into the .etm (SaveTrainerOptions); on the way out
// (Destroy) the enabled trainers are packed into E:\TDATA\EVOX\Evox.etf (SaveTrainerFile) for the
// in-game TSR.
//
// The .etm data as EvoX reads it (offsets are from the start of the data):
//   +0x00 DWORD  size of the data, the bytes written back
//   +0x0A DWORD  offset of the options' on/off bytes, one per option
//   +0x0E DWORD  offset of the string table, DWORD offsets of zero-terminated strings:
//                [0] the trainer's name, [1] information lines (each zero-terminated, an empty
//                one ends them), [2..] the option names, then 0
//   +0x12 DWORD  offset of the title ids the trainer is for, 0-terminated
//   +0x22 BYTE   the trainer is on
// (The rest, the trainer code itself, is the TSR's business.)
//
// Evox.etf: (title id, file offset of that trainer's data) DWORD pairs for every title id of every
// enabled trainer, then 0, 0; then the enabled trainers' data, one after another.
#pragma once
#include <xtl.h>

extern WCHAR g_TrainerTitle[0x100];        // 0x3D85B8 the trainer's name: the screen's title
extern WCHAR g_TrainerLines[0x100][0x100]; // 0x3D87B8 option names, then the information lines
extern char g_TrainerEnable[0x10];         // 0x479E48 "Yes" / "No": the trainer
extern char g_TrainerValues[200][0x10];    // 0x479E58 "Yes" / "No": each option
