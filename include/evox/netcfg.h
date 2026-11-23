// XNet's persisted network configuration as EvoX uses it (XNetLoadConfigParams /
// XNetSaveConfigParams, 0x6EC bytes). Only the fields EvoX writes are named. The SDK's XNet
// reads the V1 address fields; EvoX fills V1 and V2 itself.
#pragma once
#include <xtl.h>
#include <winsockx.h>

struct XNetConfigParams {
    BYTE unk000[0x14];
    DWORD v1_ip;      // +0x014
    DWORD v1_netmask; // +0x018
    DWORD v1_gateway; // +0x01C
    DWORD v1_dns1;    // +0x020
    DWORD v1_dns2;    // +0x024
    BYTE unk028[0x18];
    DWORD mode;       // +0x040 0xC: static address
    DWORD unk044;
    DWORD v2_ip;      // +0x048
    DWORD v2_netmask; // +0x04C
    DWORD v2_gateway; // +0x050
    DWORD v2_dns1;    // +0x054
    DWORD v2_dns2;    // +0x058
    BYTE unk05C[0x6EC - 0x5C];
};
static_assert(sizeof(XNetConfigParams) == 0x6EC, "XNetConfigParams");

// in the SDK's libxnet, not declared in its headers
extern "C" INT __stdcall XNetLoadConfigParams(XNetConfigParams *params);
extern "C" INT __stdcall XNetSaveConfigParams(const XNetConfigParams *params);

extern XNetConfigParams g_NetConfig; // 0x5913A0

// XNetGetEthernetLinkStatus as the XDK reported it: 0 without a link. The SDK's version can
// return speed / duplex bits without XNET_ETHERNET_LINK_ACTIVE (xemu without networking: 0xA).
DWORD EthernetLinkStatus();
