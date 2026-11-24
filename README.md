# RXDK EvolutionX

<p align="center"><b>The EvolutionX dashboard (build 3935) rebuilt as C++ source on the open RXDK SDK</b></p>

<p align="center">
  <a href="https://github.com/Team-Resurgent/RXDK-EvolutionX/actions/workflows/build.yml"><img src="https://github.com/Team-Resurgent/RXDK-EvolutionX/actions/workflows/build.yml/badge.svg?branch=main" alt="Build"></a>
  <a href="https://discord.gg/VcdSfajQGK"><img src="https://img.shields.io/badge/chat-on%20discord-7289da.svg?logo=discord" alt="Discord"></a>
</p>

<p align="center">
  <a href="https://ko-fi.com/J3J7L5UMN"><img src="https://img.shields.io/badge/ko--fi-Support-FF5E5B?style=for-the-badge&logo=ko-fi&logoColor=white" alt="ko-fi"></a>
  <a href="https://www.patreon.com/teamresurgent"><img src="https://img.shields.io/badge/Patreon-F96854?style=for-the-badge&logo=patreon&logoColor=white" alt="Patreon"></a>
</p>

## About

EvolutionX is the classic Xbox dashboard: the skinnable menu with its `[Menu]` in evox.ini, the
Settings / Info screen, launching games and apps with trainers and the in-game reset, BIOS
flashing, Backup, scripted `[Action_NN]` items, and the FTP, Telnet and RDTOOLS debug servers.
This project reconstructs build 3935 function by function as C++, and builds it on the RXDK SDK
libraries (D3D8, D3DX8, XGraphics, XNet, XAPI, the C runtime) instead of the XDK ones the original
carried. EvoX's own code is all rebuilt.

EvoX carries its built-in graphics (two XPR texture packs and the XBG logo mesh), its in-game
reset loader and TSRs inside its XBE. Those are not part of this repository: the build takes them
from your own `evoxdash.xbe` and links them in unchanged (see [Build](#build)). Nothing else of the
original is linked in.

In xemu the start screen matches the original's pixel for pixel outside the turning logo, every
skin variable reads the same, and the FTP, Telnet and debug servers give the same transcripts.

## Changes from the original

None in behaviour are intended. How it is built differs:

- **Not packed:** the original `evoxdash.xbe` is a loader stub around a raw-DEFLATE XBE; the
  rebuilt XBE is plain.
- **Crash guards:** EvoX's 31 `catch (...)` blocks (around evox.ini's lock, the servers' threads,
  skin loading, start-up) caught crashes as well as C++ exceptions with the original's compiler,
  so a fault there was logged or skipped and the dashboard kept running. RXDK's clang has no
  `__try` / `__except` on its target, so `include/evox/guard.h` registers an exception frame of its
  own for each of them; `tests/crash.txt` checks it by faulting on purpose.
- **XNet link status:** the SDK's `XNetGetEthernetLinkStatus` can report speed bits without a link;
  EvoX's checks see it as the XDK reported it (`EthernetLinkStatus` in `include/evox/netcfg.h`).

## Requirements

Windows, Linux or macOS, with:

- **RXDK** (clang, the SDK, `Rxdk.Cli` and `imagebld`) where RXDK installs itself:

  | OS | RXDK root |
  |---|---|
  | Windows | `%ProgramData%\RXDK` |
  | macOS | `~/Library/Application Support/RXDK` |
  | Linux | `$XDG_DATA_HOME/rxdk` (`~/.local/share/rxdk`) |

  Set `RXDK` to use another root. The RXDK VS Code / Visual Studio setup installs it, or run
  `pwsh scripts/stage-rxdk.ps1`, which downloads and lays it out as CI does.
- **Bash** (Git Bash on Windows) and **Python 3** (standard library only)
- **Your own `evoxdash.xbe`** (EvolutionX build 3935) in the project root, for the graphics, the
  in-game reset loader and the TSRs

## Build

```bash
./build.sh
```

| Output | What |
|---|---|
| `out/evoxdash.xbe` | EvolutionX, with the original's title id (0x586F7645 "RemoteX"), certificate, init flags and 64 KB stack |

`tools/extract_assets.py` takes the assets from your `evoxdash.xbe` at build time (checked by
SHA-256, so only build 3935 is accepted) and the build links them in unchanged; nothing extracted
is committed. `EVOX_XBE=<path>` points at the original elsewhere. Without it the XBE still builds,
with a warning, but has no graphics and no in-game reset.

Every push to `main` builds on GitHub Actions (Windows, Linux and macOS), without the assets, and
uploads that XBE as a build artifact; no release is published, since it would have no graphics.
`RX_OUT=<dir>` builds somewhere else; `EXTRA_CFLAGS` adds defines, e.g.
`EXTRA_CFLAGS=-DEVOX_UART_LOG ./build.sh`; `PYTHON` picks the Python 3.

| Switch | Does |
|---|---|
| `EVOX_UART_LOG` | the debug log also goes to the COM1 UART (xemu `-device lpc47m157 -serial stdio`) |
| `EVOX_TEST_SCRIPT="../work/test/<x>.h"` | a button test (see [Tests](#tests)) |
| `EVOX_TEST_INI` | load and save `e:\evox.ini` instead of `q:\evox.ini` |
| `EVOX_TEST_ACTION=n`, `EVOX_TEST_FRAME=f` | run menu action `n` at frame `f` |

## Tests

The tests run in xemu. Use a throwaway hard disk (xemu's `-snapshot`): EvoX writes its settings
and logs.

**Button tests** press controller buttons at given frames of the frame loop, so a screen can be
driven without a pad. `tools/gen_test_script.py` turns `tests/<x>.txt` into a header and lists the
script commands; the log lines start with `test`.

```bash
python tools/gen_test_script.py tests/settings.txt work/test/settings.h
EXTRA_CFLAGS='-DEVOX_UART_LOG -DEVOX_TEST_SCRIPT="../work/test/settings.h" -DEVOX_TEST_INI' ./build.sh
```

| Test | Does |
|---|---|
| `tests/settings.txt` | opens Settings, sets Setup Network with the Yes / No keyboard, types an FTP password, saves, prints evox.ini |
| `tests/menus.txt` | logs every menu (main with its submenus, BIOS, skins, trainers) |
| `tests/trainer.txt` | opens a trainer's options screen |
| `tests/actions.txt` | runs a scripted action (a copy, with its log) |
| `tests/backup_igr.txt` | runs Backup and installs the in-game reset (without the reboot); `tools/check_backup_igr.py` checks the files against xemu's flash, EEPROM and HDD images |
| `tests/drive.txt` | opens Settings at the Security line; test builds log IDENTIFY, SMART and the drive's lock password |
| `tests/crash.txt` | faults on purpose inside crash guards |

The menu, trainer and action tests run from a folder made by `tools/make_menu_test.py`: an
evox.ini with a `[Menu]` using each keyword, BIOS files, skins, a trainer and game folders. xemu's
`-xbe-path` serves that folder as Q:, so the original can run from a copy of it for comparison.
`tools/make_info_test.py` makes a skin that shows every skin variable.

```bash
python tools/gen_test_script.py tests/menus.txt work/test/menus.h
EXTRA_CFLAGS='-DEVOX_UART_LOG -DEVOX_TEST_SCRIPT="../work/test/menus.h"' RX_OUT=out/test ./build.sh
python tools/make_menu_test.py work/menutest/rebuild out/test/evoxdash.xbe out/evoxdash.xbe
python tools/make_menu_test.py work/menutest/orig evoxdash.xbe out/evoxdash.xbe
```

**Network tests:** `tools/server_test.py ftp|telnet|debug` runs a session against the servers and
prints the transcript. xemu needs NAT with forwards 2121->21, 2323->23 and 7310->731, and a hard
disk without `xbdm.dll` (xbdm otherwise takes the first DHCP address, which the forwards go to):
use a copy of the HDD image with the file renamed, never the original.

**Never** put configsector, format, flashbios or hddlock* steps in a test action, script the
Flash BIOS box, or send the servers FLASHBIOS, EXECUTE, FORMATPATH, FORMATDRIVE, the drive lock
commands, memory or port writes, or an FTP LIST of `/` (which validates F: and can format it).

`tools/xemu_shot.py out.png` saves xemu's framebuffer as PNG (start xemu with
`-monitor tcp:127.0.0.1:45454,server,nowait`).

## Working from the original

With `evoxdash.xbe` in the project root:

```bash
python tools/unpack_evox.py evoxdash.xbe work/evoxdash_unpacked.xbe
python tools/xbe2pe.py work/evoxdash_unpacked.xbe work/evoxdash.exe  # PE for Ghidra / REA
```

`tools/unpack_evox.py` inflates the inner XBE. `tools/xbe2pe.py` keeps every section at its
original address and turns the kernel thunk table into `xboxkrnl.exe` imports by ordinal.
`tools/disasm.py <addr>` disassembles one function of the original with strings and kernel
imports named. [docs/layout.md](docs/layout.md) maps what is EvoX's own code and what came from
the XDK libraries; [docs/functions.md](docs/functions.md) lists EvoX's functions.

## Source layout

| Files | What |
|---|---|
| `src/main.cpp`, `launch.cpp`, `globals.cpp` | start-up, evox.ini settings, network start-up, leaving the dashboard |
| `src/dash.cpp`, `dash_run.cpp`, `dash_draw.cpp` | the dashboard object, the frame loop with its action and fade states, drawing |
| `src/dash_skin.cpp`, `skinvars.cpp`, `dash_menus.cpp`, `menu.cpp` | skins and their text labels and variables, the built-in menu, folder scans and `[Menu]` |
| `src/settings.cpp`, `trainer.cpp`, `actions.cpp`, `flash.cpp`, `backup.cpp` | the Settings / Info screen and keyboards, trainers, `[Action_NN]`, the Flash BIOS box and writer, Backup |
| `src/info.cpp`, `clock.cpp`, `cdaudio.cpp`, `chameleon.cpp`, `devices.cpp` | the live information, the SNTP clock, audio CDs, the Chameleon LED, the DVD remote and USB keyboard |
| `src/hdd.cpp`, `igr.cpp` | drive letters, IDENTIFY, SMART and the lock password; the in-game reset install |
| `src/ftp.cpp`, `ftp_session.cpp`, `telnet.cpp`, `telnet_cmds.cpp`, `debugserver.cpp`, `socket.cpp`, `netcfg.cpp` | the FTP, Telnet and RDTOOLS debug servers, sockets, the network config |
| `src/inifile.cpp`, `log.cpp`, `md5.cpp`, `guard.cpp` | evox.ini reader / writer, the debug log, MD5, the crash guard |
| `src/xbfont.cpp`, `xbmesh.cpp`, `xbresource.cpp`, `xbinput.cpp`, `xbutil.cpp` | the XDK sample-framework classes EvoX built in |
| `src/testscript.cpp` | test builds only: the button tests |
| `include/evox` | one header per module |
| `tools` | asset extraction, unpacking, test helpers |

The comment on each function and global gives its address in the original 3935 image.

## Credits

EvolutionX is the work of Team EvoX. This project only reconstructs build 3935 to build it on an
open SDK.
