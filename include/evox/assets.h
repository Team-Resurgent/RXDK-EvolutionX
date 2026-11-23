// EvoX's built-in graphics and in-game reset code, linked from the user's own evoxdash.xbe at
// build time (tools/extract_assets.py, build.sh). Each size is 0 when the XBE was built without it.
#pragma once
#include <xtl.h>

extern "C" {
extern BYTE g_FontXpr[];       // 0x83750 in the original: XPR, the font texture and glyphs (CXBFont)
extern const DWORD g_FontXprSize;
extern BYTE g_ResourceXpr[];   // 0xCF240: XPR, 4 resources (CXBPackedResource)
extern const DWORD g_ResourceXprSize;
extern BYTE g_LogoMesh[];      // 0x33D240: XBG mesh (CXBMesh)
extern const DWORD g_LogoMeshSize;
extern BYTE g_Tsr1[];          // 0x95D68: in-game reset TSR 1 (TSR_Type 1), encoded
extern const DWORD g_Tsr1Size;
extern BYTE g_Tsr2[];          // 0x9D0E8: in-game reset TSR 2 (TSR_Type 2), encoded
extern const DWORD g_Tsr2Size;
extern BYTE g_IgrLoader[];     // 0x596473: the loader the kernel patch calls, built for 0x80000010
extern const DWORD g_IgrLoaderSize;
}

// the sample framework's resource tag: a name and its offset in the XPR's resource header
struct XBRESOURCE {
    const char *strName;
    DWORD dwOffset;
};
extern XBRESOURCE g_ResourceTags[]; // 0x94D70 the names in g_ResourceXpr
