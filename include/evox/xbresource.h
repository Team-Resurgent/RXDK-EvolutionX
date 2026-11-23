// The sample framework's packed-resource loader (0x26110..0x26280 in the original), changed by
// EvoX to load an XPR from memory instead of a file.
#pragma once
#include <xtl.h>
#include "evox/assets.h"

class CXBPackedResource { // 0x10 bytes
public:
    CXBPackedResource();  // 0x26110
    ~CXBPackedResource(); // 0x26280
    // xpr: an XPR0 image in memory; registers its first count resources
    HRESULT Create(IDirect3DDevice8 *dev, const BYTE *xpr, DWORD count, XBRESOURCE *tags); // 0x26180
    void Destroy();                    // 0x26250
    BYTE *GetData(const char *name);   // 0x26120 by tag name, NULL if unknown

    BYTE *m_pSysMemData;               // +0x00 resource headers (heap)
    BYTE *m_pVidMemData;               // +0x04 resource data (contiguous memory)
    DWORD m_dwNumResources;            // +0x08
    XBRESOURCE *m_pResourceTags;       // +0x0C
};

// bytes of a registered resource header (0x260C0)
DWORD XBResource_SizeOf(D3DResource *res);
