// The sample framework's xbutil.cpp helpers EvoX uses (0x26A40 in the original).
#include <string.h>
#include "evox/xbutil.h"

void XBUtil_InitMaterial(D3DMATERIAL8 &mtrl, FLOAT r, FLOAT g, FLOAT b, FLOAT a)
{
    memset(&mtrl, 0, sizeof mtrl);
    mtrl.Diffuse.r = mtrl.Ambient.r = r;
    mtrl.Diffuse.g = mtrl.Ambient.g = g;
    mtrl.Diffuse.b = mtrl.Ambient.b = b;
    mtrl.Diffuse.a = mtrl.Ambient.a = a;
}
