# RXDK EvolutionX

The EvolutionX dashboard (build 3935) rebuilt as source on the open RXDK SDK.

## Status

Early. The project builds a placeholder XBE with the original's header and certificate (title id
0x586F7645 "RemoteX", init flags 0xC, 64 KB stack); EvoX's code is being rebuilt function by
function from the original. See [docs/layout.md](docs/layout.md) for what is EvoX's own code and
what comes from RXDK's libraries.

The rebuild links RXDK's D3D8, D3DX8, XGraphics, XNet, XAPI and C runtime in place of the XDK ones
that the original carried. The output XBE is not packed.

## Requirements

- **RXDK** (clang, the SDK, `Rxdk.Cli` and `imagebld`) in its default root
  (`%ProgramData%\RXDK` on Windows, `~/Library/Application Support/RXDK` on macOS,
  `~/.local/share/rxdk` on Linux), or set `RXDK`.
- **Bash** (Git Bash on Windows).

## Build

```bash
./build.sh
```

Writes `out/evoxdash.xbe`. `RX_OUT=<dir>` builds somewhere else; `EXTRA_CFLAGS` adds flags.

## Working from the original

The original binary is not part of this repository. With `evoxdash.xbe` (build 3935) in the
project root:

```bash
python tools/unpack_evox.py evoxdash.xbe work/evoxdash_unpacked.xbe   # needs unicorn
python tools/xbe2pe.py work/evoxdash_unpacked.xbe work/evoxdash.exe  # PE for Ghidra / REA
```

`tools/unpack_evox.py` runs the original's own decompression stub in an emulator to get the inner
XBE. `tools/xbe2pe.py` keeps every section at its original address and turns the kernel thunk
table into `xboxkrnl.exe` imports by ordinal. `rea decompile-all work/evoxdash.exe --output
rea/decomp --provider ghidra` then writes Ghidra pseudocode for every function.

`tools/xemu_shot.py out.png` saves xemu's framebuffer as PNG (start xemu with
`-monitor tcp:127.0.0.1:45454,server,nowait`).
