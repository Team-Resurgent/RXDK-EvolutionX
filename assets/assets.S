# EvoX's built-in graphics and in-game reset code, cut unchanged from the original evoxdash.xbe
# (build 3935) at the addresses below, and linked in as they are. The .incbin paths are relative
# to the project root, where build.sh assembles this file.
.data

# g_FontXpr, 0x83750 in the original: XPR with the font texture and glyphs (CXBFont)
.balign 4096
.globl _g_FontXpr
_g_FontXpr:
.incbin "assets/g_FontXpr.bin"
.balign 4
.globl _g_FontXprSize
_g_FontXprSize:
.long 69632

# g_ResourceXpr, 0xCF240 in the original: XPR with 4 resources (CXBPackedResource)
.balign 4096
.globl _g_ResourceXpr
_g_ResourceXpr:
.incbin "assets/g_ResourceXpr.bin"
.balign 4
.globl _g_ResourceXprSize
_g_ResourceXprSize:
.long 2547712

# g_LogoMesh, 0x33D240 in the original: XBG mesh (CXBMesh)
.balign 4096
.globl _g_LogoMesh
_g_LogoMesh:
.incbin "assets/g_LogoMesh.bin"
.balign 4
.globl _g_LogoMeshSize
_g_LogoMeshSize:
.long 618240

# g_Tsr1, 0x95D68 in the original: in-game reset TSR 1, encoded (IgrInstall)
.balign 16
.globl _g_Tsr1
_g_Tsr1:
.incbin "assets/g_Tsr1.bin"
.balign 4
.globl _g_Tsr1Size
_g_Tsr1Size:
.long 29564

# g_Tsr2, 0x9D0E8 in the original: in-game reset TSR 2, encoded (IgrInstall)
.balign 16
.globl _g_Tsr2
_g_Tsr2:
.incbin "assets/g_Tsr2.bin"
.balign 4
.globl _g_Tsr2Size
_g_Tsr2Size:
.long 203008

# g_IgrLoader, 0x596473 in the original: the loader the kernel patch calls, built to run at 0x80000010 (IgrInstall)
.balign 16
.globl _g_IgrLoader
_g_IgrLoader:
.incbin "assets/g_IgrLoader.bin"
.balign 4
.globl _g_IgrLoaderSize
_g_IgrLoaderSize:
.long 1186
