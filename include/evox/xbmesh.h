// The sample framework's XBG mesh (0x25BB0..0x260BB in the original), loading an XBG image from
// memory: EvoX's logo (g_LogoMesh). Textures are looked up by name in a packed resource.
#pragma once
#include <xtl.h>
#include <d3dx8.h>
#include "evox/xbresource.h"

#define XBMESH_OPAQUEONLY 0x00000001
#define XBMESH_ALPHAONLY 0x00000002
#define XBMESH_NOMATERIALS 0x00000010
#define XBMESH_NOTEXTURES 0x00000020
#define XBMESH_NOFVF 0x00000040

struct XBMESH_SUBSET {               // 0x98 bytes
    D3DMATERIAL8 mtrl;               // +0x00
    IDirect3DTexture8 *pTexture;     // +0x44
    CHAR strTexture[64];             // +0x48
    DWORD dwVertexStart;             // +0x88
    DWORD dwVertexCount;             // +0x8C
    DWORD dwIndexStart;              // +0x90
    DWORD dwIndexCount;              // +0x94
};

struct XBMESH_DATA {                 // 0x34 bytes
    D3DVertexBuffer m_VB;            // +0x00
    DWORD m_dwNumVertices;           // +0x0C
    D3DIndexBuffer m_IB;             // +0x10
    DWORD m_dwNumIndices;            // +0x1C
    DWORD m_dwFVF;                   // +0x20
    DWORD m_dwVertexSize;            // +0x24
    D3DPRIMITIVETYPE m_dwPrimType;   // +0x28
    DWORD m_dwNumSubsets;            // +0x2C
    XBMESH_SUBSET *m_pSubsets;       // +0x30
};

struct XBMESH_FRAME {                // 0xC0 bytes
    D3DXMATRIX m_matTransform;       // +0x00
    XBMESH_DATA m_MeshData;          // +0x40
    CHAR m_strName[64];              // +0x74
    XBMESH_FRAME *m_pChild;          // +0xB4
    XBMESH_FRAME *m_pNext;           // +0xB8
    DWORD pad;
};

class CXBMesh { // 0x18 bytes
public:
    CXBMesh();          // 0x25BB0
    virtual HRESULT RenderFrame(IDirect3DDevice8 *dev, XBMESH_FRAME *frame, DWORD flags);   // 0x25E90
    virtual HRESULT RenderMesh(IDirect3DDevice8 *dev, XBMESH_DATA *mesh, DWORD flags);      // 0x25F30
    virtual ~CXBMesh(); // 0x25BE0 (the vtable holds the compiler-made deleting destructor 0x260A0)
    virtual BOOL RenderCallback(IDirect3DDevice8 *dev, DWORD subset, XBMESH_SUBSET *s, DWORD flags); // 0x25BD0

    HRESULT Create(IDirect3DDevice8 *dev, const BYTE *xbg, CXBPackedResource *res); // 0x25C80
    HRESULT Render(IDirect3DDevice8 *dev, DWORD flags);                             // 0x25E70

    BYTE *m_pSysMemData;          // +0x04
    BYTE *m_pVidMemData;          // +0x08
    XBMESH_FRAME *m_pMeshFrames;  // +0x0C
    DWORD m_dwNumFrames;          // +0x10
    DWORD m_dwRefCount;           // +0x14
};
