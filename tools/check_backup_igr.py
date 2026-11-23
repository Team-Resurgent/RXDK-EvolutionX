# Check the log of tests/backup_igr.txt against what the files should hold: xemu's flash, EEPROM
# and HDD images, the drive key computed here, the TSR decoded here, and the loader with the
# logged fields filled in.
# usage: python tools/check_backup_igr.py <serial log> <flash image> <eeprom image> <hdd image> [assets dir]
import hashlib, hmac, re, struct, sys

log_path, flash_path, eeprom_path, hdd_path = sys.argv[1:5]
assets = sys.argv[5] if len(sys.argv) > 5 else 'out/assets'
log = open(log_path, 'rb').read().decode('latin-1')
ok = True


def check(name, got, want):
    global ok
    good = got == want
    ok &= good
    print('%-12s %s  %s' % (name, 'ok' if good else 'DIFFERENT', got if good else '%s, want %s' % (got, want)))


def logged_hash(path):
    m = re.search(r'hash %s (-?\d+) ([0-9a-f]{32})' % re.escape(path), log)
    return (int(m.group(1)), m.group(2)) if m else None


def md5(data):
    return (len(data), hashlib.md5(data).hexdigest())


flash = open(flash_path, 'rb').read()
check('bios.bin', logged_hash(r'c:\backup\bios.bin'), md5(flash * (0x100000 // len(flash))))
# disk.bin by 4 KB blocks: block 1 (sectors 8..15) changes on every boot, so it differs from the
# image file on the host
with open(hdd_path, 'rb') as f:
    disk = f.read(0x80000)
blocks = dict((int(b), h) for b, h in re.findall(r'block (\d+) ([0-9a-f]{32})', log))
differ = [b for b in range(0x80) if blocks.get(b) != hashlib.md5(disk[b * 0x1000:(b + 1) * 0x1000]).hexdigest()]
check('disk.bin', (logged_hash(r'c:\backup\disk.bin')[0], [b for b in differ if b != 1]), (0x80000, []))
check('eeprom.bin', logged_hash(r'c:\backup\eeprom.bin'), md5(open(eeprom_path, 'rb').read()))

# (the strings are padded; the kernel's lengths, logged by test builds, leave the padding out)
lengths = re.search(r'model (\d+), serial (\d+) bytes', log)
serial = re.search(r'Disk Serial Number : "([^"]*)"', log).group(1).encode()[:int(lengths.group(2))]
model = re.search(r'Disk Model Number  : "([^"]*)"', log).group(1).encode()[:int(lengths.group(1))]
hd_key = bytes.fromhex(''.join(re.search(r'XboxHDKey ((?:[0-9a-f]{2}\s){16})', log).group(1).split()))
key = hmac.new(hd_key, model + serial, hashlib.sha1).digest() + bytes(12)
check('hddkey.bin', logged_hash(r'c:\backup\hddkey.bin'), md5(key))


def decode(data):
    k1, k2 = 0x45766F58, 0x446F6321
    out = bytearray(data)
    for i in range(len(out)):
        out[i] = ((out[i] ^ (k1 & 0xFF)) + (k2 & 0xFF)) & 0xFF
        k2 = ((k2 << 8) | (k2 >> 24)) & 0xFFFFFFFF
        k1 = (((k1 >> 8) | (k1 << 24)) + k2) & 0xFFFFFFFF
    return bytes(out)


tsr = open(assets + '/g_Tsr1.bin', 'rb').read()
part0, part1 = struct.unpack_from('<2I', tsr)
ell = tsr[:16] + decode(tsr[16:16 + part0]) + decode(tsr[16 + part0:16 + part0 + part1])
check('evox.ell', logged_hash(r'E:\TDATA\EVOX\evox.ell'), md5(ell))

m = re.search(r'igr loader ([0-9a-f]{32}) imports((?: [0-9a-f]{8}){8})', log)
loader = bytearray(open(assets + '/g_IgrLoader.bin', 'rb').read())
loader[0x8C] = 0  # game region (main sets it only on the way out)
loader[0x8D] = 0
loader[0x8E:0xAE] = b''.join(struct.pack('<I', int(a, 16)) for a in m.group(2).split())
check('loader', m.group(1), hashlib.md5(loader).hexdigest())
check('kernel call', bool(re.search(r'igr call at 800[0-9a-f]{5}', log)), True)
sys.exit(0 if ok else 1)
