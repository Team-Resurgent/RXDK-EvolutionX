// See include/evox/netcfg.h.
#include "evox/netcfg.h"

XNetConfigParams g_NetConfig;

DWORD EthernetLinkStatus()
{
    DWORD s = XNetGetEthernetLinkStatus();
    return (s & XNET_ETHERNET_LINK_ACTIVE) ? s : 0;
}
