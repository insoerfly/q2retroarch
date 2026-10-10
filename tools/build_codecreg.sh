#!/bin/sh
# Build the static ARM <codecreg> diagnostic for the Q2 / Allwinner A13 target.
# Run inside WSL. Output: tools/codecreg (static, stripped, ARM ELF).
set -e
B=/home/agent/r3x/lakka/build.Lakka-A13_4C.arm-3.7.5-devel
TC=$B/toolchain/bin/armv7a-libreelec-linux-gnueabi-gcc
STRIP=$B/toolchain/bin/armv7a-libreelec-linux-gnueabi-strip
HERE=$(cd "$(dirname "$0")" && pwd)
"$TC" -O2 -static -o "$HERE/codecreg" "$HERE/codecreg.c"
"$STRIP" "$HERE/codecreg"
file "$HERE/codecreg"
echo "OK: $HERE/codecreg"
