// RSA's MD5 (RFC 1321), as linked into EvoX (0x117C0..0x12390 in the original); it checksums
// the BIOS flash for the BIOS version.
#pragma once
#include <xtl.h>

struct MD5_CTX {          // 0x58 bytes
    DWORD state[4];       // +0x00 A, B, C, D
    DWORD count[2];       // +0x10 bits, low word first
    BYTE buffer[64];      // +0x18
};

void MD5Init(MD5_CTX *ctx);                                  // 0x117C0
void MD5Update(MD5_CTX *ctx, const BYTE *data, DWORD len);   // 0x12160
void MD5Final(BYTE digest[16], MD5_CTX *ctx);                // 0x12220
