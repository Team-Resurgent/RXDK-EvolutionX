# Grab xemu's framebuffer through its monitor (start xemu with -monitor tcp:127.0.0.1:45454,server,nowait).
# Pauses the guest around the grab, reads NV_PCRTC_START (0xFD600800) for the scan-out address and
# saves 640x480 X8R8G8B8 as PNG.
# usage: python tools/xemu_shot.py out.png [port] [width height]
import socket, struct, sys, time, os, re, tempfile, zlib

png = sys.argv[1]
port = int(sys.argv[2]) if len(sys.argv) > 2 else 45454
w, h = (int(sys.argv[3]), int(sys.argv[4])) if len(sys.argv) > 4 else (640, 480)

s = socket.create_connection(('127.0.0.1', port), timeout=10)

def drain(quiet=0.3, limit=10):
    # read until the monitor has been quiet for `quiet` s (a prompt alone can belong to the
    # previous command, so waiting for "(qemu)" misaligns replies)
    s.settimeout(quiet)
    buf, end = b'', time.time() + limit
    while time.time() < end:
        try:
            chunk = s.recv(65536)
        except socket.timeout:
            break
        if not chunk:
            break
        buf += chunk
    return buf.decode(errors='replace')

def cmd(c):
    s.sendall(c.encode() + b'\n')
    return drain()

drain(1.0)  # banner
cmd('stop')
try:
    out = cmd('xp /1wx 0xfd600800')
    start = int(re.findall(r':\s*(0x[0-9a-fA-F]+)', out)[-1], 16) & 0x0FFFFFFF
    raw = os.path.join(tempfile.gettempdir(), 'xemu_fb.raw')
    cmd('pmemsave 0x%x %d "%s"' % (start, w * h * 4, raw.replace('\\', '/')))
finally:
    cmd('cont')
    s.close()

for _ in range(50):
    if os.path.exists(raw) and os.path.getsize(raw) == w * h * 4:
        break
    time.sleep(0.1)
data = open(raw, 'rb').read()
rows = b''.join(b'\0' + bytes(b for px in range(w) for b in (data[(y * w + px) * 4 + 2], data[(y * w + px) * 4 + 1], data[(y * w + px) * 4]))
               for y in range(h))
def chunk(t, b): return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b) & 0xFFFFFFFF)
open(png, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                      + chunk(b'IDAT', zlib.compress(rows, 6)) + chunk(b'IEND', b''))
print('%s: scan-out 0x%x' % (png, start))
