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
menu class, the built-in main menu and the menu screen with its input, the menu actions (launching
an XBE, reboot, power off), the settings screen with its keyboards, the drive letters, the BIOS,
skin and trainer folder scans, and the `[Menu]` parser with its game folder scan (`AutoAddItem`),
and the per-frame tasks: the live information for the skin variables and the settings screen
(clock text, free space, network, BIOS and kernel versions, temperatures, Chameleon), the DVD tray
and disc check, memory units and the SNTP clock, Backup, the in-game reset install (its loader and
TSRs are taken from `evoxdash.xbe` like the graphics), and the three network servers: FTP (port
21), Telnet (23) and the RDTOOLS debug server (731, xbdm-style), the trainer screens and Evox.etf,
the scripted actions (`[Action_NN]`: the "Continue ?" dialog and its steps), the "Flash BIOS" box
and the flash writer, and the DVD remote and USB keyboard. In xemu the start screen (background,
logo, selection bar and main menu) matches the original's pixel for pixel outside the turning
logo, with the built-in menu and with a `[Menu]`, and every skin variable reads the same as in the
original (`tools/make_info_test.py` makes a skin that shows them all). What is called but not
rebuilt yet (the drive identify, ATA and SMART code) is in `include/evox/pending.h` and the stub
classes in `include/evox/dash.h`: stubs in `src/pending.cpp` log `pending: <name> (<original
address>)` and return a neutral result. Source comments give each function's address in the
original, so it can be checked against the disassembly.

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

## Button tests

A test script presses controller buttons at given frames of the frame loop, so a screen can be
driven in xemu without a pad. `tests/settings.txt` opens Settings, sets Setup Network to Yes with
the Yes/No keyboard, types a new FTP password on the text keyboard, then picks Save And Exit and
prints the saved `evox.ini` to the log:

```bash
python tools/gen_test_script.py tests/settings.txt work/test/settings.h
EXTRA_CFLAGS='-DEVOX_UART_LOG -DEVOX_TEST_SCRIPT="../work/test/settings.h" -DEVOX_TEST_INI' ./build.sh
```

`tests/menus.txt` logs every menu (main with its submenus, BIOS, skins, trainers). Run it from a
folder made by `tools/make_menu_test.py`, whose evox.ini has a `[Menu]` using each keyword, with
BIOS files, skins, a trainer and game folders; xemu's `-xbe-path` serves that folder as Q:, so the
original can be run from a copy of it for comparison:

```bash
python tools/gen_test_script.py tests/menus.txt work/test/menus.h
EXTRA_CFLAGS='-DEVOX_UART_LOG -DEVOX_TEST_SCRIPT="../work/test/menus.h"' RX_OUT=out/test ./build.sh
python tools/make_menu_test.py work/menutest/rebuild out/test/evoxdash.xbe out/evoxdash.xbe
python tools/make_menu_test.py work/menutest/orig evoxdash.xbe out/evoxdash.xbe
```

`tests/backup_igr.txt` runs Backup and installs the in-game reset (without the reboot), logging
the files' hashes; `tools/check_backup_igr.py` compares them with xemu's flash, EEPROM and HDD
images and with its own decoding of the TSR.

`tests/trainer.txt` opens a trainer's options screen, and `tests/actions.txt` runs a scripted
action (a copy, with its log), both from the folder `tools/make_menu_test.py` makes. Never put
configsector, format, flashbios or hddlock* steps in a test action, and never script the Flash
BIOS box.

`tools/gen_test_script.py` lists the script commands. `EVOX_TEST_INI` loads and saves
`e:\evox.ini` in place of `q:\evox.ini`, so run it on a throwaway HDD (xemu's `-snapshot`). The
log lines start with `test`.

## Network tests

`tools/server_test.py ftp|telnet|debug` runs a session against the servers in xemu and prints the
transcript; the same session against the original and the rebuild gives identical transcripts
(FTP up to the file times). It uses only commands that read, and files under `E:\evoxtest`; it
never sends FLASHBIOS, EXECUTE, FORMATPATH, FORMATDRIVE, LIST of "/" (which validates F: and can
format it), port or memory writes, or the drive lock commands.

xemu needs NAT with forwards 2121->21, 2323->23 and 7310->731, and a hard disk without
`xbdm.dll`: xbdm otherwise takes the first DHCP address, which the forwards go to. Use a copy of
the HDD image with the file renamed, never the original.
