# EvolutionX 3935 layout (from `work/evoxdash_unpacked.xbe`)

`evoxdash.xbe` is packed. `tools/unpack_evox.py` runs the stub's decompressor (0x127F0) under
unicorn and writes the inner XBE. Its header stores addresses plain (not XOR-scrambled).
`tools/xbe2pe.py` wraps it in a PE (same addresses, kernel imports as `xboxkrnl.exe` ordinals) for
Ghidra: `work/evoxdash.exe`. `rea decompile-all` wrote Ghidra pseudocode for every function to
`rea/decomp/functions/<addr>.c` (1816 written, 1 failure in XNet, 113 imports).

Header: XDK 5455, title id 586F7645 "RemoteX", init flags 0xC, entry 0x29F53, kernel thunk
0x7D5C0 (139 imports).

## .text (0x11000..0x35ACC)

| Range | What | Rebuild? |
|---|---|---|
| 0x11000..0x24E2F | EvoX itself: FTP/telnet commands, network setup, SNTP clock, skins, trainers, BIOS/HDD backup and flash, launching, `q:\evox.ini`; the dashboard object (its `Create` 0x1D920 is the sample framework's `CXBApplication::Create` with EvoX's set-up folded in). `main` = 0x24220 (to 0x24DA6) | yes, as C/C++ |
| 0x24E30..0x2628F | EvoX's menu/list class (0x24E30.., `GetItem` 0x24E90), its embedded-resource class (0x25200..0x256B0, XPR at 0x83750), font class (0x18 bytes: 0x25BB0, `Create` 0x25C80) and the framework's packed-resource loader (0x26110..0x26280, XPR0 check 0x26180) | yes |
| 0x26290..0x26488 | a socket wrapper class over XNet | yes |
| 0x26490..0x26A31 | the sample framework's `xbinput.cpp` (`XBInput_CreateGamepads`, `XBInput_RefreshDeviceList`, `XBInput_GetInput`) | rebuilt: `src/xbinput.cpp` |
| 0x26A40..0x35ACC | C runtime (LIBCMT), XAPI (title meta, `t:\$u`, partitions; XapiTitleStartup at the entry), XKBDD keyboard driver, CRT math/printf | no: RXDK libc / libxapi / xkbd |

`docs/functions.md` (`tools/funcmap.py`) lists EvoX's 237 functions (81.5 KB): size, callers,
callees and the strings each one uses.

Start-up: entry 0x29F53 (XAPI) creates the main thread at 0x29EDF, which runs the CRT initializers
(0x2C573, 0x27D8F, 0x2C4E6, 0x2C48E) and calls `main` (0x24220).

## Other sections

| Section | Range | What | Rebuild? |
|---|---|---|---|
| D3D | 0x35AE0..0x47F88 | Direct3D 8 | no: libd3d8 |
| D3DX | 0x47FA0..0x64718 | D3DX 8, including IJG libjpeg (texture loading) | no: libd3dx8 |
| XGRPH | 0x64720..0x66865 | XGraphics | no: libxgraphics |
| XNET | 0x66880..0x750D8 | XNet | no: libxnet |
| XPP | 0x750E0..0x7D5A4 | XInput / peripherals | no: libxapi |
| .rdata | 0x7D5C0..0x836DC | kernel thunk, strings, constants | data |
| .data | 0x836E0..0x595898 | 3.3 MB raw / 5.3 MB virtual: two XPR0 textures (0x83750, 0xCF240), strings, globals | data |
| _text | 0x5958A0..0x59641D | 7 functions: in-game reset / ELL support | yes |
| text | 0x596420..0x596919 | ELL loader (`tdata\evox\evox.ell`) | yes |

The XPR at 0x83750 is the one `0x25250` loads (CXBPackedResource on an embedded resource).
