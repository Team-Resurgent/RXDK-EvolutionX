# Unpack evoxdash.xbe (build 3935). The XBE is a small loader stub around the real XBE, stored as
# raw DEFLATE: the stub's descriptor at 0x12C78 gives (header offset, unpacked size, packed size)
# and the stream starts at 0x12C84. The stub inflates it and loads the result in place of itself.
# usage: python tools/unpack_evox.py evoxdash.xbe work/evoxdash_unpacked.xbe
import hashlib, struct, sys, zlib

ORIGINAL_SHA256 = '3b84ca74535fad8800d7016f1dd1f5e7e6a6d5d02508f35af802aab9b2933022'
UNPACKED_SHA256 = '5b4ceb20b9311715fd27ff4abfeb9fc738bf4d4aa6261dc62b8911b4eac1acec'
BASE = 0x10000


def unpack(packed):
    """The inner XBE of evoxdash.xbe 3935; raises ValueError for any other file."""
    h = hashlib.sha256(packed).hexdigest()
    if h != ORIGINAL_SHA256:
        raise ValueError('not the EvolutionX 3935 evoxdash.xbe (SHA-256 %s)' % h)
    _, size, packed_size = struct.unpack_from('<III', packed, 0x12C78 - BASE)
    start = 0x12C84 - BASE
    inner = zlib.decompressobj(-15).decompress(packed[start:start + packed_size])
    if len(inner) != size or hashlib.sha256(inner).hexdigest() != UNPACKED_SHA256:
        raise ValueError('unpacked image does not match')
    return inner


if __name__ == '__main__':
    inner = unpack(open(sys.argv[1], 'rb').read())
    open(sys.argv[2], 'wb').write(inner)
    print('%s: %#x bytes' % (sys.argv[2], len(inner)))
