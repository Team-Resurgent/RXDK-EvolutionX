// The bitmap font, 0x25200..0x25B9E in the original. See include/evox/xbfont.h.
#include <math.h>
#include <string.h>
#include "evox/xbfont.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof" // (layout checks on purpose)
static_assert(sizeof(GLYPH_ATTR) == 0x18, "GLYPH_ATTR");
static_assert(offsetof(CXBFont, m_xprResource) == 0x18, "m_xprResource");
static_assert(offsetof(CXBFont, m_pVBs) == 0x30, "m_pVBs");
static_assert(sizeof(CXBFont) == 0x50, "CXBFont");

#define F32(bits) __builtin_bit_cast(float, (unsigned)(bits))

static const DWORD VB_SIZE = 0x1C000;   // 0x1000 vertices
static const DWORD MAX_VERTICES = 0x1000;

struct FONT_VERTEX { // 0x1C bytes: D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1
    float x, y, z, rhw;
    DWORD colour;
    float tu, tv;
};
static_assert(sizeof(FONT_VERTEX) == 0x1C, "FONT_VERTEX");

// the font resource: a texture, then user data (8-byte header) with the glyph table
struct FONT_FILE {
    DWORD dwVersion;   // 4
    DWORD dwFontHeight;
    DWORD dwTexWidth;
    DWORD dwTexHeight;
    DWORD dwBPP;
    WCHAR cLowChar, cHighChar;
    DWORD dwNumGlyphs;
    GLYPH_ATTR Glyphs[1];
};

CXBFont::CXBFont()
{
    m_pd3dDevice = nullptr;
    m_pFontTexture = nullptr;
    m_pVBs[0] = m_pVBs[1] = nullptr;
    m_pVB = nullptr;
    m_dwCurrentVB = 0;
    m_bOwnsTexture = FALSE;
    m_pVertices = nullptr;
    m_dwNumQuads = 0;
    m_dwNestedBeginCount = 0;
    m_cLowChar = 0;
    m_cHighChar = 0;
    m_dwNumGlyphs = 0;
    m_Glyphs = nullptr;
    m_dwFontHeight = 0x24;
    m_dwTexWidth = 0x40;
    m_dwTexHeight = 0x40;
}

CXBFont::~CXBFont() { Destroy(); }

HRESULT CXBFont::Create(IDirect3DDevice8 *dev, DWORD)
{
    m_pd3dDevice = dev;
    if (m_xprResource.Create(dev, g_FontXpr, 2, nullptr) < 0)
        return E_FAIL;
    m_pFontTexture = (IDirect3DTexture8 *)m_xprResource.m_pSysMemData;
    const FONT_FILE *f =
        (const FONT_FILE *)(m_xprResource.m_pSysMemData + XBResource_SizeOf((D3DResource *)m_pFontTexture) + 8);
    m_dwFontHeight = f->dwFontHeight;
    m_dwTexWidth = f->dwTexWidth;
    m_dwTexHeight = f->dwTexHeight;
    if (f->dwVersion != 4)
        return E_FAIL;
    m_cLowChar = f->cLowChar;
    m_cHighChar = f->cHighChar;
    m_Glyphs = (GLYPH_ATTR *)f->Glyphs;
    for (int i = 0; i < 2; i++)
        if (!(m_pVBs[i] = D3DDevice_CreateVertexBuffer2(VB_SIZE)))
            return E_OUTOFMEMORY;
    m_dwCurrentVB = 0;
    m_pVB = m_pVBs[0];
    return S_OK;
}

HRESULT CXBFont::Destroy()
{
    m_xprResource.Destroy();
    for (int i = 0; i < 2; i++)
        if (m_pVBs[i]) {
            D3DResource_Release((D3DResource *)m_pVBs[i]);
            m_pVBs[i] = nullptr;
        }
    if (m_bOwnsTexture) {
        if (m_pFontTexture) {
            D3DResource_Release((D3DResource *)m_pFontTexture);
            m_pFontTexture = nullptr;
        }
        delete[] (BYTE *)m_Glyphs;
    }
    m_pd3dDevice = nullptr;
    m_pFontTexture = nullptr;
    m_pVB = nullptr;
    m_dwCurrentVB = 0;
    m_bOwnsTexture = FALSE;
    m_pVertices = nullptr;
    m_dwNumQuads = 0;
    m_dwNestedBeginCount = 0;
    m_dwTexWidth = 0x40;
    m_dwTexHeight = 0x40;
    m_cLowChar = 0;
    m_cHighChar = 0;
    m_dwNumGlyphs = 0;
    m_Glyphs = nullptr;
    m_dwFontHeight = 0x24;
    return S_OK;
}

// Width and height of text. As in EvoX's build, a newline restarts the width (and ends the
// measurement with firstLineOnly), characters outside the font are not counted and '\r' is not
// special.
HRESULT CXBFont::GetTextExtent(const WCHAR *text, FLOAT *width, FLOAT *height, BOOL firstLineOnly)
{
    if (!text || !width || !height)
        return E_INVALIDARG;
    *width = 0;
    *height = 0;
    float sx = 0;
    float sy = (float)(m_dwFontHeight + 1);
    for (WCHAR c; (c = *text++);) {
        if (c == L'\n') {
            if (firstLineOnly)
                return S_OK;
            sx = 0;
            sy += (float)(m_dwFontHeight + 1);
        }
        if (c < m_cLowChar || c > m_cHighChar)
            continue;
        const GLYPH_ATTR &g = m_Glyphs[c - m_cLowChar];
        sx += g.wOffset;
        sx += g.wAdvance;
        if (sx > *width)
            *width = sx;
        if (sy > *height)
            *height = sy;
    }
    return S_OK;
}

HRESULT CXBFont::Begin()
{
    if (m_dwNestedBeginCount == 0) {
        if (D3DResource_IsBusy((D3DResource *)m_pVB)) { // the GPU still reads it: use the other one
            if (++m_dwCurrentVB >= 2)
                m_dwCurrentVB = 0;
            m_pVB = m_pVBs[m_dwCurrentVB];
        }
        m_pVertices = D3DVertexBuffer_Lock2((D3DVertexBuffer *)m_pVB, 0);
        m_dwNumQuads = 0;
    }
    m_dwNestedBeginCount++;
    return S_OK;
}

HRESULT CXBFont::End()
{
    if (m_dwNestedBeginCount == 0)
        return E_FAIL;
    if (--m_dwNestedBeginCount == 0 && m_dwNumQuads > 0)
        Flush();
    return S_OK;
}

void CXBFont::Flush()
{
    D3DDevice_SetTexture(0, (D3DBaseTexture *)m_pFontTexture);
    D3DDevice_SetStreamSource(0, (D3DVertexBuffer *)m_pVB, sizeof(FONT_VERTEX));
    D3DDevice_SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    D3DDevice_SetPixelShader(0);
    D3DDevice_SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    D3DDevice_SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    D3DDevice_SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    D3DDevice_SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    D3DDevice_SetRenderState(D3DRS_ALPHAREF, 0x08);
    D3DDevice_SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
    D3DDevice_SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    D3DDevice_SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
    D3DDevice_SetRenderState(D3DRS_ZENABLE, FALSE);
    D3DDevice_SetRenderState(D3DRS_FOGENABLE, FALSE);
    D3DDevice_SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
    D3DDevice_SetRenderState(D3DRS_STENCILENABLE, FALSE);
    D3DDevice_SetRenderState(D3DRS_EDGEANTIALIAS, FALSE);
    D3DDevice_SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    D3DDevice_SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    D3DDevice_SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    D3DDevice_SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    D3DDevice_SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    D3DDevice_SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_POINT);
    D3DDevice_SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
    D3DDevice_SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
    D3DDevice_SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    D3DDevice_SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    D3DDevice_DrawVertices(D3DPT_QUADLIST, 0, m_dwNumQuads * 4);
}

HRESULT CXBFont::DrawText(FLOAT x, FLOAT y, DWORD colour, const WCHAR *text, DWORD flags)
{
    Begin();
    float sx = x, sy = y;
    if (flags & XBFONT_CENTER_Y) {
        float w, h;
        GetTextExtent(text, &w, &h, FALSE);
        sy = floorf(y - h * F32(0x3F000000));
    }
    bool newLine = true;
    while (*text) {
        if (newLine) {
            if (flags & (XBFONT_RIGHT | XBFONT_CENTER_X)) {
                float w, h;
                GetTextExtent(text, &w, &h, TRUE);
                if (flags & XBFONT_RIGHT)
                    sx = floorf(x - w);
                if (flags & XBFONT_CENTER_X)
                    sx = floorf(x - w * F32(0x3F000000));
            }
            newLine = false;
        }
        WCHAR c = *text++;
        if (c == L'\n') {
            sx = x;
            sy += (float)m_dwFontHeight;
            newLine = true;
        }
        if (c < m_cLowChar || c > m_cHighChar)
            continue;
        const GLYPH_ATTR &g = m_Glyphs[c - m_cLowChar];

        // half a pixel aligns texel centres with pixel centres
        sx += g.wOffset;
        float sx1 = sx - F32(0x3F000000);
        float sx2 = ((float)g.wWidth + F32(0x3F800000)) + sx1;
        float sy1 = sy - F32(0x3F000000);
        float sy2 = ((float)m_dwFontHeight + F32(0x3F800000)) + sy1;
        sx += g.wAdvance;
        float tw = (float)m_dwTexWidth, th = (float)m_dwTexHeight;
        float tx1 = (float)(m_dwTexWidth - 1) * g.tu1 / tw;
        float ty1 = (float)(m_dwTexHeight - 1) * g.tv1 / th;
        float tx2 = (float)(m_dwTexWidth - 1) * g.tu2 / tw;
        float ty2 = (float)(m_dwTexHeight - 1) * g.tv2 / th;

        FONT_VERTEX *v = (FONT_VERTEX *)m_pVertices;
        v[0] = {sx1, sy2, 0, 0, colour, tx1, ty2};
        v[1] = {sx1, sy1, 0, 0, colour, tx1, ty1};
        v[2] = {sx2, sy1, 0, 0, colour, tx2, ty1};
        v[3] = {sx2, sy2, 0, 0, colour, tx2, ty2};
        m_pVertices += 4 * sizeof(FONT_VERTEX);

        // a full buffer is drawn and the same one locked again
        if (++m_dwNumQuads * 4 + 4 > MAX_VERTICES) {
            Flush();
            m_pVertices = D3DVertexBuffer_Lock2((D3DVertexBuffer *)m_pVB, 0);
            m_dwNumQuads = 0;
        }
    }
    End();
    return S_OK;
}
