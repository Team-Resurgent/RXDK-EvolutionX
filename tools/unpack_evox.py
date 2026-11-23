# Unpack evoxdash.xbe: emulate its stub decompressor (0x127F0) and write the inner XBE.
# usage: python tools/unpack_evox.py evoxdash.xbe work/evoxdash_unpacked.xbe   (needs unicorn)
import struct,sys
from unicorn import *
from unicorn.x86_const import *
d=open(sys.argv[1],'rb').read()
u=Uc(UC_ARCH_X86,UC_MODE_32)
u.mem_map(0x10000,0x100000); u.mem_write(0x10000,d[:0xc0000])
OUT=0x1000000; sz0,sz1,clen=struct.unpack_from('<III',d,0x12c78-0x10000)
u.mem_map(OUT,(sz1+0x200000+0xfff)&~0xfff)
u.mem_write(0x129f8,struct.pack('<I',OUT))
u.mem_map(0x800000,0x100000); sp=0x8ff000; STOP=0x900000-0x10
u.mem_write(sp,struct.pack('<I',STOP)); u.reg_write(UC_X86_REG_ESP,sp)
u.emu_start(0x127f0,STOP)
n=struct.unpack('<I',u.mem_read(0x11008,4))[0]
print('out bytes',hex(n),'sizes',hex(sz0),hex(sz1),hex(clen))
open(sys.argv[2],'wb').write(bytes(u.mem_read(OUT,n)))
