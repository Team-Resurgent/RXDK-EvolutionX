// The dashboard's drawing helpers and the draw passes rebuilt so far (original addresses per
// function). States are set through the SDK; the original wrote several of them straight into
// D3D's state tables (D3D__RenderState / D3D__TextureState), with the same values.
#include <xtl.h>
#include <d3dx8.h>
#include "evox/dash.h"
#include "evox/skinvars.h"
#include "evox/xbutil.h"

#define F32(bits) __builtin_bit_cast(float, (unsigned)(bits))

// 0x17450: a pre-transformed, textured quad (x, y, z, rhw, u, v) with the texture's colour and
// alpha * 255 as the texture factor's alpha
void CEvoxDash::DrawQuad(IDirect3DTexture8 *tex, float alpha, void *verts)
{
    D3DDevice_SetTexture(0, (D3DBaseTexture *)tex);
    D3DDevice_SetPixelShader(0);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    D3DDevice_SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    D3DDevice_SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
    D3DDevice_SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    D3DDevice_SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
    D3DDevice_SetRenderState(D3DRS_TEXTUREFACTOR, (DWORD)(long long)(alpha * F32(0x437F0000)) << 24); // * 255
    D3DDevice_SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    D3DDevice_SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    D3DDevice_SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    D3DDevice_SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    D3DDevice_SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    D3DDevice_SetRenderState(D3DRS_FOGENABLE, FALSE);
    D3DDevice_SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
    D3DDevice_SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX1);
    D3DDevice_DrawVerticesUP(D3DPT_QUADLIST, 4, verts, 0x18);
}

// 0x18140: a rectangle from (left, top) to (right, bottom), filled with fill and outlined with
// border (both ARGB, through the texture factor)
void CEvoxDash::DrawRect(float left, float top, float right, float bottom, DWORD fill, DWORD border)
{
    struct { float x, y, z, rhw; } v[4] = {
        {left - F32(0x3F000000), top - F32(0x3F000000), 0, 0},
        {right - F32(0x3F000000), top - F32(0x3F000000), 0, 0},
        {right - F32(0x3F000000), bottom - F32(0x3F000000), 0, 0},
        {left - F32(0x3F000000), bottom - F32(0x3F000000), 0, 0},
    };
    D3DDevice_SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
    D3DDevice_SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    D3DDevice_SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
    D3DDevice_SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    D3DDevice_SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    D3DDevice_SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    D3DDevice_SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    D3DDevice_SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    D3DDevice_SetVertexShader(D3DFVF_XYZRHW);
    D3DDevice_SetRenderState(D3DRS_TEXTUREFACTOR, fill);
    D3DDevice_DrawVerticesUP(D3DPT_QUADLIST, 4, v, sizeof v[0]);
    D3DDevice_SetRenderState(D3DRS_TEXTUREFACTOR, border);
    D3DDevice_DrawVerticesUP(D3DPT_LINELOOP, 4, v, sizeof v[0]);
}

// 0x18010: the skin's background picture, if it has one
void CEvoxDash::DrawBackground()
{
    if (!m_bgTexture)
        return;
    D3DDevice_SetTexture(0, (D3DBaseTexture *)m_bgTexture);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    D3DDevice_SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    D3DDevice_SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    D3DDevice_SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    D3DDevice_SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    D3DDevice_SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
    D3DDevice_SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    D3DDevice_SetRenderState(D3DRS_FOGENABLE, FALSE);
    D3DDevice_SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
    D3DDevice_SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    D3DDevice_SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
    D3DDevice_SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    D3DDevice_SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    D3DDevice_SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    D3DDevice_SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX1);
    D3DDevice_SetStreamSource(0, (D3DVertexBuffer *)m_bgVB, 0x18);
    D3DDevice_DrawVertices(D3DPT_QUADLIST, 0, 4);
}

// [Skin] LogoType: where the logo turns (0 centre, 1..4 the corners)
static const D3DVECTOR s_LogoPositions[5] = { // 0x94B5C
    {0, 0, 0},
    {F32(0xC3160000), F32(0x42C80000), F32(0x43960000)},   // (-150,  100, 300)
    {F32(0x43160000), F32(0x42C80000), F32(0x43960000)},   // ( 150,  100, 300)
    {F32(0xC3160000), F32(0xC2C80000), F32(0x43960000)},   // (-150, -100, 300)
    {F32(0x43160000), F32(0xC2C80000), F32(0x43960000)},   // ( 150, -100, 300)
};

// 0x17CD0: the turning chrome logo: the XBG mesh, additively blended, with xray2.bmp mapped by
// the camera-space normals
void CEvoxDash::DrawLogo()
{
    if (!m_drawLogo)
        return;
    D3DDevice_SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
    D3DDevice_SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
    D3DDevice_SetTextureStageState(1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
    D3DDevice_SetTextureStageState(1, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
    D3DDevice_SetRenderState(D3DRS_DITHERENABLE, TRUE);
    D3DDevice_SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    D3DDevice_SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    D3DDevice_SetRenderState(D3DRS_AMBIENT, 0xFF0000FF);
    D3DDevice_SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    D3DDevice_SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
    D3DDevice_SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
    D3DDevice_SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    D3DDevice_SetRenderState(D3DRS_AMBIENT, 0x80);
    D3DDevice_SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    D3DDevice_SetRenderState(D3DRS_LIGHTING, TRUE);
    D3DMATERIAL8 mtrl;
    XBUtil_InitMaterial(mtrl, F32(0x3F800000), F32(0x3F800000), F32(0x3F800000), F32(0x3F800000));
    D3DDevice_SetMaterial(&mtrl);
    D3DDevice_SetRenderState(D3DRS_LIGHTING, FALSE);
    D3DDevice_SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    D3DDevice_SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    D3DDevice_SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    D3DDevice_SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCCOLOR);
    D3DDevice_SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
    D3DDevice_SetRenderState(D3DRS_TEXTUREFACTOR, 0xFF506070);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);

    // tilted, turning with the app time, at the skin's position
    D3DXMATRIX world, tilt, move;
    D3DXMatrixRotationX(&tilt, F32(0x40AFEDE0));                   // 5.498 (7/4 pi)
    D3DXMatrixRotationY(&world, m_fAppTime * F32(0x3E4CCCCD));     // 0.2 rad/s
    D3DXMatrixMultiply(&world, &world, &tilt);
    const D3DVECTOR &p = s_LogoPositions[m_logoType];
    D3DXMatrixTranslation(&move, p.x, p.y, p.z);
    D3DXMatrixMultiply(&world, &world, &move);
    D3DDevice_SetTransform(D3DTS_WORLD, &world);

    // xray2.bmp (offset 0x3C in the resources), sphere-mapped: camera-space normals scaled by
    // (0.5, -0.5) and moved to (0.5, 0.5). The original leaves the matrix's other entries
    // uninitialised; with two texture coordinates only these are used.
    D3DDevice_SetTexture(0, (D3DBaseTexture *)(m_xpr.m_pSysMemData + 0x3C));
    D3DXMATRIX tex = {};
    tex._11 = F32(0x3F000000);
    tex._22 = F32(0xBF000000);
    tex._41 = F32(0x3F000000);
    tex._42 = F32(0x3F000000);
    D3DDevice_SetTransform(D3DTS_TEXTURE0, &tex);
    D3DDevice_SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
    D3DDevice_SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACENORMAL);
    m_mesh->Render(m_pd3dDevice, XBMESH_NOMATERIALS | XBMESH_NOTEXTURES);

    D3DDevice_SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    D3DDevice_SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
    D3DDevice_SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    D3DDevice_SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    D3DDevice_SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
}

// 0x1CA50: the skin's text labels, with <variables> expanded, faded by m_labelAlpha
void CEvoxDash::DrawLabels()
{
    m_font.Begin();
    for (SkinLabel *l = m_labels; l; l = l->next) {
        const WCHAR *text = ExpandSkinVars(l->text);
        DWORD alpha = (DWORD)(long long)((float)m_labelAlpha * l->alpha);
        m_font.DrawText(l->x, l->y, alpha << 24 | l->rgb, text, l->flags);
    }
    m_font.End();
}
