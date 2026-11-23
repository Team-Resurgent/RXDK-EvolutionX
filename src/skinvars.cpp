// Skin text variables. See include/evox/skinvars.h.
// The original's value buffers are separate globals whose sizes are not known yet; here each has
// its own buffer (addresses in the original noted per buffer).
#include <wchar.h>
#include "evox/skinvars.h"

WCHAR g_VarSpaceC[0x40];  // 0x47BF48
WCHAR g_VarSpaceE[0x40];  // 0x57E0B0
WCHAR g_VarSpaceF[0x40];  // 0x57D2E0
WCHAR g_VarSpaceG[0x40];  // 0x57E0F0
WCHAR g_VarSpaceX[0x40];  // 0x47C3A0
WCHAR g_VarSpaceY[0x40];  // 0x47C250
WCHAR g_VarSpaceZ[0x40];  // 0x47C210
WCHAR g_VarTime[0x40];    // 0x57DC28
WCHAR g_VarIP[0x20];      // 0x57D320
WCHAR g_VarName[0x40];    // 0x47D1E0
WCHAR g_VarVersion[0x40]; // 0x47C098
WCHAR g_VarBiosVer[0x40]; // 0x47C318
WCHAR g_VarKernelVer[0x40]; // 0x47CCE0
WCHAR g_VarRdName[0x40];  // 0x57D260
WCHAR g_VarCD[0x40];      // 0x57E530
WCHAR g_VarTemp1[0x20];   // 0x47C420
WCHAR g_VarTemp2[0x20];   // 0x47C168
WCHAR g_VarTemp3[0x20];   // 0x47CD60
static WCHAR g_VarCrLf[] = L"\n"; // 0x80410

SkinVar g_SkinVars[19] = {
    {L"SpaceC", g_VarSpaceC},   {L"SpaceE", g_VarSpaceE},   {L"SpaceF", g_VarSpaceF},
    {L"SpaceG", g_VarSpaceG},   {L"SpaceX", g_VarSpaceX},   {L"SpaceY", g_VarSpaceY},
    {L"SpaceZ", g_VarSpaceZ},   {L"Time", g_VarTime},       {L"IP", g_VarIP},
    {L"Name", g_VarName},       {L"Version", g_VarVersion}, {L"BIOSVer", g_VarBiosVer},
    {L"KernelVer", g_VarKernelVer}, {L"RDName", g_VarRdName}, {L"CD", g_VarCD},
    {L"CrLf", g_VarCrLf},       {L"Temp1", g_VarTemp1},     {L"Temp2", g_VarTemp2},
    {L"Temp3", g_VarTemp3},
};

static WCHAR g_Expanded[0x400]; // 0x47C4E0

// Copy text, replacing each "<Name>" (any case) with its value; an unknown name gives
// "Env error!", a '<' without '>' the whole result "Error in string!". (No bounds checks, as in
// the original: names are copied into a 64-character buffer.)
const WCHAR *ExpandSkinVars(const WCHAR *text)
{
    int n = 0;
    while (*text) {
        if (*text != L'<') {
            g_Expanded[n++] = *text++;
            continue;
        }
        WCHAR name[0x40];
        int i = 0;
        for (text++; *text != L'>'; text++) {
            if (!*text)
                return L"Error in string!";
            name[i++] = *text;
        }
        text++;
        name[i] = 0;
        int v = 0;
        while (v < 19 && _wcsicmp(g_SkinVars[v].name, name) != 0)
            v++;
        g_Expanded[n] = 0;
        wcscat(g_Expanded, v < 19 ? g_SkinVars[v].value : L"Env error!");
        n = wcslen(g_Expanded);
    }
    g_Expanded[n] = 0;
    return g_Expanded;
}
