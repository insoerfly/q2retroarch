#!/bin/bash
B=/home/agent/r3x/lakka/build.Lakka-A13_4C.arm-3.7.5-devel
TC=$B/toolchain
CROSS=$TC/bin/armv7a-libreelec-linux-gnueabi-
OUT=/mnt/c/distr/soft/r3xclone/lakka-a13/out/cores_rebuilt
LOG=/root/cores_rebuild.log
rm -rf "$OUT"; mkdir -p "$OUT"
exec > "$LOG" 2>&1
echo "START $(date)"
list="fceumm-159f27a:fceumm_libretro.so
snes9x2010-e86e546:snes9x2010_libretro.so
snes9x2005-fd45b0e:snes9x2005_libretro.so
snes9x2002-540baad:snes9x2002_libretro.so
gambatte-ca0f7e1:gambatte_libretro.so
genesis-plus-gx-a2380d8:genesis_plus_gx_libretro.so
picodrive-80d31d7:picodrive_libretro.so
nestopia-cb1e24e:nestopia_libretro.so
prosystem-1924a37:prosystem_libretro.so
mgba-a69c343:mgba_libretro.so
handy-7c2dbcb:handy_libretro.so
vecx-33a8a89:vecx_libretro.so
gme-635b1e9:gme_libretro.so"
for item in $list; do
  d="${item%%:*}"; so="${item##*:}"
  [ -d "$B/$d" ] || { echo "SKIP $d (no dir)"; continue; }
  cd "$B/$d"
  mk="Makefile.libretro"; [ -f "$mk" ] || mk="Makefile"
  make -f "$mk" clean >/dev/null 2>&1
  make -f "$mk" CC="$CROSS""gcc" CXX="$CROSS""g++" AR="$CROSS""ar" -j"$(nproc)" >"/tmp/core_$d.log" 2>&1
  rc=$?
  f=$(find . -maxdepth 3 -name "$so" 2>/dev/null | head -1)
  flag=$(grep -m1 -oE "\-O[0-3s]" "/tmp/core_$d.log" 2>/dev/null)
  if [ $rc -eq 0 ] && [ -n "$f" ]; then
     cp -f "$f" "$OUT/$so"
     echo "OK  $d rc=$rc opt=$flag md5=$(md5sum "$OUT/$so" | cut -c1-8)"
  else
     echo "FAIL $d rc=$rc file=$f"
     tail -3 "/tmp/core_$d.log"
  fi
done
echo "DONE $(date)"
ls -la "$OUT"
