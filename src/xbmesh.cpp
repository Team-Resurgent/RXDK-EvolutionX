// The XBG mesh, 0x25BB0..0x260BB in the original. See include/evox/xbmesh.h.
#include <string.h>
#include "evox/xbmesh.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof" // (layout checks on purpose)
static_assert(sizeof(XBMESH_SUBSET) == 0x98, "XBMESH_SUBSET");
static_assert(offsetof(XBMESH_SUBSET, dwIndexStart) == 0x90, "dwIndexStart");
static_assert(sizeof(XBMESH_DATA) == 0x34, "XBMESH_DATA");
static_assert(offsetof(XBMESH_FRAME, m_MeshData) == 0x40, "m_MeshData");
static_assert(offsetof(XBMESH_FRAME, m_pChild) == 0xB4, "m_pChild");
static_assert(sizeof(XBMESH_FRAME) == 0xC0, "XBMESH_FRAME");
static_assert(sizeof(CXBMesh) == 0x18, "CXBMesh");

#define F32(bits) __builtin_bit_cast(float, (unsigned)(bits))

struct XBG_HEADER {
    DWORD dwMagic;     // 'XBG\2'
    DWORD dwNumFrames;
    DWORD dwSysMemSize;
    DWORD dwVidMemSize;
};

CXBMesh::CXBMesh()
{
    m_pSysMemData = nullptr;
    m_pVidMemData = nullptr;
    m_pMeshFrames = nullptr;
    m_dwNumFrames = 0;
    m_dwRefCount = 1;
}

CXBMesh::~CXBMesh()
{
    for (DWORD f = 0; f < m_dwNumFrames; f++) {
        XBMESH_DATA &m = m_pMeshFrames[f].m_MeshData;
        for (DWORD s = 0; s < m.m_dwNumSubsets; s++)
            if (m.m_pSubsets[s].pTexture) {
                D3DResource_Release((D3DResource *)m.m_pSubsets[s].pTexture);
                m.m_pSubsets[s].pTexture = nullptr;
            }
    }
    if (m_pSysMemData)
        delete[] m_pSysMemData;
    if (m_pVidMemData)
        D3D_FreeContiguousMemory(m_pVidMemData);
}

HRESULT CXBMesh::Create(IDirect3DDevice8 *, const BYTE *xbg, CXBPackedResource *res)
{
    const XBG_HEADER *h = (const XBG_HEADER *)xbg;
    if (h->dwMagic != 0x02474258) // 'XBG\2'
        return E_FAIL;
    DWORD sysSize = h->dwSysMemSize, vidSize = h->dwVidMemSize;
    // (0x100 bytes of zeroed slack after each copy, as in the original)
    m_pSysMemData = new BYTE[sysSize + 0x100];
    memset(m_pSysMemData, 0, sysSize + 0x100);
    memcpy(m_pSysMemData, xbg + sizeof(XBG_HEADER), sysSize);
    m_pVidMemData = (BYTE *)D3D_AllocContiguousMemory(vidSize + 0x100, 1);
    memset(m_pVidMemData, 0, vidSize + 0x100);
    memcpy(m_pVidMemData, xbg + sizeof(XBG_HEADER) + sysSize, vidSize);
    m_pMeshFrames = (XBMESH_FRAME *)m_pSysMemData;
    m_dwNumFrames = h->dwNumFrames;

    // pointers in the file are offsets from its start (the header included)
    BYTE *base = m_pSysMemData - sizeof(XBG_HEADER);
    for (DWORD f = 0; f < m_dwNumFrames; f++) {
        XBMESH_FRAME &fr = m_pMeshFrames[f];
        XBMESH_DATA &m = fr.m_MeshData;
        if (fr.m_pChild)
            fr.m_pChild = (XBMESH_FRAME *)(base + (DWORD)fr.m_pChild);
        if (fr.m_pNext)
            fr.m_pNext = (XBMESH_FRAME *)(base + (DWORD)fr.m_pNext);
        if (m.m_pSubsets)
            m.m_pSubsets = (XBMESH_SUBSET *)(base + (DWORD)m.m_pSubsets);
        if (m.m_dwNumIndices)
            m.m_IB.Data += (DWORD)base; // the indices are in system memory
        if (m.m_dwNumVertices)
            D3DResource_Register((D3DResource *)&m.m_VB, m_pVidMemData);
    }
    if (res)
        for (DWORD f = 0; f < m_dwNumFrames; f++) {
            XBMESH_DATA &m = m_pMeshFrames[f].m_MeshData;
            for (DWORD s = 0; s < m.m_dwNumSubsets; s++)
                m.m_pSubsets[s].pTexture = (IDirect3DTexture8 *)res->GetData(m.m_pSubsets[s].strTexture);
        }
    return S_OK;
}

HRESULT CXBMesh::Render(IDirect3DDevice8 *dev, DWORD flags)
{
    if (m_pMeshFrames)
        RenderFrame(dev, m_pMeshFrames, flags);
    return S_OK;
}

HRESULT CXBMesh::RenderFrame(IDirect3DDevice8 *dev, XBMESH_FRAME *frame, DWORD flags)
{
    D3DXMATRIX saved, world;
    D3DDevice_GetTransform(D3DTS_WORLD, &saved);
    D3DXMatrixMultiply(&world, &frame->m_matTransform, &saved);
    D3DDevice_SetTransform(D3DTS_WORLD, &world);
    if (frame->m_MeshData.m_dwNumSubsets)
        RenderMesh(dev, &frame->m_MeshData, flags);
    if (frame->m_pChild)
        RenderFrame(dev, frame->m_pChild, flags);
    D3DDevice_SetTransform(D3DTS_WORLD, &saved);
    if (frame->m_pNext)
        RenderFrame(dev, frame->m_pNext, flags);
    return S_OK;
}

HRESULT CXBMesh::RenderMesh(IDirect3DDevice8 *dev, XBMESH_DATA *mesh, DWORD flags)
{
    if (!mesh->m_dwNumVertices)
        return S_OK;
    D3DDevice_SetStreamSource(0, &mesh->m_VB, mesh->m_dwVertexSize);
    D3DDevice_SetIndices(&mesh->m_IB, 0);
    if (!(flags & XBMESH_NOFVF))
        D3DDevice_SetVertexShader(mesh->m_dwFVF);
    for (DWORD i = 0; i < mesh->m_dwNumSubsets; i++) {
        XBMESH_SUBSET &s = mesh->m_pSubsets[i];
        // opaque subsets (alpha 1), then transparent ones. As built here, XBMESH_NOMATERIALS only
        // forces the opaque pass (the XDK source forces both).
        bool render = !(flags & XBMESH_ALPHAONLY) &&
                      ((flags & XBMESH_NOMATERIALS) || !(s.mtrl.Diffuse.a < F32(0x3F800000)));
        if (!(flags & XBMESH_OPAQUEONLY) && !(flags & XBMESH_NOMATERIALS) && s.mtrl.Diffuse.a < F32(0x3F800000))
            render = true;
        if (!render)
            continue;
        if (!(flags & XBMESH_NOMATERIALS))
            D3DDevice_SetMaterial(&s.mtrl);
        if (!(flags & XBMESH_NOTEXTURES))
            D3DDevice_SetTexture(0, (D3DBaseTexture *)s.pTexture);
        if (!RenderCallback(dev, i, &s, flags))
            continue;
        DWORD prims = mesh->m_dwPrimType == D3DPT_TRIANGLESTRIP ? s.dwIndexCount - 2 : s.dwIndexCount / 3;
        D3DDevice_DrawIndexedVertices(mesh->m_dwPrimType, D3DVERTEXCOUNT(mesh->m_dwPrimType, prims),
                                      D3D__IndexData + s.dwIndexStart);
    }
    return S_OK;
}

BOOL CXBMesh::RenderCallback(IDirect3DDevice8 *, DWORD, XBMESH_SUBSET *, DWORD) { return TRUE; }
