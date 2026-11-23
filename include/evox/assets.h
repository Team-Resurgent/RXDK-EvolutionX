// EvoX's built-in graphics, linked from the user's own evoxdash.xbe at build time
// (tools/extract_assets.py, build.sh). Each size is 0 when the XBE was built without it.
#pragma once
#include <xtl.h>

extern "C" {
extern BYTE g_FontXpr[];       // 0x83750 in the original: XPR, the font texture and glyphs (CXBFont)
extern const DWORD g_FontXprSize;
extern BYTE g_ResourceXpr[];   // 0xCF240: XPR, 4 resources (CXBPackedResource)
extern const DWORD g_ResourceXprSize;
extern BYTE g_LogoMesh[];      // 0x33D240: XBG mesh (CXBMesh)
extern const DWORD g_LogoMeshSize;
}

// the sample framework's resource tag: a name and its offset in the XPR's resource header
struct XBRESOURCE {
    const char *strName;
    DWORD dwOffset;
};
extern XBRESOURCE g_ResourceTags[]; // 0x94D70 the names in g_ResourceXpr
