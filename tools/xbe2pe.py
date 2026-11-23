# Wrap an (unpacked, plain-address) XBE's sections in a PE32 so Ghidra / REA open it as x86 code.
# Sections keep their virtual addresses (image base 0x10000); the kernel thunk table is turned
# into a PE import table of xboxkrnl.exe by ordinal, so calls show up as xboxkrnl ordinals.
# usage: python tools/xbe2pe.py work/evoxdash_unpacked.xbe work/evoxdash.exe
import struct, sys

src, dst = sys.argv[1], sys.argv[2]
d = open(src, 'rb').read()
BASE = struct.unpack_from('<I', d, 0x104)[0]
nsec, secha = struct.unpack_from('<I', d, 0x11C)[0], struct.unpack_from('<I', d, 0x120)[0]
entry = struct.unpack_from('<I', d, 0x128)[0]
kthunk = struct.unpack_from('<I', d, 0x158)[0]
# the unpacked image stores plain addresses; a normal XBE scrambles them
if not (BASE <= entry < BASE + 0x4000000):
    entry ^= 0xA8FC57AB
    kthunk ^= 0x5B6D40B6

secs = []
for i in range(nsec):
    fl, va, vs, ra, rs, na = struct.unpack_from('<IIIIII', d, secha - BASE + i * 56)
    name = d[na - BASE:na - BASE + 8].split(b'\0')[0]
    secs.append((name, va, vs, ra, rs, fl))

def raw_of(va):
    for _, v, vs, r, rs, _ in secs:
        if v <= va < v + rs:
            return r + va - v
    raise ValueError(hex(va))

# kernel ordinals from the thunk table
ords = []
o = raw_of(kthunk)
while True:
    x = struct.unpack_from('<I', d, o + 4 * len(ords))[0]
    if x == 0:
        break
    ords.append(x & 0x7FFFFFFF)

FA = 0x200
def align(x, a): return (x + a - 1) & ~(a - 1)

# XBE header area -> its own section, then the XBE sections, then an import section
img_end = max(v + vs for _, v, vs, _, _, _ in secs)
IMP_VA = align(img_end, 0x1000)

# import section: descriptor, ILT, name; the IAT is the original thunk table itself
ilt_off, dll_off = 0x28, 0x28 + 4 * (len(ords) + 1)
imp = bytearray(dll_off + 16)
struct.pack_into('<IIIII', imp, 0, IMP_VA - BASE + ilt_off, 0, 0, IMP_VA - BASE + dll_off, kthunk - BASE)
for i, n in enumerate(ords):
    struct.pack_into('<I', imp, ilt_off + 4 * i, 0x80000000 | n)
imp[dll_off:dll_off + 12] = b'xboxkrnl.exe'

hdr_sec = (b'.xbehdr', BASE, 0x1000, 0, 0x1000, 0)
all_secs = [hdr_sec] + secs + [(b'.idata', IMP_VA, len(imp), None, len(imp), 0)]
nall = len(all_secs)
hdrsize = align(0x178 + 40 * nall, FA)

out = bytearray(hdrsize)
raw_ptr = hdrsize
blobs, table = [], []
for name, va, vs, ra, rs, fl in all_secs:
    data = bytes(imp) if ra is None else d[ra:ra + rs]
    if name == b'.xbehdr':
        data = d[:0x1000]
    rsz = align(len(data), FA)
    if name == b'.xbehdr':
        ch = 0x40000040                                   # initialized data, read
    elif name in (b'.text', b'_text', b'text', b'D3D', b'D3DX', b'XGRPH', b'XNET', b'XPP') and (fl & 4):
        ch = 0xE0000020                                   # code, rwx (XBE sections are all rwx)
    else:
        ch = 0xC0000040 | (0x80 if vs > len(data) else 0)  # data, rw
    table.append(struct.pack('<8sIIIIIIHHI', name, max(vs, len(data)), va - BASE, rsz, raw_ptr, 0, 0, 0, 0, ch))
    blobs.append(data + b'\0' * (rsz - len(data)))
    raw_ptr += rsz

# DOS + PE headers
struct.pack_into('<2s58xI', out, 0, b'MZ', 0x40)
coff = struct.pack('<4sHHIIIHH', b'PE\0\0', 0x14C, nall, struct.unpack_from('<I', d, 0x114)[0], 0, 0, 0xE0, 0x010F)
size_of_image = align(IMP_VA + len(imp), 0x1000) - BASE
opt = struct.pack('<HBBIIIIIIIIIHHHHHHIIIIHHIIIIII', 0x10B, 7, 0, 0, 0, 0,
                  entry - BASE, 0x1000, 0, BASE, 0x1000, FA, 4, 0, 0, 0, 4, 0, 0,
                  size_of_image, hdrsize, 0, 1, 0, 0x100000, 0x1000, 0x100000, 0x1000, 0, 16)
dirs = bytearray(16 * 8)
struct.pack_into('<II', dirs, 8, IMP_VA - BASE, 0x28)                    # import directory
struct.pack_into('<II', dirs, 12 * 8, kthunk - BASE, 4 * (len(ords) + 1))  # IAT
out[0x40:0x40 + len(coff)] = coff
out[0x58:0x58 + len(opt)] = opt
out[0x58 + len(opt):0x58 + len(opt) + len(dirs)] = dirs
p = 0x58 + 0xE0
for t in table:
    out[p:p + 40] = t
    p += 40
out += b''.join(blobs)
open(dst, 'wb').write(out)
print('%s: %d sections, entry %#x, %d kernel imports at %#x' % (dst, nall, entry, len(ords), kthunk))
