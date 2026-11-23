// RSA's MD5 (RFC 1321); see include/evox/md5.h.
#include <string.h>
#include "evox/md5.h"

static_assert(sizeof(MD5_CTX) == 0x58, "MD5_CTX");

#define F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | ~(z)))
#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define STEP(f, a, b, c, d, x, s, ac) \
    (a) += f((b), (c), (d)) + (x) + (DWORD)(ac); \
    (a) = ROL((a), (s)) + (b)

// 0x117C0 (callers go through the jump at 0x11890)
void MD5Init(MD5_CTX *ctx)
{
    ctx->count[0] = ctx->count[1] = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
}

// 0x118A0; Encode (0x117F0, in MD5Final) and Decode (0x11840, here) are written inline
static void Transform(DWORD state[4], const BYTE block[64])
{
    DWORD a = state[0], b = state[1], c = state[2], d = state[3], x[16];
    for (int i = 0; i < 16; i++)
        x[i] = block[i * 4] | block[i * 4 + 1] << 8 | block[i * 4 + 2] << 16 | (DWORD)block[i * 4 + 3] << 24;

    STEP(F, a, b, c, d, x[0], 7, 0xD76AA478);   STEP(F, d, a, b, c, x[1], 12, 0xE8C7B756);
    STEP(F, c, d, a, b, x[2], 17, 0x242070DB);  STEP(F, b, c, d, a, x[3], 22, 0xC1BDCEEE);
    STEP(F, a, b, c, d, x[4], 7, 0xF57C0FAF);   STEP(F, d, a, b, c, x[5], 12, 0x4787C62A);
    STEP(F, c, d, a, b, x[6], 17, 0xA8304613);  STEP(F, b, c, d, a, x[7], 22, 0xFD469501);
    STEP(F, a, b, c, d, x[8], 7, 0x698098D8);   STEP(F, d, a, b, c, x[9], 12, 0x8B44F7AF);
    STEP(F, c, d, a, b, x[10], 17, 0xFFFF5BB1); STEP(F, b, c, d, a, x[11], 22, 0x895CD7BE);
    STEP(F, a, b, c, d, x[12], 7, 0x6B901122);  STEP(F, d, a, b, c, x[13], 12, 0xFD987193);
    STEP(F, c, d, a, b, x[14], 17, 0xA679438E); STEP(F, b, c, d, a, x[15], 22, 0x49B40821);

    STEP(G, a, b, c, d, x[1], 5, 0xF61E2562);   STEP(G, d, a, b, c, x[6], 9, 0xC040B340);
    STEP(G, c, d, a, b, x[11], 14, 0x265E5A51); STEP(G, b, c, d, a, x[0], 20, 0xE9B6C7AA);
    STEP(G, a, b, c, d, x[5], 5, 0xD62F105D);   STEP(G, d, a, b, c, x[10], 9, 0x02441453);
    STEP(G, c, d, a, b, x[15], 14, 0xD8A1E681); STEP(G, b, c, d, a, x[4], 20, 0xE7D3FBC8);
    STEP(G, a, b, c, d, x[9], 5, 0x21E1CDE6);   STEP(G, d, a, b, c, x[14], 9, 0xC33707D6);
    STEP(G, c, d, a, b, x[3], 14, 0xF4D50D87);  STEP(G, b, c, d, a, x[8], 20, 0x455A14ED);
    STEP(G, a, b, c, d, x[13], 5, 0xA9E3E905);  STEP(G, d, a, b, c, x[2], 9, 0xFCEFA3F8);
    STEP(G, c, d, a, b, x[7], 14, 0x676F02D9);  STEP(G, b, c, d, a, x[12], 20, 0x8D2A4C8A);

    STEP(H, a, b, c, d, x[5], 4, 0xFFFA3942);   STEP(H, d, a, b, c, x[8], 11, 0x8771F681);
    STEP(H, c, d, a, b, x[11], 16, 0x6D9D6122); STEP(H, b, c, d, a, x[14], 23, 0xFDE5380C);
    STEP(H, a, b, c, d, x[1], 4, 0xA4BEEA44);   STEP(H, d, a, b, c, x[4], 11, 0x4BDECFA9);
    STEP(H, c, d, a, b, x[7], 16, 0xF6BB4B60);  STEP(H, b, c, d, a, x[10], 23, 0xBEBFBC70);
    STEP(H, a, b, c, d, x[13], 4, 0x289B7EC6);  STEP(H, d, a, b, c, x[0], 11, 0xEAA127FA);
    STEP(H, c, d, a, b, x[3], 16, 0xD4EF3085);  STEP(H, b, c, d, a, x[6], 23, 0x04881D05);
    STEP(H, a, b, c, d, x[9], 4, 0xD9D4D039);   STEP(H, d, a, b, c, x[12], 11, 0xE6DB99E5);
    STEP(H, c, d, a, b, x[15], 16, 0x1FA27CF8); STEP(H, b, c, d, a, x[2], 23, 0xC4AC5665);

    STEP(I, a, b, c, d, x[0], 6, 0xF4292244);   STEP(I, d, a, b, c, x[7], 10, 0x432AFF97);
    STEP(I, c, d, a, b, x[14], 15, 0xAB9423A7); STEP(I, b, c, d, a, x[5], 21, 0xFC93A039);
    STEP(I, a, b, c, d, x[12], 6, 0x655B59C3);  STEP(I, d, a, b, c, x[3], 10, 0x8F0CCC92);
    STEP(I, c, d, a, b, x[10], 15, 0xFFEFF47D); STEP(I, b, c, d, a, x[1], 21, 0x85845DD1);
    STEP(I, a, b, c, d, x[8], 6, 0x6FA87E4F);   STEP(I, d, a, b, c, x[15], 10, 0xFE2CE6E0);
    STEP(I, c, d, a, b, x[6], 15, 0xA3014314);  STEP(I, b, c, d, a, x[13], 21, 0x4E0811A1);
    STEP(I, a, b, c, d, x[4], 6, 0xF7537E82);   STEP(I, d, a, b, c, x[11], 10, 0xBD3AF235);
    STEP(I, c, d, a, b, x[2], 15, 0x2AD7D2BB);  STEP(I, b, c, d, a, x[9], 21, 0xEB86D391);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

// 0x12160
void MD5Update(MD5_CTX *ctx, const BYTE *data, DWORD len)
{
    DWORD index = (ctx->count[0] >> 3) & 0x3F;
    if ((ctx->count[0] += len << 3) < (len << 3))
        ctx->count[1]++;
    ctx->count[1] += len >> 29;
    DWORD part = 64 - index, i = 0;
    if (len >= part) {
        memcpy(&ctx->buffer[index], data, part);
        Transform(ctx->state, ctx->buffer);
        for (i = part; i + 63 < len; i += 64)
            Transform(ctx->state, &data[i]);
        index = 0;
    }
    memcpy(&ctx->buffer[index], &data[i], len - i);
}

// 0x12220
void MD5Final(BYTE digest[16], MD5_CTX *ctx)
{
    static const BYTE padding[64] = {0x80};
    BYTE bits[8];
    for (int i = 0; i < 8; i++)
        bits[i] = (BYTE)(ctx->count[i / 4] >> (i % 4 * 8));
    DWORD index = (ctx->count[0] >> 3) & 0x3F;
    MD5Update(ctx, padding, index < 56 ? 56 - index : 120 - index);
    MD5Update(ctx, bits, 8);
    for (int i = 0; i < 16; i++)
        digest[i] = (BYTE)(ctx->state[i / 4] >> (i % 4 * 8));
    memset(ctx, 0, sizeof *ctx);
}
