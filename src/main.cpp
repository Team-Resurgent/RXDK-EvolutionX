// Start-up placeholder until the original main (0x24220) is rebuilt: brings up Direct3D at
// 640x480 and clears the screen, so the toolchain, link and XBE header can be checked.
#include <xtl.h>

void __cdecl main()
{
    IDirect3D8 *d3d = Direct3DCreate8(D3D_SDK_VERSION);
    if (!d3d)
        return;

    D3DPRESENT_PARAMETERS pp = {};
    pp.BackBufferWidth = 640;
    pp.BackBufferHeight = 480;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;

    IDirect3DDevice8 *dev = nullptr;
    if (FAILED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, nullptr, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev)))
        return;

    for (;;) {
        dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0x10, 0x30, 0x60), 1.0f, 0);
        dev->Present(nullptr, nullptr, nullptr, nullptr);
    }
}
