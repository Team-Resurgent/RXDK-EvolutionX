# EvolutionX 3935 layout (from `work/evoxdash_unpacked.xbe`)

`evoxdash.xbe` is packed: a loader stub around the real XBE, stored as raw DEFLATE (descriptor at
0x12C78, stream at 0x12C84). `tools/unpack_evox.py` inflates it. Its header stores addresses plain (not XOR-scrambled).
`tools/xbe2pe.py` wraps it in a PE (same addresses, kernel imports as `xboxkrnl.exe` ordinals) for
Ghidra: `work/evoxdash.exe`. `rea decompile-all` wrote Ghidra pseudocode for every function to
`rea/decomp/functions/<addr>.c` (1816 written, 1 failure in XNet, 113 imports).

Header: XDK 5455, title id 586F7645 "RemoteX", init flags 0xC, entry 0x29F53, kernel thunk
0x7D5C0 (139 imports).

## .text (0x11000..0x35ACC)

| Range | What | Rebuild? |
|---|---|---|
| 0x11000..0x24E2F | EvoX itself: FTP/telnet commands, network setup, SNTP clock, skins, trainers, BIOS/HDD backup and flash, launching, `q:\evox.ini`; the dashboard object (its `Create` 0x1D920 is the sample framework's `CXBApplication::Create` with EvoX's set-up folded in). `main` = 0x24220 (to 0x24DA6) | yes, as C/C++ |
| 0x24E30..0x251FF | EvoX's menu/list class (0x24E30.., `GetItem` 0x24E90) | yes |
| 0x25200..0x2628F | sample framework, loading from memory: `CXBFont` (0x25200..0x25B9E, font XPR 0x83750), `CXBMesh` (0x25BB0..0x260BB, XBG 0x33D240), `CXBPackedResource` (0x260C0..0x26280) | rebuilt: `src/xbfont.cpp`, `src/xbmesh.cpp`, `src/xbresource.cpp` |
| 0x26290..0x26488 | a socket wrapper class over XNet | yes |
| 0x26490..0x26A31 | the sample framework's `xbinput.cpp` (`XBInput_CreateGamepads`, `XBInput_RefreshDeviceList`, `XBInput_GetInput`) | rebuilt: `src/xbinput.cpp` |
| 0x26A40..0x26A80 | `XBUtil_InitMaterial` (xbutil.cpp) | rebuilt: `src/xbutil.cpp` |
| 0x26A81..0x35ACC | C runtime (LIBCMT), XAPI (title meta, `t:\$u`, partitions; XapiTitleStartup at the entry), XKBDD keyboard driver, CRT math/printf | no: RXDK libc / libxapi / xkbd |

`docs/functions.md` (`tools/funcmap.py`) lists EvoX's 238 functions (81.6 KB): size, callers,
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
| .data | 0x836E0..0x595898 | 3.3 MB raw / 5.3 MB virtual: strings, globals and the assets below | data |
| _text | 0x5958A0..0x59641D | in-game reset / ELL support; the build number 3935 (0x595963); the audio CD reader from 0x59617F (`src/cdaudio.cpp`) | yes |
| text | 0x596420..0x596919 | the code filling in the IGR loader's kernel addresses (rebuilt in `src/igr.cpp`), then the loader itself (`g_IgrLoader`, from 0x596473) | asset |


## Assets

Linked from the user's `evoxdash.xbe` at build time (`tools/extract_assets.py`), not committed:

| Symbol | Address | Size | What |
|---|---|---|---|
| `g_FontXpr` | 0x83750 | 0x11000 | XPR: font texture + glyph table (CXBFont, `0x25250`) |
| `g_ResourceXpr` | 0xCF240 | 0x26E000 | XPR, 4 resources; tags `xray1.bmp`, `xray2.bmp` at 0x94D70 (CXBPackedResource, `0x26180`) |
| `g_LogoMesh` | 0x33D240 | 0x96F00 | XBG mesh `XBG\x02`, 1 frame (CXBMesh, `0x25C80`) |
| `g_Tsr1` | 0x95D68 | 0x737C | in-game reset TSR for `TSR_Type` 1: a 16-byte header with two part sizes, then the parts, encoded |
| `g_Tsr2` | 0x9D0E8 | 0x31900 | in-game reset TSR for `TSR_Type` 2, the same way |
| `g_IgrLoader` | 0x596473 (`text`) | 0x4A2 | the loader the patched kernel calls, built to run at 0x80000010 |

IgrInstall (`src/igr.cpp`) decodes the chosen TSR into `E:\TDATA\EVOX\evox.ell`, and copies the
loader to 0x80000010 with its game region, flag and eight kernel addresses filled in.
