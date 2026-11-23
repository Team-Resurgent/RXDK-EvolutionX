#!/usr/bin/env bash
# The build: EvolutionX from this project's sources on the RXDK SDK.
#   compile:  clang with the SDK's own flags (Rxdk.Cli compile-args), C and C++
#   link:     Rxdk.Cli link-exe (XAPI start-up -> main)
#   XBE:      imagebld, with the original's title id, certificate fields and init flags
# usage: ./build.sh   (RXDK: RXDK root; RX_OUT: output dir, default out; EXTRA_CFLAGS: extra flags)
# Runs on Windows (Git Bash / MSYS2), Linux and macOS, with RXDK where RXDK itself puts it.
set -euo pipefail
cd "$(dirname "$0")"

case "$(uname -s)" in
MINGW* | MSYS* | CYGWIN*) os=windows EXE=.exe DEFAULT_ROOT="${ProgramData:-C:/ProgramData}/RXDK" ;;
Darwin) os=macos EXE= DEFAULT_ROOT="$HOME/Library/Application Support/RXDK" ;;
*) os=linux EXE= DEFAULT_ROOT="${XDG_DATA_HOME:-$HOME/.local/share}/rxdk" ;;
esac
case "$(uname -m)" in
arm64 | aarch64) arch=arm64 ;;
*) arch=x64 ;;
esac
RXDK=${RXDK:-$DEFAULT_ROOT}
RXDK=${RXDK//\\//}
CL="$RXDK/llvm/xbox-$os-$arch/bin/clang$EXE"
CLI="$RXDK/tools/Rxdk.Cli$EXE"
IMAGEBLD="$RXDK/tools/imagebld$EXE"
for t in "$CL" "$CLI" "$IMAGEBLD"; do
    [ -x "$t" ] || { echo "missing $t (RXDK root: $RXDK; set RXDK)"; exit 1; }
done

OUT=${RX_OUT:-out}
mkdir -p "$OUT/obj"
# no stale outputs: a failed step must not leave the previous build looking current
rm -f "$OUT/evoxdash.exe" "$OUT/evoxdash.xbe"

# the SDK's compile flags (a read loop, not mapfile: macOS ships bash 3.2)
CFLAGS_C=() CFLAGS_CXX=()
while IFS= read -r f; do CFLAGS_C+=("$f"); done < <("$CLI" compile-args --lang c | tr -d '\r')
while IFS= read -r f; do CFLAGS_CXX+=("$f"); done < <("$CLI" compile-args --lang c++ | tr -d '\r')
OURS="${EXTRA_CFLAGS:-} -O2 -Wall -Wno-pragma-pack -Iinclude" # (the SDK headers trip -Wpragma-pack)

OBJS=()
while IFS= read -r src; do
    obj="$OUT/obj/${src%.*}.o"
    mkdir -p "$(dirname "$obj")"
    case "$src" in
    *.c) "$CL" "${CFLAGS_C[@]}" $OURS -c "$src" -o "$obj" ;;
    *.cpp) "$CL" "${CFLAGS_CXX[@]}" $OURS -c "$src" -o "$obj" ;;
    esac
    OBJS+=("$obj")
done < <(find src -name '*.c' -o -name '*.cpp' | LC_ALL=C sort)

MSYS_NO_PATHCONV=1 "$CLI" link-exe -o "$OUT/evoxdash.exe" "${OBJS[@]}" \
    libxgraphics.lib libd3dx8.lib libd3d8.lib libxnet.lib libxapi.lib libcpp.lib libc.lib libcompat.lib libkernel.lib \
    | grep -v '^\$ ' || true
[ -f "$OUT/evoxdash.exe" ] || { echo "link failed"; exit 1; }

# the original's certificate and header: title id 0x586F7645 "RemoteX", all media, regions
# NA/JP/rest + manufacturing, version 0, 64 KB stack, init flags 0xC (limit memory to 64 MB,
# don't set up the hard disk; what XAPI read from the unpacked header in the original)
MSYS_NO_PATHCONV=1 "$IMAGEBLD" /IN:"$OUT/evoxdash.exe" /OUT:"$OUT/evoxdash.xbe" \
    /STACK:65536 /INITFLAGS:0xC /TESTID:0x586F7645 /TESTNAME:"RemoteX" \
    /TESTMEDIATYPES:0x800000FF /TESTREGION:0x80000007 /TESTRATINGS:0xFFFFFFFF /TESTVERSION:0 /NOLIBWARN
[ -f "$OUT/evoxdash.xbe" ] || { echo "imagebld failed"; exit 1; }
echo "built $OUT/evoxdash.xbe"
