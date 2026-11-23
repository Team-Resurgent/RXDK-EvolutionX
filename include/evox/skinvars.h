// Skin text variables: "<Name>" in a skin label is replaced by its current value
// (ExpandSkinVars, 0x20F50 in the original; the table is at 0x95B20). The values are filled in
// by the per-frame tasks.
#pragma once
#include <xtl.h>

struct SkinVar {
    const WCHAR *name;
    WCHAR *value;
};
extern SkinVar g_SkinVars[19];

// the expanded text, in one shared buffer (0x47C4E0)
const WCHAR *ExpandSkinVars(const WCHAR *text);

// the value buffers (original addresses in skinvars.cpp)
extern WCHAR g_VarSpaceC[0x40], g_VarSpaceE[0x40], g_VarSpaceF[0x40], g_VarSpaceG[0x40];
extern WCHAR g_VarSpaceX[0x40], g_VarSpaceY[0x40], g_VarSpaceZ[0x40];
extern WCHAR g_VarTime[0x40], g_VarIP[0x20], g_VarName[0x40], g_VarVersion[0x40];
extern WCHAR g_VarBiosVer[0x40], g_VarKernelVer[0x40], g_VarRdName[0x40], g_VarCD[0x40];
extern WCHAR g_VarTemp1[0x20], g_VarTemp2[0x20], g_VarTemp3[0x20];
