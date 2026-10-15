#!/bin/bash
# Build the ported XMB launcher for the Q2/A13 (ARM / Lakka toolchain + FreeType).
#
#   Run from anywhere:  bash launcher/port/build.sh
# Produces launcher/port/launcher.arm (+ .stripped).
#
# Offline UI preview (renders /tmp/launcher.ppm, no device needed):
#   gcc -O2 -DHOST_TEST -DROMDIR='"/tmp/lroms"' -I/usr/include/freetype2 \
#       -o /tmp/ltest launcher.c -lfreetype && /tmp/ltest
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/launcher.arm"

TC=${TC:-/home/agent/r3x/lakka/build.Lakka-A13_4C.arm-3.7.5-devel/toolchain}
CC=$TC/bin/armv7a-libreelec-linux-gnueabi-gcc
SR=$TC/armv7a-libreelec-linux-gnueabi/sysroot

"$CC" -O2 -Wall -march=armv7-a -mfpu=neon -mfloat-abi=hard \
      -I"$SR/usr/include/freetype2" -I"$SR/usr/include" \
      -o "$OUT" "$HERE/launcher.c" \
      -L"$SR/usr/lib" -lfreetype -lm

"$TC/bin/armv7a-libreelec-linux-gnueabi-strip" -o "$OUT.stripped" "$OUT" 2>/dev/null || cp -f "$OUT" "$OUT.stripped"
ls -l "$OUT" "$OUT.stripped"
echo "OK"
