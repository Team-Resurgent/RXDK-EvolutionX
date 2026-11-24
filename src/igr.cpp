// IgrInstall (0x23BA0 in the original, with 0x23AF0, 0x23B70 and the loader set-up at 0x596420):
// the in-game reset. Before the quick reboot into a title, the kernel's call to the XBE entry
// point is redirected to a loader at 0x80000010 (kept over the reboot), which loads
// E:\TDATA\EVOX\evox.ell into the title: the TSR that watches the pad for the reset combination.
// The loader and both TSRs come from the user's evoxdash.xbe (include/evox/assets.h).
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "evox/assets.h"
#include "evox/globals.h"
#include "evox/system.h"

#define KERNEL_BASE ((BYTE *)0x80010000)
#define LOADER_ADDRESS ((BYTE *)0x80000010)

// 0xCE9E8: the kernel's "call [XeImageHeader.AddressOfEntryPoint]" (call dword ptr [0x10128])
static const BYTE s_EntryCall[6] = {0xFF, 0x15, 0x28, 0x01, 0x01, 0x00};

// where the loader keeps what IgrInstall fills in (0x5964FF.. in the original's text section)
enum {
    LOADER_GAME_REGION = 0x8C,   // 0x5964FF g_GameRegion (set by main)
    LOADER_IGR = 0x8D,        // 0x596500 g_LoaderIgr (set by main)
    LOADER_IMPORTS = 0x8E,       // 0x596501 kernel addresses, "EvoxEvox..." until filled
};
// 0x596420: the kernel exports the loader calls, in its order
static const int s_LoaderImports[8] = {
    182, // MmSetAddressProtect
    164, // LaunchDataPage
    184, // NtAllocateVirtualMemory
    199, // NtFreeVirtualMemory
    202, // NtOpenFile
    219, // NtReadFile
    187, // NtClose
    8,   // DbgPrint
};

// an export's address from the kernel's export table (the original copies its own import thunks,
// which the XBE loader filled the same way)
static DWORD KernelExport(int ordinal)
{
    const BYTE *nt = KERNEL_BASE + *(const DWORD *)(KERNEL_BASE + 0x3C);
    const BYTE *exports = KERNEL_BASE + *(const DWORD *)(nt + 0x78);
    DWORD base = *(const DWORD *)(exports + 0x10);
    const DWORD *functions = (const DWORD *)(KERNEL_BASE + *(const DWORD *)(exports + 0x1C));
    return (DWORD)(KERNEL_BASE + functions[ordinal - base]);
}

// 0x23AF0: undo the TSR's encoding: each byte xor the low byte of k1 plus the low byte of k2,
// then k1 = ror(k1, 8) + (k2 = rol(k2, 8))
static void Decode(BYTE *p, int len)
{
    DWORD k1 = 0x45766F58, k2 = 0x446F6321; // "XovE", "!coD"
    for (int i = 0; i < len; i++) {
        p[i] = (BYTE)((p[i] ^ (BYTE)k1) + (BYTE)k2);
        k2 = (k2 << 8) | (k2 >> 24);
        k1 = ((k1 >> 8) | (k1 << 24)) + k2;
    }
}

// 0x23B70: the two parts after the 16-byte header (their sizes at +0 and +4)
static void DecodeTsr(BYTE *ell)
{
    DWORD part0 = ((DWORD *)ell)[0], part1 = ((DWORD *)ell)[1];
    Decode(ell + 0x10, part0);
    Decode(ell + 0x10 + part0, part1);
}

void IgrInstall()
{
    if (g_IgrLoaderSize == 0)
        return; // built without evoxdash.xbe: nothing to install
    DWORD at;
    for (at = 0; at < 0x25F00; at++)
        if (memcmp(KERNEL_BASE + at, s_EntryCall, sizeof s_EntryCall) == 0)
            break;
    if (at == 0x25F00)
        return; // not a kernel this knows

    // the loader into the page at 0x80000000, and the kernel's entry call through it
    BYTE *call = KERNEL_BASE + at;
    ULONG protect = MmQueryAddressProtect(call);
    MmSetAddressProtect(call, 0x28, PAGE_READWRITE);
    *(DWORD *)(call + 2) = (DWORD)LOADER_ADDRESS; // call dword ptr [0x80000010]: the loader's entry
    MmSetAddressProtect(call, 0x28, protect);
    MmSetAddressProtect((PVOID)0x80000000, 0x1000, PAGE_EXECUTE_READWRITE);
    memset((void *)0x80000000, 0, 0x400);
    memcpy(LOADER_ADDRESS, g_IgrLoader, g_IgrLoaderSize);
    LOADER_ADDRESS[LOADER_GAME_REGION] = g_GameRegion;
    LOADER_ADDRESS[LOADER_IGR] = g_LoaderIgr;
    for (int i = 0; i < 8; i++)
        ((DWORD *)(LOADER_ADDRESS + LOADER_IMPORTS))[i] = KernelExport(s_LoaderImports[i]);

    // the TSR: e:\debug.ell as it is, else the built-in one for [Misc] TSR_Type, decoded
    CreateDirectory("E:\\TDATA\\EvoX", nullptr);
    BYTE *ell;
    long size;
    if (FILE *f = fopen("e:\\debug.ell", "rb")) {
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fseek(f, 0, SEEK_SET);
        ell = (BYTE *)malloc(size);
        fread(ell, 1, size, f);
        fclose(f);
    } else {
        const BYTE *tsr;
        if (g_TsrType == 1) {
            tsr = g_Tsr1;
            size = g_Tsr1Size;
        } else if (g_TsrType == 2) {
            tsr = g_Tsr2;
            size = g_Tsr2Size;
        } else {
            return; // (the original goes on with an uninitialised buffer and size)
        }
        if (size == 0)
            return; // built without evoxdash.xbe
        ell = (BYTE *)malloc(size);
        memcpy(ell, tsr, size);
        DecodeTsr(ell);
    }
    if (FILE *f = fopen("E:\\TDATA\\EVOX\\evox.ell", "wb")) {
        fwrite(ell, 1, size, f);
        fclose(f);
    }
    free(ell);
}
