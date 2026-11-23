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
`Run` (the frame loop with its action and fade state machines), the sample-framework classes EvoX
built in (font, XBG mesh, packed resources), the drawing helpers, the skin's text labels and
variables, the logo, skin loading (`[Skin_<name>]` in evox.ini, else the built-in pictures), the
menu class, the built-in main menu and the menu screen with its input. In xemu the start screen
(background, logo, selection bar and main menu) matches the original's pixel for pixel outside
the turning logo. What is called but not rebuilt yet (the settings and other screens, the
servers, the folder scans, the `[Menu]` parser, launching) is in `include/evox/pending.h` and
the stub classes in `include/evox/dash.h`: stubs in `src/pending.cpp` log
`pending: <name> (<original address>)` and return a neutral result. Source comments give each
function's address in the original, so it can be checked against the disassembly.

The rebuild links RXDK's D3D8, D3DX8, XGraphics, XNet, XAPI and C runtime in place of the XDK ones
that the original carried. The output XBE is not packed.

## Requirements

- **RXDK** (clang, the SDK, `Rxdk.Cli` and `imagebld`) in its default root
  (`%ProgramData%\RXDK` on Windows, `~/Library/Application Support/RXDK` on macOS,
  `~/.local/share/rxdk` on Linux), or set `RXDK`.
- **Bash** (Git Bash on Windows) and **Python 3** (standard library only).
- **Your own `evoxdash.xbe`** (EvolutionX build 3935) in the project root, for EvoX's built-in
  graphics. It is not part of this repository.

## Build

```bash
./build.sh
```

Writes `out/evoxdash.xbe`. EvoX carries its graphics inside its XBE: two XPR texture packs and
the XBG logo mesh. `tools/extract_assets.py` takes them from your `evoxdash.xbe` at build time
(checked by SHA-256, so only build 3935 is accepted) and the build links them in unchanged; nothing
extracted is committed. `EVOX_XBE=<path>` points at the original elsewhere. Without it the XBE
still builds, with a warning, and has no graphics.

`RX_OUT=<dir>` builds somewhere else; `EXTRA_CFLAGS` adds flags; `PYTHON` picks the Python 3.

## Working from the original

With `evoxdash.xbe` (build 3935) in the project root:

```bash
python tools/unpack_evox.py evoxdash.xbe work/evoxdash_unpacked.xbe
python tools/xbe2pe.py work/evoxdash_unpacked.xbe work/evoxdash.exe  # PE for Ghidra / REA
```

`evoxdash.xbe` is a loader stub around the real XBE, stored as raw DEFLATE; `tools/unpack_evox.py`
inflates it. `tools/xbe2pe.py` keeps every section at its original address and turns the kernel thunk
table into `xboxkrnl.exe` imports by ordinal. `rea decompile-all work/evoxdash.exe --output
rea/decomp --provider ghidra` then writes Ghidra pseudocode for every function.

`tools/xemu_shot.py out.png` saves xemu's framebuffer as PNG (start xemu with
`-monitor tcp:127.0.0.1:45454,server,nowait`).

`EXTRA_CFLAGS=-DEVOX_UART_LOG ./build.sh` also writes the debug log to the COM1 UART, which xemu
shows with `-device lpc47m157 -serial stdio` (or `-serial file:<path>`). `tools/disasm.py <addr>`
disassembles one function of the original with strings and kernel imports named.
