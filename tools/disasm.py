# Disassemble one function of the unpacked original, with strings, kernel imports and call
# targets named inline. Function bounds come from rea/decomp (Ghidra) when present.
# usage: python tools/disasm.py <addr> [work/evoxdash_unpacked.xbe]
import capstone, os, re, struct, sys

addr = int(sys.argv[1], 16)
path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), '..', 'work', 'evoxdash_unpacked.xbe')
root = os.path.join(os.path.dirname(__file__), '..')
d = open(path, 'rb').read()
BASE = 0x10000
nsec, secha = struct.unpack_from('<II', d, 0x11C)
secs = []
for i in range(nsec):
    fl, va, vs, ra, rs, na = struct.unpack_from('<IIIIII', d, secha - BASE + i * 56)
    secs.append((va, vs, ra, rs))

def raw(va):
    for v, vs, r, rs in secs:
        if v <= va < v + rs:
            return r + va - v
    return None

def cstr(va):
    o = raw(va)
    if o is None:
        return None
    m = re.match(rb'[ -~\t\r\n]{3,}\0', d[o:o + 300])
    return m.group()[:-1].decode() if m else None

def wstr(va):
    o = raw(va)
    if o is None:
        return None
    m = re.match(rb'(?:[ -~]\0){3,}\0\0', d[o:o + 600])
    return m.group()[:-2].decode('utf-16le') if m else None

kthunk = struct.unpack_from('<I', d, 0x158)[0]
# kernel export names from the RXDK SDK's ordinal header
KNAMES = {}
rxdk = os.environ.get('RXDK') or os.path.join(os.environ.get('ProgramData', 'C:/ProgramData'), 'RXDK')
ords_h = os.path.join(rxdk, 'sdk', 'include', 'xboxkrnl', 'xboxkrnl_ordinals.h')
if os.path.exists(ords_h):
    for name, n in re.findall(r'#define\s+RXDK_ORDINAL_(\w+)\s+(\d+)', open(ords_h).read()):
        KNAMES[int(n)] = name
def kslot(va):
    i = (va - kthunk) // 4
    if 0 <= i < 139 and (va - kthunk) % 4 == 0:
        o = struct.unpack_from('<I', d, raw(kthunk) + 4 * i)[0] & 0x7FFFFFFF
        return KNAMES.get(o, 'ord_%d' % o)
    return None

# function bounds from the Ghidra export
ranges = []
f = os.path.join(root, 'rea', 'decomp', 'functions', '0x%x.c' % addr)
if os.path.exists(f):
    for m in re.finditer(r'^// range: (0x[0-9a-f]+)-(0x[0-9a-f]+)', open(f, encoding='latin-1').read(), re.M):
        ranges.append((int(m.group(1), 16), int(m.group(2), 16) + 1))
if not ranges:
    ranges = [(addr, addr + 0x400)]

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.skipdata = True
for s, e in ranges:
    for i in md.disasm(d[raw(s):raw(s) + (e - s)], s):
        note = []
        for m in re.finditer(r'0x[0-9a-f]+', i.op_str):
            v = int(m.group(), 16)
            k = kslot(v)
            if k:
                note.append(k)
                continue
            t = cstr(v) or wstr(v)
            if t:
                note.append(repr(t)[:70])
            elif i.mnemonic == 'call' and os.path.exists(os.path.join(root, 'rea', 'decomp', 'functions', '0x%x.c' % v)):
                note.append('FUN_%x' % v)
        print('%08x  %-7s %-40s %s' % (i.address, i.mnemonic, i.op_str, ('; ' + ', '.join(note)) if note else ''))
    print('--')
