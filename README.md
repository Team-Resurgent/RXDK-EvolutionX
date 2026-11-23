# RXDK EvolutionX

The EvolutionX dashboard (build 3935) rebuilt as source on the open RXDK SDK.

## Status

Early. EvoX's code is being rebuilt function by function from the original; the XBE keeps the
original's header and certificate (title id 0x586F7645 "RemoteX", init flags 0xC, 64 KB stack).
See [docs/layout.md](docs/layout.md) for what is EvoX's own code and what comes from RXDK's
libraries.

Rebuilt so far: `main` (start-up, Q: mapping, evox.ini settings, MS-dashboard hand-off, network
config and start-up), the evox.ini reader/writer (`CIniFile`), the debug log, gamepad input, and
the dashboard object's construction, `Create` (Direct3D, gamepads, servers, `[Misc]` settings),
`Destroy`, `Render` (draw passes, fade of the last title's screen, screenshot, screen saver) and
`Run` (the frame loop with its action and fade state machines). In xemu the frame loop runs and
presents frames; the screen stays black until the draw passes are rebuilt. What `main` calls
but is not rebuilt yet is in `include/evox/pending.h`: stubs in `src/pending.cpp` log
`pending: <name> (<original address>)` and return a neutral result. Source comments give each
function's address in the original, so it can be checked against the disassembly.

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

`EXTRA_CFLAGS=-DEVOX_UART_LOG ./build.sh` also writes the debug log to the COM1 UART, which xemu
shows with `-device lpc47m157 -serial stdio` (or `-serial file:<path>`). `tools/disasm.py <addr>`
disassembles one function of the original with strings and kernel imports named.
