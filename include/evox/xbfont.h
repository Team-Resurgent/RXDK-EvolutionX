// The sample framework's bitmap font (0x25200..0x25B9E in the original), as EvoX built it: the
// font XPR is embedded (g_FontXpr), text is batched into one of two vertex buffers between Begin
// and End, and characters outside the font are skipped.
#pragma once
#include <xtl.h>
#include "evox/xbresource.h"

#define XBFONT_RIGHT 0x00000001
#define XBFONT_CENTER_X 0x00000002
#define XBFONT_CENTER_Y 0x00000004

struct GLYPH_ATTR { // 0x18 bytes
    FLOAT tu1, tv1, tu2, tv2; // texture coordinates
    SHORT wOffset;            // +0x10 pixel offset before the glyph
    SHORT wWidth;             // +0x12
    SHORT wAdvance;           // +0x14
    SHORT pad;
};

class CXBFont { // 0x50 bytes
public:
    CXBFont();   // 0x25200
    ~CXBFont();  // 0x256B0
    HRESULT Create(IDirect3DDevice8 *dev, DWORD unused); // 0x25250 (font XPR: g_FontXpr)
    HRESULT Destroy();                                   // 0x25310
    HRESULT GetTextExtent(const WCHAR *text, FLOAT *width, FLOAT *height, BOOL firstLineOnly); // 0x253A0
    HRESULT Begin();                                     // 0x254C0
    HRESULT End();                                       // 0x25700
    HRESULT DrawText(FLOAT x, FLOAT y, DWORD colour, const WCHAR *text, DWORD flags); // 0x25730

    DWORD m_dwFontHeight;            // +0x00
    DWORD m_dwTexWidth;              // +0x04
    DWORD m_dwTexHeight;             // +0x08
    WCHAR m_cLowChar;                // +0x0C
    WCHAR m_cHighChar;               // +0x0E
    DWORD m_dwNumGlyphs;             // +0x10 (never set)
    GLYPH_ATTR *m_Glyphs;            // +0x14
    CXBPackedResource m_xprResource; // +0x18
    IDirect3DDevice8 *m_pd3dDevice;  // +0x28
    IDirect3DTexture8 *m_pFontTexture; // +0x2C
    IDirect3DVertexBuffer8 *m_pVBs[2]; // +0x30
    IDirect3DVertexBuffer8 *m_pVB;   // +0x38 the one being filled
    DWORD m_dwCurrentVB;             // +0x3C
    BYTE *m_pVertices;               // +0x40 locked vertex pointer
    DWORD m_dwNumQuads;              // +0x44
    DWORD m_dwNestedBeginCount;      // +0x48
    BOOL m_bOwnsTexture;             // +0x4C (never set: Destroy frees the texture only then)

private:
    void Flush(); // 0x25520 set state and draw the batched quads
};
