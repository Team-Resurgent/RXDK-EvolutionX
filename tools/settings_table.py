# Read OpenSettings (0x1A150) in the original and print its settings table in order: each entry's
# label, value buffer, type, and the evox.ini value (section, key, default) copied into the buffer.
# A reading aid for rebuilding src/settings.cpp; it prints what it cannot classify.
# usage: python tools/settings_table.py [start end]
import capstone, os, re, struct, sys

root = os.path.join(os.path.dirname(__file__), '..')
d = open(os.path.join(root, 'work', 'evoxdash_unpacked.xbe'), 'rb').read()
start, end = (int(sys.argv[1], 16), int(sys.argv[2], 16)) if len(sys.argv) > 2 else (0x1A150, 0x1B100)
BASE, TABLE = 0x10000, 0x3F88B8


def raw(va):
    if 0x10000 <= va < 0x7D5C0:
        return va - BASE
    if 0x7D5C0 <= va < 0x836E0:
        return va - 0x7D5C0 + 0x6E000
    if 0x836E0 <= va < 0x836E0 + 0x353488:
        return va - 0x836E0 + 0x75000
    return None


def text(va):
    o = raw(va)
    if o is None:
        return '[%x]' % va
    s = d[o:o + 120]
    if s[1:2] == b'\0' and s[0:1] != b'\0':  # UTF-16
        return 'L"%s"' % s.decode('utf-16le', 'replace').split('\0')[0]
    return '"%s"' % s.split(b'\0')[0].decode('latin-1')


md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = False
pushes, regs, idx = [], {}, None
for i in md.disasm(d[raw(start):raw(end)], start):
    m, op = i.mnemonic, i.op_str
    if m == 'push':
        v = regs.get(op) if op in regs else (int(op, 16) if op.startswith('0x') else op)
        pushes.append(v)
        continue
    if m == 'mov' and re.match(r'^e[a-d]x|^e[sd]i$', op.split(',')[0]):
        dst, src = [x.strip() for x in op.split(',', 1)]
        if src.startswith('0x'):
            regs[dst] = int(src, 16)
        elif src in regs:
            regs[dst] = regs[src]
        else:
            regs.pop(dst, None)
    if m == 'call':
        t = int(op, 16) if op.startswith('0x') else None
        if t == 0x11780 and len(pushes) >= 3:
            sec, key, dflt = pushes[-1], pushes[-2], pushes[-3]
            print('    ini [%s] %s = %s' % (text(sec) if isinstance(sec, int) else sec,
                                            text(key) if isinstance(key, int) else key,
                                            text(dflt) if isinstance(dflt, int) else dflt))
        elif t is not None and t not in (0x11780,):
            print('    call 0x%x %s' % (t, [hex(p) if isinstance(p, int) else p for p in pushes[-4:]]))
        pushes = []
        continue
    if m == 'mov' and op.startswith('edx, 0x') and int(op[5:], 16) >= 0x3D0000:
        print('    -> copy to 0x%x' % int(op[5:], 16))
    mm = re.match(r'dword ptr \[(?:eax \+ )?(0x[0-9a-f]+)\], (.+)', op)
    if m == 'mov' and mm:
        a, v = int(mm.group(1), 16), mm.group(2)
        off = (a - TABLE) % 16 if a >= TABLE and a < TABLE + 0xC80 else None
        val = int(v, 16) if v.startswith('0x') else regs.get(v, v)
        if off == 0:
            print('entry label %s' % (text(val) if isinstance(val, int) else val))
        elif off == 4:
            print('      value 0x%x %s' % (val, text(val)) if isinstance(val, int) else '      value %s' % val)
        elif off == 0xC:
            print('      type %s' % (hex(val) if isinstance(val, int) else val))
