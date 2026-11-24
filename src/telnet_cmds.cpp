// The Telnet server's hardware commands (0x15440..0x16B8C in the original): sector, memory,
// I/O-port, SMBus and PCI dumps, the I/O and PCI writes, SaveMem, TEST2, the drive's SMART and
// IDENTIFY data and its security (lock) commands. They do what they are asked without checks,
// as in the original: hddlockenable locks the drive, O / PCIDUMP / TEST2 write to hardware.
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "evox/system.h"
#include "evox/telnet.h"

#define ARG(i) g_TelnetArgv[i]

static BYTE InByte(WORD port)
{
    BYTE v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static void OutByte(WORD port, BYTE v) { __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port)); }
static DWORD InDword(WORD port)
{
    DWORD v;
    __asm__ volatile("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static void OutDword(WORD port, DWORD v) { __asm__ volatile("outl %0, %1" : : "a"(v), "Nd"(port)); }

// "<address> : " then 16 bytes in hex, " | " and as characters (the low 7 bits, '.' below 0x20):
// the line every dump prints (each command has its own copy in the original)
static void HexLine(char *line, DWORD address, const BYTE *bytes)
{
    int n = sprintf(line, "%08x : ", (unsigned)address);
    for (int i = 0; i < 16; i++)
        n += sprintf(line + n, "%02x ", bytes[i]);
    n += sprintf(line + n, " | ");
    for (int i = 0; i < 16; i++) {
        BYTE c = bytes[i] & 0x7F;
        n += c > 0x1F ? sprintf(line + n, "%c", c) : sprintf(line + n, ".");
    }
}

// 0x15440: CDSECTORDUMP <sector> <count>, 2048-byte sectors of the disc
void CTelnetServer::CdSectorDump()
{
    if (g_TelnetArgc != 3) {
        Printf("CdSectorDump V0.1\n\r");
        Printf("Usage : CdSectorDump <Sector> <Number Of Sectors>\n\r");
        return;
    }
    DWORD sector = strtoul(ARG(1), nullptr, 10), count = strtoul(ARG(2), nullptr, 10);
    HANDLE h = CreateFile("CdRom0:", GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return;
    DWORD size = count << 11, offset = sector << 11;
    BYTE *buf = (BYTE *)VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    LONG high = 0;
    SetFilePointer(h, offset, &high, FILE_BEGIN);
    DWORD read;
    if (ReadFile(h, buf, size, &read, nullptr)) {
        char line[0x90];
        for (DWORD pos = 0; pos < size; pos += 16) {
            HexLine(line, offset + pos, buf + pos);
            Printf("%s\r\n", line);
        }
    } else {
        Printf("Error Reading CD!\r\n");
    }
    VirtualFree(buf, 0, MEM_RELEASE);
    CloseHandle(h);
}

// 0x15BD0: HDDSECTORDUMP <sector> <count>, 512-byte sectors of the whole disk
void CTelnetServer::HddSectorDump()
{
    ANSI_STRING name = {0x1C, 0x1D, (PCHAR) "\\Device\\Harddisk0\\partition0"};
    if (g_TelnetArgc != 3) {
        Printf("HddSectorDump V0.1\n\r");
        Printf("Usage : HddSectorDump <Sector> <Number Of Sectors>\n\r");
        return;
    }
    DWORD sector = strtoul(ARG(1), nullptr, 10), count = strtoul(ARG(2), nullptr, 10);
    OBJECT_ATTRIBUTES oa = {nullptr, &name, OBJ_CASE_INSENSITIVE};
    IO_STATUS_BLOCK iosb;
    HANDLE h = INVALID_HANDLE_VALUE; // (not initialised in the original)
    NtOpenFile(&h, GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE, &oa, &iosb, FILE_SHARE_READ | FILE_SHARE_WRITE,
               FILE_SYNCHRONOUS_IO_ALERT);
    if (h == INVALID_HANDLE_VALUE)
        return;
    DWORD size = count << 9;
    BYTE *buf = (BYTE *)VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    ULONGLONG offset = (ULONGLONG)sector * 0x200;
    LONG high = (LONG)(offset >> 32);
    SetFilePointer(h, (LONG)offset, &high, FILE_BEGIN);
    DWORD read;
    if (ReadFile(h, buf, size, &read, nullptr)) {
        char line[0x90];
        for (DWORD pos = 0; pos < size; pos += 16) {
            HexLine(line, (sector << 9) + pos, buf + pos); // (the address shown is 32-bit)
            Printf("%s\r\n", line);
        }
    } else {
        Printf("Error Reading HDD!\r\n");
    }
    VirtualFree(buf, 0, MEM_RELEASE);
    CloseHandle(h);
}

// an ATA command with a 512-byte data-in block (SMART READ DATA / IDENTIFY)
static bool AtaRead(BYTE features, BYTE lbaMid, BYTE lbaHigh, BYTE command, BYTE *data)
{
    AtaRegisters r = {};
    r.features = features;
    r.lbaMid = lbaMid;
    r.lbaHigh = lbaHigh;
    r.command = command;
    r.dataSize = 0x200;
    r.data = data;
    memset(data, 0, 0x200);
    return AtaCommand(&r);
}

// 0x15640: smart, the SMART data (SMART READ DATA) dumped, then each attribute's 12 bytes
void CTelnetServer::Smart()
{
    alignas(16) BYTE data[0x200]; // (raw disk transfers need an aligned buffer)
    char line[0x90];
    if (!AtaRead(0xD0, 0x4F, 0xC2, 0xB0, data))
        return;
    for (int pos = 0; pos < 0x200; pos += 16) {
        HexLine(line, pos, data + pos);
        Printf("%s\r\n", line);
    }
    for (int pos = 2; pos < 0x160; pos += 12) {
        if (!data[pos])
            continue;
        int n = 0;
        for (int i = 0; i < 12; i++)
            n += sprintf(line + n, "%03d ", data[pos + i]);
        Printf("%s\r\n", line);
    }
}

// 0x157E0: hddinfo, the drive's IDENTIFY data in words
void CTelnetServer::HddInfo()
{
    alignas(16) BYTE data[0x200]; // (raw disk transfers need an aligned buffer)
    char line[0x90];
    if (!AtaRead(0, 0, 0, 0xEC, data))
        return;
    for (int pos = 0; pos < 0x200; pos += 16) {
        int n = sprintf(line, "%08x : ", pos >> 1);
        for (int i = 0; i < 16; i += 2)
            n += sprintf(line + n, "%04x ", *(const WORD *)(data + pos + i));
        n += sprintf(line + n, " | ");
        for (int i = 0; i < 16; i++) {
            BYTE c = data[pos + i] & 0x7F;
            n += c > 0x1F ? sprintf(line + n, "%c", c) : sprintf(line + n, ".");
        }
        Printf("%s\r\n", line);
    }
}

// the master password the lock commands use
static void TeamAssembly(BYTE password[0x20])
{
    memset(password, 0, 0x20);
    memcpy(password, "TEAMASSEMBLY", 12);
}

// 0x15920: hddlockdisable, SECURITY DISABLE PASSWORD with the drive's own password
void CTelnetServer::HddLockDisable()
{
    if (HddPasswordMissing()) {
        Printf("Error : XboxHDKey BAD\n\r");
        return;
    }
    AtaSecurityCommand(0xF6, 0, g_HddPassword);
    IdentifyDrive();
}

// 0x15960: hddlockenable, SECURITY SET PASSWORD: master "TEAMASSEMBLY", user the drive's own
// password (this locks the drive)
void CTelnetServer::HddLockEnable()
{
    if (HddPasswordMissing()) {
        Printf("Error : XboxHDKey BAD\n\r");
        return;
    }
    BYTE master[0x20];
    TeamAssembly(master);
    AtaSecurityCommand(0xF1, 1, master);
    AtaSecurityCommand(0xF1, 0, g_HddPassword);
    IdentifyDrive();
}

// 0x15A00: hddmasterdisable, SECURITY DISABLE PASSWORD with the master password
void CTelnetServer::HddMasterDisable()
{
    if (HddPasswordMissing()) {
        Printf("Error : XboxHDKey BAD\n\r");
        return;
    }
    BYTE master[0x20];
    TeamAssembly(master);
    AtaSecurityCommand(0xF6, 1, master);
    IdentifyDrive();
}

// 0x15A90: hddlockdisablexboxhdkey <16 hex bytes>: the drive's password from the given
// XboxHDKey (HMAC-SHA1 of model and serial number), then SECURITY UNLOCK and DISABLE PASSWORD
void CTelnetServer::HddLockDisableXboxHdKey()
{
    if (g_TelnetArgc != 0x11) {
        Printf("Usage : HDDLockDisableXboxHDKey <Key[0]> <Key[1]> ... <Key[15]>\n\r");
        return;
    }
    BYTE key[0x10], password[0x20] = {};
    for (int i = 0; i < 0x10; i++)
        key[i] = (BYTE)strtoul(ARG(i + 1), nullptr, 16);
    XcHMAC(key, 0x10, (PUCHAR)HalDiskModelNumber.Buffer, HalDiskModelNumber.Length,
           (PUCHAR)HalDiskSerialNumber.Buffer, HalDiskSerialNumber.Length, password);
    AtaSecurityCommand(0xF2, 0, password);
    AtaSecurityCommand(0xF6, 0, password);
    IdentifyDrive();
}

// 0x15B50: hddlockdisablepassword <32 hex bytes>: SECURITY UNLOCK and DISABLE PASSWORD with it
void CTelnetServer::HddLockDisablePassword()
{
    if (g_TelnetArgc != 0x21) {
        Printf("Usage : HDDLockDisablePassword <Key[0]> <Key[1]> ... <Key[31]>\n\r");
        return;
    }
    BYTE password[0x20];
    for (int i = 0; i < 0x20; i++)
        password[i] = (BYTE)strtoul(ARG(i + 1), nullptr, 16);
    AtaSecurityCommand(0xF2, 0, password);
    AtaSecurityCommand(0xF6, 0, password);
    IdentifyDrive();
}

// 0x15DF0: SMBUSDUMP <address> <length> <B/W> (hex), by bytes or words. Each value is stored as
// a ULONG at its byte position (as the original: later ones overwrite the zero high bytes).
void CTelnetServer::SmbusDump()
{
    if (g_TelnetArgc != 4) {
        Printf("SMBUSDump V0.2\n\r");
        Printf("Usage : SMBUSDump <Adr> <Len> <B/W>\n\r");
        return;
    }
    DWORD address = strtoul(ARG(1), nullptr, 16), len = strtoul(ARG(2), nullptr, 16);
    bool words = !(ARG(3)[0] == 'b' || ARG(3)[0] == 'B');
    char line[0x90];
    for (DWORD pos = 0; pos < len; pos += 16) {
        BYTE buf[0x14];
        ULONG v;
        if (!words) {
            for (int i = 0; i < 16; i++) {
                HalReadSMBusValue((UCHAR)(address | 1), (UCHAR)(pos + i), FALSE, &v);
                memcpy(buf + i, &v, 4);
            }
        } else {
            for (int i = 0; i < 16; i += 2) {
                HalReadSMBusValue((UCHAR)(address | 1), (UCHAR)(pos + i), TRUE, &v);
                memcpy(buf + i, &v, 4);
            }
        }
        HexLine(line, pos, buf);
        Printf("%s\r\n", line);
    }
}

// 0x15FD0: MEMDUMP <address> <length> (hex), physical memory. (The original's line ends with
// "%hs\n\r", the XDK's narrow %s.)
void CTelnetServer::MemDump()
{
    if (g_TelnetArgc != 3) {
        Printf("MemDump V0.1\n\r");
        Printf("Usage : MemDump <Addresse> <Len>\n\r");
        return;
    }
    DWORD address = strtoul(ARG(1), nullptr, 16), len = strtoul(ARG(2), nullptr, 16);
    if (address + len < address)
        len = 0xFFFFFFFF - address;
    const BYTE *mem = (const BYTE *)MmMapIoSpace(address, len, PAGE_READWRITE | PAGE_NOCACHE);
    char line[0x90];
    for (DWORD pos = 0; pos < len; pos += 16) {
        HexLine(line, address + pos, mem + pos);
        Printf("%s\n\r", line);
    }
    MmUnmapIoSpace((PVOID)mem, len);
}

// 0x16140: SaveMem <file> <address> <length> (hex)
void CTelnetServer::SaveMem()
{
    if (g_TelnetArgc != 4) {
        Printf("SaveMem V0.1\n\r");
        Printf("Usage : SaveMem <FileName> <Addresse> <Len>\n\r");
        return;
    }
    DWORD address = strtoul(ARG(2), nullptr, 16), len = strtoul(ARG(3), nullptr, 16);
    if (address + len < address)
        len = 0xFFFFFFFF - address;
    void *mem = MmMapIoSpace(address, len, PAGE_READWRITE | PAGE_NOCACHE);
    if (FILE *f = fopen(ARG(1), "wb")) {
        fwrite(mem, 1, len, f);
        fclose(f);
    } else {
        Printf("Error Creating File %s\n\r", ARG(1));
    }
    MmUnmapIoSpace(mem, len);
}

// 0x16210: TEST2 <register> (hex, no check of the argument count): for each PCI configuration
// address from the register to the end of its 256-byte block, in dwords, writes 256 patterns
// (0x00000000, 0x01010101, ... 0xFFFFFFFF) through ports 0xCF8 / 0xCFC and shows those after
// which the flash's first dword no longer reads 0xFFFFFFFF; then XboxHardwareInfo's flags
void CTelnetServer::Test2()
{
    DWORD reg = strtoul(ARG(1), nullptr, 16);
    volatile const DWORD *flash = (volatile const DWORD *)MmMapIoSpace(0xFF000000, 0x10, PAGE_READWRITE | PAGE_NOCACHE);
    DWORD low = reg & 0xFF;
    if (low < 0x100) {
        DWORD count = ((0xFF - low) >> 2) + 1;
        reg = (reg & 0xFFFFFF00) + low;
        for (; count; count--, reg += 4) {
            Printf("%08x\n\r", reg);
            for (DWORD k = 0; k < 0x100; k++) {
                DWORD pattern = k * 0x01010101;
                OutDword(0xCF8, reg);
                OutDword(0xCFC, pattern);
                if (*flash != 0xFFFFFFFF) {
                    Printf("%08x-%08x\n\r", reg, pattern, 0);
                    Sleep(100);
                }
            }
        }
    }
    MmUnmapIoSpace((PVOID)flash, 0x10);
    Printf("%08x\n\r", XboxHardwareInfo.Flags);
}

// 0x16320: O <port> <byte> (hex)
void CTelnetServer::IoOut()
{
    if (g_TelnetArgc != 3) {
        Printf("IO Out V0.1\n\r");
        Printf("Usage : O <Addresse> <Data>\n\r");
        return;
    }
    DWORD port = strtoul(ARG(1), nullptr, 16);
    BYTE data = (BYTE)strtoul(ARG(2), nullptr, 16);
    OutByte((WORD)port, data);
}

// 0x16390: IODUMP <port> <length> (hex)
void CTelnetServer::IoDump()
{
    if (g_TelnetArgc != 3) {
        Printf("IODump V0.1\n\r");
        Printf("Usage : IODump <Addresse> <Len>\n\r");
        return;
    }
    DWORD port = strtoul(ARG(1), nullptr, 16), len = strtoul(ARG(2), nullptr, 16);
    if (port + len < port)
        len = 0xFFFF - port;
    char line[0x90];
    for (DWORD pos = 0; pos < len; pos += 16) {
        BYTE buf[16];
        for (int i = 0; i < 16; i++)
            buf[i] = InByte((WORD)(port + i + pos));
        HexLine(line, port + pos, buf);
        Printf("%s\n\r", line); // ("%hs\n\r" in the original)
    }
}

// ---- PCI ------------------------------------------------------------------------------------

// 0x948F8: class code names (0x0605 is given twice, as in the original: NuBus is 0x0606)
static const struct {
    WORD cls;
    const char *name;
} s_PciClasses[0x4B] = {
    {0x0000, ""},
    {0x0001, "VGA-compatible device"},
    {0x0100, "SCSI bus controller"},
    {0x0101, "IDE controller"},
    {0x0102, "Floppy disk controller"},
    {0x0103, "IPI bus controller"},
    {0x0104, "RAID controller"},
    {0x0180, "Other mass storage controller"},
    {0x0200, "Ethernet controller"},
    {0x0201, "Token Ring controller"},
    {0x0202, "FDDI controller"},
    {0x0203, "ATM controller"},
    {0x0204, "ISDN controller"},
    {0x0280, "Other network controller"},
    {0x0300, "VGA-compatible controller"},
    {0x0301, "XGA controller"},
    {0x0302, "3D controller"},
    {0x0380, "Other display controller"},
    {0x0400, "Video device"},
    {0x0401, "Audio device"},
    {0x0402, "Computer telephony device"},
    {0x0480, "Other multimedia device"},
    {0x0500, "RAM"},
    {0x0501, "Flash"},
    {0x0580, "Other memory controller"},
    {0x0600, "Host bridge"},
    {0x0601, "ISA bridge"},
    {0x0602, "EISA bridge"},
    {0x0603, "MCA bridge"},
    {0x0604, "PCI-to-PCI bridge"},
    {0x0605, "PCMCIA bridge"},
    {0x0605, "NuBus bridge"},
    {0x0607, "CardBus bridge"},
    {0x0608, "RACEway bridge"},
    {0x0680, "Other bridge device"},
    {0x0700, "Serial controller"},
    {0x0701, "Parallel port"},
    {0x0702, "Multiport serial controller"},
    {0x0703, "Modem"},
    {0x0780, "Other communications device"},
    {0x0900, "Keyboard controller"},
    {0x0901, "Digitizer (pen)"},
    {0x0902, "Mouse controller"},
    {0x0903, "Scanner controller"},
    {0x0904, "Gameport controller"},
    {0x0980, "Other input controller"},
    {0x0A00, "Generic docking station"},
    {0x0A80, "Other type of docking station"},
    {0x0B00, "386"},
    {0x0B01, "386"},
    {0x0B02, "Pentium"},
    {0x0B10, "Alpha"},
    {0x0B20, "PowerPC"},
    {0x0B30, "MIPS"},
    {0x0B40, "Co-processor"},
    {0x0C00, "IEEE 1394 (FireWire)"},
    {0x0C01, "ACCESS.bus"},
    {0x0C02, "SSA"},
    {0x0C03, "Universal Serial Bus (USB)"},
    {0x0C04, "Fibre Channel"},
    {0x0C05, "SMBus (System Management Bus)"},
    {0x0D00, "iRDA compatible controller"},
    {0x0D01, "Consumer IR controller"},
    {0x0D10, "RF controller"},
    {0x0D80, "Other type of wireless controller"},
    {0x0E00, "Intelligent I/O (I2O) Architecture"},
    {0x0F01, "TV"},
    {0x0F02, "Audio"},
    {0x0F03, "Voice"},
    {0x0F04, "Data"},
    {0x1000, "Network and computing en/decryption"},
    {0x1010, "Entertainment en/decryption"},
    {0x1080, "Other en/decryption"},
    {0x1100, "DPIO modules"},
    {0x1180, "Other data acquisition/signal processing controllers"},
};

static const char *PciClassName(DWORD cls)
{
    for (const auto &c : s_PciClasses)
        if (cls == c.cls)
            return c.name;
    return nullptr;
}

// 0x16520 / 0x16540: configuration space through ports 0xCF8 / 0xCFC
static void PciWrite(DWORD address, DWORD value)
{
    OutDword(0xCF8, address | 0x80000000);
    OutDword(0xCFC, value);
}
static DWORD PciRead(DWORD address)
{
    OutDword(0xCF8, address | 0x80000000);
    return InDword(0xCFC);
}

// 0x16570: a register (hex) and a value of up to 8 hex digits for the bytes from that register
// to the end of its dword ("x" digits are left alone): the dword's value and the mask of the bits
// to keep. Returns the dword-aligned register.
static DWORD ParseMask(const char *regText, const char *valueText, DWORD *value, DWORD *mask)
{
    DWORD reg = strtoul(regText, nullptr, 16);
    unsigned pad = (4 - (reg & 3)) * 2 - strlen(valueText);
    if (pad > 7) {
        *mask = 0xFFFFFFFF;
        *value = 0;
        return reg & ~3;
    }
    char digits[0x20];
    memset(digits, 'x', 8);
    strcpy(digits + pad, valueText);
    strcat(digits, "xxxxxxxx");
    digits[8] = 0;
    strlwr(digits);
    *mask = 0;
    for (int i = 0; i < 8; i++) {
        if (digits[i] == 'x') {
            *mask = *mask << 4 | 0xF;
            digits[i] = '0';
        } else {
            *mask <<= 4;
        }
    }
    *value = strtoul(digits, nullptr, 16);
    return reg & ~3;
}

// 0x16690: one function's 256 configuration bytes (its address's register byte is left at 0xFC)
void CTelnetServer::PciDumpDevice(DWORD *address)
{
    DWORD a = *address;
    Printf("Bus : %d Device : %d Func : %d -> %08x : ", (a >> 16) & 0x1F, (a >> 11) & 0x1F, (a >> 8) & 7, a);
    *(BYTE *)address = 8;
    if (const char *name = PciClassName(PciRead(*address) >> 16))
        Printf("%s", name);
    Printf("\n\r");
    for (int reg = 0; reg < 0x100; reg += 4) {
        *(BYTE *)address = (BYTE)reg;
        Printf((reg & 0xF) ? " - " : "\n\r");
        Printf("%02x : %08lx", reg, PciRead(*address));
    }
    Printf("\n\r");
}

// 0x16780: every function on buses 0..31, listed or dumped. Bus 0 device 0 has only function 0
// looked at; reaching 1:1.0 skips to the next bus and function (sic).
void CTelnetServer::PciScan(bool dump)
{
    for (int bus = 0; bus < 0x20; bus++) {
        for (int dev = 0; dev < 0x20; dev++) {
            for (int func = 0; func < 8; func++) {
                DWORD a = (((bus & 0x1F) << 5 | (dev & 0x1F)) << 3 | (func & 7)) << 8;
                if (a == 0x100) {
                    func = 8;
                    continue;
                }
                if (a == 0x10800) {
                    bus++;
                    continue;
                }
                DWORD id = PciRead(a);
                if (id == 0xFFFFFFFF)
                    continue;
                if (dump) {
                    PciDumpDevice(&a);
                } else {
                    Printf("%08lx:%2d:%2d:%d %08lx - ", a, bus, dev, func, id);
                    a = (a & 0xFFFFFF08) | 8;
                    if (const char *name = PciClassName(PciRead(a) >> 16))
                        Printf("%s", name);
                    Printf("\n\r");
                }
                if (func == 0 && !(PciRead((a & 0xFFFFFF0C) | 0xC) & 0x800000))
                    break; // not a multi-function device
            }
        }
    }
}

// 0x16910: PCIDUMP [b:<bus>] [all | <dev> [<func>] [<reg> <value>]]: the list, every function
// dumped, one function dumped, or a register written (masked as ParseMask says) and dumped
void CTelnetServer::PciDump()
{
    DWORD address = 0, value = 0, mask = 0;
    bool dump = false, write = false;
    Printf("\n\rPCIDump V2.0\n\r");
    if (g_TelnetArgc >= 2) {
        const char *a = ARG(1);
        if ((a[0] == 'b' || a[0] == 'B') && a[1] == ':') {
            address = (strtol(a + 2, nullptr, 10) & 0x1F) << 16;
            g_TelnetArgv++;
            g_TelnetArgc--;
        }
    }
    if (g_TelnetArgc == 1) {
        PciScan(false);
        return;
    }
    if (g_TelnetArgc == 2) {
        if (_stricmp(ARG(1), "all") == 0) {
            PciScan(true);
            return;
        }
        address ^= ((strtol(ARG(1), nullptr, 10) << 11) ^ address) & 0xF800;
        dump = true;
    }
    if (g_TelnetArgc == 3) {
        DWORD dev = strtol(ARG(1), nullptr, 10), func = strtol(ARG(2), nullptr, 10);
        address = ((dev & 0x1F) << 3 | (func & 7)) << 8 | (address & 0xFFFF00FF);
        dump = true;
    }
    if (g_TelnetArgc == 4) {
        DWORD dev = strtol(ARG(1), nullptr, 10);
        DWORD reg = ParseMask(ARG(2), ARG(3), &value, &mask);
        address = (dev & 0x1F) << 11 | (address & 0xFFFF0700) | (reg & 0xFF);
        dump = write = true;
    }
    if (g_TelnetArgc == 5) {
        DWORD dev = strtol(ARG(1), nullptr, 10), func = strtol(ARG(2), nullptr, 10);
        DWORD reg = ParseMask(ARG(3), ARG(4), &value, &mask);
        address = ((dev & 0x1F) << 3 | (func & 7)) << 8 | (reg & 0xFF) | (address & 0xFFFF0000);
        dump = write = true;
    }
    if (write) {
        Printf("%08x-%08x,%08x\r\n", address, mask, value);
        PciWrite(address, (PciRead(address) & mask) | value);
    }
    if (dump)
        PciDumpDevice(&address);
}
