// The packed-resource loader, 0x26110..0x26280 in the original. See include/evox/xbresource.h.
#include <string.h>
#include "evox/xbresource.h"

static_assert(sizeof(CXBPackedResource) == 0x10, "CXBPackedResource");

struct XPR_HEADER {
    DWORD dwMagic;      // 'XPR0'
    DWORD dwTotalSize;
    DWORD dwHeaderSize; // including this header
};

DWORD XBResource_SizeOf(D3DResource *res)
{
    switch (D3DResource_GetType(res)) {
    case D3DRTYPE_TEXTURE:
    case D3DRTYPE_VOLUMETEXTURE:
    case D3DRTYPE_CUBETEXTURE:
        return 0x14; // sizeof(D3DTexture)
    case D3DRTYPE_VERTEXBUFFER:
    case D3DRTYPE_INDEXBUFFER:
    case D3DRTYPE_PALETTE:
        return 0x0C;
    default:
        return 0;
    }
}

CXBPackedResource::CXBPackedResource()
{
    m_pSysMemData = nullptr;
    m_pVidMemData = nullptr;
    m_dwNumResources = 0;
    m_pResourceTags = nullptr;
}

CXBPackedResource::~CXBPackedResource() { Destroy(); }

HRESULT CXBPackedResource::Create(IDirect3DDevice8 *, const BYTE *xpr, DWORD count, XBRESOURCE *tags)
{
    const XPR_HEADER *h = (const XPR_HEADER *)xpr;
    if (h->dwMagic != 0x30525058) // 'XPR0'
        return E_INVALIDARG;
    DWORD sysSize = h->dwHeaderSize - sizeof(XPR_HEADER);
    DWORD vidSize = h->dwTotalSize - h->dwHeaderSize;
    m_pSysMemData = new BYTE[sysSize];
    m_pVidMemData = (BYTE *)D3D_AllocContiguousMemory(vidSize, D3DTEXTURE_ALIGNMENT);
    memcpy(m_pSysMemData, xpr + sizeof(XPR_HEADER), sysSize);
    memcpy(m_pVidMemData, xpr + h->dwHeaderSize, vidSize);

    // register the resources; user data (a negative type word) is skipped by its size
    BYTE *p = m_pSysMemData;
    for (DWORD i = 0; i < count; i++) {
        if ((int)((DWORD *)p)[0] < 0) {
            p += ((DWORD *)p)[1] + 8;
        } else {
            D3DResource_Register((D3DResource *)p, m_pVidMemData);
            p += XBResource_SizeOf((D3DResource *)p);
        }
    }
    m_dwNumResources = count;
    m_pResourceTags = tags;
    return S_OK;
}

void CXBPackedResource::Destroy()
{
    if (m_pSysMemData) {
        delete[] m_pSysMemData;
        m_pSysMemData = nullptr;
    }
    if (m_pVidMemData) {
        D3D_FreeContiguousMemory(m_pVidMemData);
        m_pVidMemData = nullptr;
    }
    m_dwNumResources = 0;
    m_pResourceTags = nullptr;
}

// NOTE: as in the original, the search runs over m_dwNumResources entries, which can be more
// than the tag table holds (EvoX passes 4 resources with 2 tags), so an unknown name reaches the
// table's NULL terminator.
BYTE *CXBPackedResource::GetData(const char *name)
{
    if (!m_pResourceTags || !name)
        return nullptr;
    for (DWORD i = 0; i < m_dwNumResources; i++)
        if (_stricmp(name, m_pResourceTags[i].strName) == 0)
            return m_pSysMemData + m_pResourceTags[i].dwOffset;
    return nullptr;
}
